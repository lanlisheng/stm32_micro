
/*
 ******************************************************************************
 * @file    usart.c
 * @brief   This file provides code for the configuration
 *          of the USART instances.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "usart.h"
#include "fifo.h"
#include "dev_uart.h"
#include "main.h"

/* 串口设备实例 */
uart_device_t s_uart_dev[2] = {0};

/* 测试 */
uint32_t s_UartTxRxCount[4] = {0};

/* fifo上锁函数 */
void fifo_lock(void)
{
    __disable_irq();
}

/* fifo解锁函数 */
void fifo_unlock(void)
{
    __enable_irq();
}

/**
 * @brief  串口读取数据接口，实际是从接收fifo读取
 * @param
 * @retval
 */
uint16_t uart_read(uint8_t uart_id, uint8_t *buf, uint16_t size)
{
    return fifo_read(&s_uart_dev[uart_id].rx_fifo, buf, size);
}

/**
 * @brief  串口发送数据接口，实际是写入发送fifo，发送由dma处理
 * @param
 * @retval
 */
uint16_t uart_write(uint8_t uart_id, const uint8_t *buf, uint16_t size)
{
    return fifo_write(&s_uart_dev[uart_id].tx_fifo, buf, size);
}
