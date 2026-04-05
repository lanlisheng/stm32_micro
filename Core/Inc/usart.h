/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    usart.h
 * @brief   This file contains all the function prototypes for
 *          the usart.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

    /* USER CODE BEGIN Includes */
    /* USER CODE END Includes */

    extern UART_HandleTypeDef huart1;

/* USER CODE BEGIN Private defines */
#define USART1_RX_BUFFER_SIZE 128U
#define USART1_CPU_MONITOR_PERIOD_MS 1000U

    typedef struct
    {
        uint32_t window_ms;
        uint32_t rx_cycles;
        uint32_t tx_cycles;
        uint32_t total_cycles;
        uint32_t window_cycles;
        uint32_t rx_bytes;
        uint32_t tx_bytes;
        uint32_t rx_interrupt_count;
        uint32_t tx_call_count;
        float rx_cpu_percent;
        float tx_cpu_percent;
        float total_cpu_percent;
    } USART1_CpuStats_t;

    /* USER CODE END Private defines */

    void MX_USART1_UART_Init(void);

    /* USER CODE BEGIN Prototypes */
    HAL_StatusTypeDef USART1_StartReceiveIT(void);
    void USART1_Init(void);
    void comInit(void);
    void USART1_Process(void);
    HAL_StatusTypeDef USART1_SendString(const char *str);
    HAL_StatusTypeDef USART1_Send(const uint8_t *data, uint16_t length);
    HAL_StatusTypeDef USART1_Receive(uint8_t *data, uint16_t max_length, uint16_t *received_length);
    HAL_StatusTypeDef UART_SendBuf(const uint8_t *data, uint16_t length);
    HAL_StatusTypeDef comSendBuf(const uint8_t *data, uint16_t length);
    void comSendChar(uint8_t ch);
    uint16_t UART_GetChar(uint8_t *data, uint16_t max_length);
    uint16_t comGetChar(uint8_t *data, uint16_t max_length);
    void USART1_CpuMonitorInit(void);
    void USART1_CpuMonitorTask(void);

    extern uint16_t uart_write(uint8_t uart_id, const uint8_t *buf, uint16_t size);
    extern uint16_t uart_read(uint8_t uart_id, uint8_t *buf, uint16_t size);
    extern void uart_dmarx_done_isr(uint8_t uart_id);
    extern void uart_dmarx_half_done_isr(uint8_t uart_id);
    extern void uart_dmarx_idle_isr(uint8_t uart_id);
    extern void uart_dmatx_done_isr(uint8_t uart_id);
    extern void uart_poll_dma_tx(uint8_t uart_id);

    /* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */
