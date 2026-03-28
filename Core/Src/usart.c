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
#include "rtt_log.h"
#include <string.h>

/* USER CODE BEGIN 0 */
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
static uint32_t usart1_cpu_monitor_tick;
static USART1_CpuStats_t usart1_cpu_stats;

static uint32_t USART1_GetCycleCount(void)
{
    return DWT->CYCCNT;
}

static void USART1_EnableCycleCounter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t USART1_GetWindowCycles(uint32_t elapsed_ms)
{
    uint64_t cpu_hz = (uint64_t)HAL_RCC_GetHCLKFreq();

    return (uint32_t)((cpu_hz * elapsed_ms) / 1000ULL);
}

static void USART1_TransmitBytes(const uint8_t *data, uint16_t length, uint8_t profile_tx)
{
    uint32_t start_cycles;

    if ((data == NULL) || (length == 0U))
    {
        return;
    }

    start_cycles = USART1_GetCycleCount();
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, length, HAL_MAX_DELAY);

    if (profile_tx != 0U)
    {
        usart1_tx_cycles_acc += (USART1_GetCycleCount() - start_cycles);
        usart1_tx_bytes_acc += length;
        usart1_tx_call_count_acc++;
    }
}

static void USART1_RingBufferPush(uint8_t data)
{
    uint16_t next_head = (uint16_t)((usart1_rx_head + 1U) % USART1_RX_BUFFER_SIZE);

    if (next_head == usart1_rx_tail)
    {
        usart1_rx_overflow = 1U;
        return;
    }

    usart1_rx_buffer[usart1_rx_head] = data;
    usart1_rx_head = next_head;
}

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

        /* USART1 interrupt Init */
        HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
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

        /* USART1 interrupt Deinit */
        HAL_NVIC_DisableIRQ(USART1_IRQn);
        /* USER CODE BEGIN USART1_MspDeInit 1 */

        /* USER CODE END USART1_MspDeInit 1 */
    }
}

/* USER CODE BEGIN 1 */
HAL_StatusTypeDef USART1_StartReceiveIT(void)
{
    return HAL_UART_Receive_IT(&huart1, &usart1_rx_byte, 1U);
}

void USART1_CpuMonitorInit(void)
{
    USART1_EnableCycleCounter();
    memset(&usart1_cpu_stats, 0, sizeof(usart1_cpu_stats));
    usart1_cpu_monitor_tick = HAL_GetTick();
}

void USART1_Process(void)
{
    uint8_t tx_buffer[USART1_RX_BUFFER_SIZE];
    uint16_t tx_length;

    if (usart1_rx_overflow != 0U)
    {
        static const char overflow_msg[] = "\r\n[USART1] RX buffer overflow\r\n";

        usart1_rx_overflow = 0U;
        USART1_TransmitBytes((const uint8_t *)overflow_msg, (uint16_t)strlen(overflow_msg), 1U);
    }

    tx_length = USART1_RingBufferPop(tx_buffer, (uint16_t)sizeof(tx_buffer));
    if (tx_length > 0U)
    {
        USART1_TransmitBytes(tx_buffer, tx_length, 1U);
    }
}

HAL_StatusTypeDef USART1_SendString(const char *str)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint16_t length;
    uint32_t start_cycles;

    if (str == NULL)
    {
        return HAL_ERROR;
    }

    length = (uint16_t)strlen(str);
    start_cycles = USART1_GetCycleCount();
    status = HAL_UART_Transmit(&huart1, (uint8_t *)str, length, HAL_MAX_DELAY);
    if (status == HAL_OK)
    {
        usart1_tx_cycles_acc += (USART1_GetCycleCount() - start_cycles);
        usart1_tx_bytes_acc += length;
        usart1_tx_call_count_acc++;
    }

    return status;
}

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
    rx_cpu_permille = (window_cycles == 0U) ? 0U : (uint32_t)(((uint64_t)rx_cycles * 1000ULL) / window_cycles);
    tx_cpu_permille = (window_cycles == 0U) ? 0U : (uint32_t)(((uint64_t)tx_cycles * 1000ULL) / window_cycles);
    total_cpu_permille = (window_cycles == 0U) ? 0U : (uint32_t)(((uint64_t)(rx_cycles + tx_cycles) * 1000ULL) / window_cycles);
    usart1_cpu_stats.rx_cpu_percent = (float)rx_cpu_permille / 10.0f;
    usart1_cpu_stats.tx_cpu_percent = (float)tx_cpu_permille / 10.0f;
    usart1_cpu_stats.total_cpu_percent = (float)total_cpu_permille / 10.0f;
    RTT_LogPrintf("[USART1 CPU %lums] RX:%luB/%luIRQ %lu.%01lu%%, TX:%luB/%luCall %lu.%01lu%%, TOTAL:%lu.%01lu%%\r\n",
                  usart1_cpu_stats.window_ms,
                  usart1_cpu_stats.rx_bytes,
                  usart1_cpu_stats.rx_interrupt_count,
                  rx_cpu_permille / 10U,
                  rx_cpu_permille % 10U,
                  usart1_cpu_stats.tx_bytes,
                  usart1_cpu_stats.tx_call_count,
                  tx_cpu_permille / 10U,
                  tx_cpu_permille % 10U,
                  total_cpu_permille / 10U,
                  total_cpu_permille % 10U);
}

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
