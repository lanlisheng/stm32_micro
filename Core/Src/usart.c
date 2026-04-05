/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    usart.c
 * @brief   This file provides code for the configuration
 *          of the USART instances.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */
#include "rtt_log.h"
#include <string.h>

#define USART1_TX_QUEUE_SIZE 512U

/*
 * USART1 RX/TX 运行状�?�及性能统计变量�?
 *
 * 环形缓冲区采用循�? FIFO 方式管理�?
 * - usart1_rx_byte：保存中断接收的�?新一字节�?
 * - usart1_rx_buffer：USART1 接收数据的环形缓冲区�?
 * - usart1_rx_head：环形缓冲区的下�?个写入位置�??
 * - usart1_rx_tail：环形缓冲区的下�?个读取位置�??
 * - usart1_rx_overflow：缓冲区满时丢弃数据并置位该标志�?
 *
 * 性能统计累加器用于记�? RX/TX �? CPU 周期、字节数和调用次数，
 * 便于计算和输�? CPU 占用情况�?
 */
static uint8_t usart1_rx_byte;
static uint8_t usart1_rx_buffer[USART1_RX_BUFFER_SIZE];
static volatile uint16_t usart1_rx_head;
static volatile uint16_t usart1_rx_tail;
static volatile uint8_t usart1_rx_overflow;
static volatile uint32_t usart1_rx_cycles_acc;
static volatile uint32_t usart1_tx_cycles_acc;
static volatile uint32_t usart1_rx_bytes_acc;
static volatile uint32_t usart1_tx_bytes_acc;
static volatile uint32_t usart1_rx_interrupt_count_acc;
static volatile uint32_t usart1_tx_call_count_acc;
static uint8_t usart1_tx_queue[USART1_TX_QUEUE_SIZE];
static volatile uint16_t usart1_tx_head;
static volatile uint16_t usart1_tx_tail;
static volatile uint16_t usart1_tx_active_size;
static uint32_t usart1_cpu_monitor_tick;
static USART1_CpuStats_t usart1_cpu_stats;

/*
 * 读取当前 DWT 周期计数器�??
 * 该计数器随每�? CPU 时钟周期递增，用�? USART TX/RX 性能统计�?
 */
static uint32_t USART1_GetCycleCount(void) { return DWT->CYCCNT; }

/*
 * 启用 DWT 周期计数器并重置计数值�??
 * 必须先�?�过 CoreDebug 使能 DWT 单元，计数器才能运行�?
 */
static void USART1_EnableCycleCounter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint16_t USART1_TxQueueAvailable(void)
{
    uint16_t head = usart1_tx_head;
    uint16_t tail = usart1_tx_tail;
    return (uint16_t)((tail + USART1_TX_QUEUE_SIZE - head - 1U) % USART1_TX_QUEUE_SIZE);
}

static uint16_t USART1_TxQueueCount(void)
{
    uint16_t head = usart1_tx_head;
    uint16_t tail = usart1_tx_tail;
    return (uint16_t)((head + USART1_TX_QUEUE_SIZE - tail) % USART1_TX_QUEUE_SIZE);
}

static void USART1_StartTxFromQueue(void)
{
    if (huart1.gState != HAL_UART_STATE_READY)
    {
        return;
    }

    uint16_t count = USART1_TxQueueCount();
    if (count == 0U)
    {
        return;
    }

    uint16_t contiguous = (usart1_tx_tail < usart1_tx_head)
                              ? (uint16_t)(usart1_tx_head - usart1_tx_tail)
                              : (uint16_t)(USART1_TX_QUEUE_SIZE - usart1_tx_tail);
    usart1_tx_active_size = contiguous;
    (void)HAL_UART_Transmit_IT(&huart1, &usart1_tx_queue[usart1_tx_tail], contiguous);
}

/*
 * 将经过的时间窗口（毫秒）转换为等效的 CPU 周期数，
 * 使用当前 HCLK 频率计算�?
 * 该�?�用于计�? USART 活动�? CPU 使用率�??
 */
static uint32_t USART1_GetWindowCycles(uint32_t elapsed_ms)
{
    uint64_t cpu_hz = (uint64_t)HAL_RCC_GetHCLKFreq();

    return (uint32_t)((cpu_hz * elapsed_ms) / 1000ULL);
}

/*
 * 通过 USART1 发�?�一段字节数据�??
 * 如果启用 profile_tx，则会测量本次发送�?�时并累加到性能统计中�??
 */
static HAL_StatusTypeDef USART1_TransmitBytes(const uint8_t *data,
                                              uint16_t length,
                                              uint8_t profile_tx)
{
    uint32_t start_cycles;
    uint32_t primask;
    uint16_t i;
    uint16_t free_space;

    if ((data == NULL) || (length == 0U))
    {
        return HAL_ERROR;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    free_space = USART1_TxQueueAvailable();
    if (length > free_space)
    {
        __set_PRIMASK(primask);
        return HAL_BUSY;
    }

    for (i = 0U; i < length; i++)
    {
        usart1_tx_queue[usart1_tx_head] = data[i];
        usart1_tx_head = (uint16_t)((usart1_tx_head + 1U) % USART1_TX_QUEUE_SIZE);
    }

    if (profile_tx != 0U)
    {
        usart1_tx_bytes_acc += length;
        usart1_tx_call_count_acc++;
    }

    __set_PRIMASK(primask);

    if (profile_tx != 0U)
    {
        start_cycles = USART1_GetCycleCount();
        USART1_StartTxFromQueue();
        usart1_tx_cycles_acc += (USART1_GetCycleCount() - start_cycles);
    }
    else
    {
        USART1_StartTxFromQueue();
    }

    return HAL_OK;
}

/*
 * 将接收到的一字节推入 USART1 RX 环形缓冲区�??
 * 如果缓冲区已满，则设置溢出标志并丢弃该字节�??
 */
static void USART1_RingBufferPush(uint8_t data)
{
    uint16_t next_head =
        (uint16_t)((usart1_rx_head + 1U) % USART1_RX_BUFFER_SIZE);

    if (next_head == usart1_rx_tail)
    {
        usart1_rx_overflow = 1U;
        return;
    }

    usart1_rx_buffer[usart1_rx_head] = data;
    usart1_rx_head = next_head;
}

/*
 * �? USART1 RX 环形缓冲区读取最�? max_len 字节到目标缓冲区中�??
 * 返回从缓冲区中读取的字节数�??
 */
static uint16_t USART1_RingBufferPop(uint8_t *data, uint16_t max_len)
{
    uint16_t count = 0U;

    while ((usart1_rx_tail != usart1_rx_head) && (count < max_len))
    {
        data[count] = usart1_rx_buffer[usart1_rx_tail];
        usart1_rx_tail = (uint16_t)((usart1_rx_tail + 1U) % USART1_RX_BUFFER_SIZE);
        count++;
    }

    return count;
}

/* USER CODE END 0 */

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

    /* USER CODE BEGIN USART1_Init 0 */

    /* USER CODE END USART1_Init 0 */

    /* USER CODE BEGIN USART1_Init 1 */

    /* USER CODE END USART1_Init 1 */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN USART1_Init 2 */

    /* USER CODE END USART1_Init 2 */
}

void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    if (uartHandle->Instance == USART1)
    {
        /* USER CODE BEGIN USART1_MspInit 0 */

        /* USER CODE END USART1_MspInit 0 */

        /** Initializes the peripherals clock
         */
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
        PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16910CLKSOURCE_D2PCLK2;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            Error_Handler();
        }

        /* USART1 clock enable */
        __HAL_RCC_USART1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**USART1 GPIO Configuration
        PA9     ------> USART1_TX
        PA10     ------> USART1_RX
        */
        GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* USART1 DMA Init */
        /* USART1_RX Init */
        hdma_usart1_rx.Instance = DMA1_Stream0;
        hdma_usart1_rx.Init.Request = DMA_REQUEST_USART1_RX;
        hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_rx.Init.Mode = DMA_CIRCULAR;
        hdma_usart1_rx.Init.Priority = DMA_PRIORITY_MEDIUM;
        hdma_usart1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(uartHandle, hdmarx, hdma_usart1_rx);

        /* USART1_TX Init */
        hdma_usart1_tx.Instance = DMA1_Stream1;
        hdma_usart1_tx.Init.Request = DMA_REQUEST_USART1_TX;
        hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority = DMA_PRIORITY_MEDIUM;
        hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(uartHandle, hdmatx, hdma_usart1_tx);

        /* USART1 interrupt Init */
        HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
        /* USER CODE BEGIN USART1_MspInit 1 */

        /* USER CODE END USART1_MspInit 1 */
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uartHandle)
{

    if (uartHandle->Instance == USART1)
    {
        /* USER CODE BEGIN USART1_MspDeInit 0 */

        /* USER CODE END USART1_MspDeInit 0 */
        /* Peripheral clock disable */
        __HAL_RCC_USART1_CLK_DISABLE();

        /**USART1 GPIO Configuration
        PA9     ------> USART1_TX
        PA10     ------> USART1_RX
        */
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);

        /* USART1 DMA DeInit */
        HAL_DMA_DeInit(uartHandle->hdmarx);
        HAL_DMA_DeInit(uartHandle->hdmatx);

        /* USART1 interrupt Deinit */
        HAL_NVIC_DisableIRQ(USART1_IRQn);
        /* USER CODE BEGIN USART1_MspDeInit 1 */

        /* USER CODE END USART1_MspDeInit 1 */
    }
}

/* USER CODE BEGIN 1 */
/*
 * USART1 用户回调与辅�? API�?
 *
 * 本节包含驱动中断接收流程、回显接收数据�?�以及计�? CPU 使用率的
 * 用户自定义操作�??
 */
HAL_StatusTypeDef USART1_StartReceiveIT(void)
{
    return HAL_UART_Receive_IT(&huart1, &usart1_rx_byte, 1U);
}

void USART1_Init(void)
{
    MX_USART1_UART_Init();
    USART1_CpuMonitorInit();
    (void)USART1_StartReceiveIT();
}

void comInit(void)
{
    USART1_Init();
}

/*
 * 初始�? USART1 �? CPU 监测�?
 * 启用周期计数器并重置统计数据，开始新的监测窗口�??
 */
void USART1_CpuMonitorInit(void)
{
    USART1_EnableCycleCounter();
    memset(&usart1_cpu_stats, 0, sizeof(usart1_cpu_stats));
    usart1_cpu_monitor_tick = HAL_GetTick();
}

/*
 * 轮询安全的处理函数�??
 * 处理 RX 缓冲区溢出提示，并将接收到的数据回显回发送端�?
 */
void USART1_Process(void)
{
    uint8_t tx_buffer[USART1_RX_BUFFER_SIZE];
    uint16_t tx_length;

    if (usart1_rx_overflow != 0U)
    {
        static const char overflow_msg[] = "\r\n[USART1] RX buffer overflow\r\n";

        usart1_rx_overflow = 0U;
        USART1_TransmitBytes((const uint8_t *)overflow_msg,
                             (uint16_t)strlen(overflow_msg), 1U);
    }

    tx_length = USART1_RingBufferPop(tx_buffer, (uint16_t)sizeof(tx_buffer));
    if (tx_length > 0U)
    {
        USART1_TransmitBytes(tx_buffer, tx_length, 1U);
    }
}

/*
 * 通过 USART1 发�?�一个以 NUL 结尾的字符串�?
 * 只有发�?�成功时，才将�?�时累加到�?�能统计中�??
 */
HAL_StatusTypeDef USART1_SendString(const char *str)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint16_t length;

    if (str == NULL)
    {
        return HAL_ERROR;
    }

    length = (uint16_t)strlen(str);
    status = USART1_TransmitBytes((const uint8_t *)str, length, 1U);
    return status;
}

HAL_StatusTypeDef USART1_Send(const uint8_t *data, uint16_t length)
{
    return USART1_TransmitBytes(data, length, 1U);
}

HAL_StatusTypeDef USART1_Receive(uint8_t *data, uint16_t max_length, uint16_t *received_length)
{
    if ((data == NULL) || (received_length == NULL) || (max_length == 0U))
    {
        return HAL_ERROR;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    *received_length = USART1_RingBufferPop(data, max_length);
    __set_PRIMASK(primask);

    return HAL_OK;
}

HAL_StatusTypeDef UART_SendBuf(const uint8_t *data, uint16_t length)
{
    return USART1_Send(data, length);
}

HAL_StatusTypeDef comSendBuf(const uint8_t *data, uint16_t length)
{
    return UART_SendBuf(data, length);
}

void comSendChar(uint8_t ch)
{
    (void)UART_SendBuf(&ch, 1);
}

uint16_t UART_GetChar(uint8_t *data, uint16_t max_length)
{
    uint16_t received = 0;

    if (USART1_Receive(data, max_length, &received) != HAL_OK)
    {
        return 0;
    }

    return received;
}

uint16_t comGetChar(uint8_t *data, uint16_t max_length)
{
    return UART_GetChar(data, max_length);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uint32_t start_cycles = USART1_GetCycleCount();
        uint32_t primask = __get_PRIMASK();

        __disable_irq();
        usart1_tx_tail = (uint16_t)((usart1_tx_tail + usart1_tx_active_size) % USART1_TX_QUEUE_SIZE);
        __set_PRIMASK(primask);

        USART1_StartTxFromQueue();
        usart1_tx_cycles_acc += (USART1_GetCycleCount() - start_cycles);
    }
}

/*
 * USART1 的周期�?? CPU 监测任务�?
 * 在中断保护下复制统计计数，计�? CPU 使用率百分比，并通过 RTT 打印日志�?
 */
void USART1_CpuMonitorTask(void)
{
    uint32_t now_tick = HAL_GetTick();
    uint32_t elapsed_ms = now_tick - usart1_cpu_monitor_tick;
    uint32_t primask;
    uint32_t rx_cycles;
    uint32_t tx_cycles;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t rx_interrupt_count;
    uint32_t tx_call_count;
    uint32_t window_cycles;
    uint32_t rx_cpu_permille;
    uint32_t tx_cpu_permille;
    uint32_t total_cpu_permille;

    if (elapsed_ms < USART1_CPU_MONITOR_PERIOD_MS)
    {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    rx_cycles = usart1_rx_cycles_acc;
    tx_cycles = usart1_tx_cycles_acc;
    rx_bytes = usart1_rx_bytes_acc;
    tx_bytes = usart1_tx_bytes_acc;
    rx_interrupt_count = usart1_rx_interrupt_count_acc;
    tx_call_count = usart1_tx_call_count_acc;
    usart1_rx_cycles_acc = 0U;
    usart1_tx_cycles_acc = 0U;
    usart1_rx_bytes_acc = 0U;
    usart1_tx_bytes_acc = 0U;
    usart1_rx_interrupt_count_acc = 0U;
    usart1_tx_call_count_acc = 0U;
    __set_PRIMASK(primask);

    window_cycles = USART1_GetWindowCycles(elapsed_ms);
    usart1_cpu_monitor_tick = now_tick;

    usart1_cpu_stats.window_ms = elapsed_ms;
    usart1_cpu_stats.rx_cycles = rx_cycles;
    usart1_cpu_stats.tx_cycles = tx_cycles;
    usart1_cpu_stats.total_cycles = rx_cycles + tx_cycles;
    usart1_cpu_stats.window_cycles = window_cycles;
    usart1_cpu_stats.rx_bytes = rx_bytes;
    usart1_cpu_stats.tx_bytes = tx_bytes;
    usart1_cpu_stats.rx_interrupt_count = rx_interrupt_count;
    usart1_cpu_stats.tx_call_count = tx_call_count;
    rx_cpu_permille =
        (window_cycles == 0U)
            ? 0U
            : (uint32_t)(((uint64_t)rx_cycles * 1000ULL) / window_cycles);
    tx_cpu_permille =
        (window_cycles == 0U)
            ? 0U
            : (uint32_t)(((uint64_t)tx_cycles * 1000ULL) / window_cycles);
    total_cpu_permille =
        (window_cycles == 0U)
            ? 0U
            : (uint32_t)(((uint64_t)(rx_cycles + tx_cycles) * 1000ULL) /
                         window_cycles);
    usart1_cpu_stats.rx_cpu_percent = (float)rx_cpu_permille / 10.0f;
    usart1_cpu_stats.tx_cpu_percent = (float)tx_cpu_permille / 10.0f;
    usart1_cpu_stats.total_cpu_percent = (float)total_cpu_permille / 10.0f;
    /*
     * RTT 日志说明:
     * - window_ms: 本次统计窗口的时长，单位毫秒
     * - rx_bytes: 本窗口内 USART1 接收的字节�?�数
     * - rx_interrupt_count: 本窗口内 USART1 接收中断触发次数
     * - rx_cpu_permille / 10U: USART1 接收处理占用�? CPU 百分比整数部�?
     * - rx_cpu_permille % 10U: USART1 接收处理占用�? CPU 百分比小数部分（1/10�?
     * - tx_bytes: 本窗口内 USART1 发�?�的字节总数
     * - tx_call_count: 本窗口内 USART1 发�?�调用次�?
     * - tx_cpu_permille / 10U: USART1 发�?�处理占用的 CPU 百分比整数部�?
     * - tx_cpu_permille % 10U: USART1 发�?�处理占用的 CPU 百分比小数部分（1/10�?
     * - total_cpu_permille / 10U: 本窗口内 USART1 总共占用�? CPU 百分比整数部�?
     * - total_cpu_permille % 10U: 本窗口内 USART1 总共占用�? CPU 百分比小数部分（1/10�?
     */
    RTT_LogPrintf("[USART1 CPU %lums] RX:%luB/%luIRQ %lu.%01lu%%, "
                  "TX:%luB/%luCall %lu.%01lu%%, TOTAL:%lu.%01lu%%\r\n",
                  usart1_cpu_stats.window_ms, usart1_cpu_stats.rx_bytes,
                  usart1_cpu_stats.rx_interrupt_count, rx_cpu_permille / 10U,
                  rx_cpu_permille % 10U, usart1_cpu_stats.tx_bytes,
                  usart1_cpu_stats.tx_call_count, tx_cpu_permille / 10U,
                  tx_cpu_permille % 10U, total_cpu_permille / 10U,
                  total_cpu_permille % 10U);
}

/*
 * USART1 接收完成中断�? HAL 回调�?
 * 将接收字节推入本地环形缓冲区，并立即重启下一次中断接收�??
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uint32_t start_cycles = USART1_GetCycleCount();

        USART1_RingBufferPush(usart1_rx_byte);
        (void)HAL_UART_Receive_IT(&huart1, &usart1_rx_byte, 1U);
        usart1_rx_cycles_acc += (USART1_GetCycleCount() - start_cycles);
        usart1_rx_bytes_acc++;
        usart1_rx_interrupt_count_acc++;
    }
}

/*
 * USART1 错误条件�? HAL 回调�?
 * 清除常见 UART 错误标志，并重启中断接收以便自动恢复�?
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_PEFLAG(huart);
        (void)HAL_UART_Receive_IT(&huart1, &usart1_rx_byte, 1U);
    }
}

/* USER CODE END 1 */
