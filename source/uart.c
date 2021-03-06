/*
 * Copyright (C) 2016 Kubos Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
  * @defgroup UART
  * @addtogroup UART
  * @{
  */

/**
  *
  * @file       uart.c
  * @brief      Kubos-HAL-MSP430F5529 - UART module
  *
  * @author     kubos.co
  */

#include "kubos-hal/uart.h"
#include "msp430f5529-hal/uart.h"
#include <msp430.h>

/**
  * @brief Helper function to convert uart device option.
  */
static inline hal_uart_device uart_dev(KUARTNum uart)
{
    switch(uart)
    {
        case K_UART1: return HAL_UART_A0;
        case K_UART2: return HAL_UART_A1;
        default: return 0;
    }
}

/**
  * @brief Helper function to get uart handle.
  */
static inline hal_uart_handle * uart_handle(KUARTNum uart)
{
    switch(uart)
    {
        case K_UART1: return &hal_uart_dev[HAL_UART_A0];
        case K_UART2: return &hal_uart_dev[HAL_UART_A1];
        default: return NULL;
    }
}

/**
  * @brief Helper function to convert baud option.
  */
static inline hal_uart_baudrate uart_baud(long int baud)
{
    switch(baud)
    {
        case 9600:
            return HAL_UART_9600;
        case 115200:
            return HAL_UART_115200;
        default: return HAL_UART_9600;
    }
}

/**
  * @brief Helper function to convert parity option.
  */
static inline hal_uart_parity uart_parity(KParity parity)
{
    switch (parity)
    {
        case K_PARITY_NONE: return HAL_UART_PARITY_NONE;
        case K_PARITY_EVEN: return HAL_UART_PARITY_EVEN;
        case K_PARITY_ODD:  return HAL_UART_PARITY_ODD;
        default: return HAL_UART_PARITY_NONE;
    }
}

/**
  * @brief Helper function to convert stop bits option.
  */
static inline hal_uart_stopbits uart_stopbits(KStopBits stopbits)
{
    switch (stopbits)
    {
        case K_STOP_BITS_1: return HAL_UART_STOP_BITS_1;
        case K_STOP_BITS_1_5: return HAL_UART_STOP_BITS_1_5;
        case K_STOP_BITS_2:  return HAL_UART_STOP_BITS_2;
        default: return HAL_UART_STOP_BITS_1;
    }
}

/**
  * @brief Helper function to convert word len option.
  */
static inline hal_uart_wordlen uart_wordlen(KWordLen wordlen)
{
    switch (wordlen)
    {
        case K_WORD_LEN_7BIT: return HAL_UART_WORD_LEN_7;
        case K_WORD_LEN_8BIT: return HAL_UART_WORD_LEN_8;
        default: return HAL_UART_WORD_LEN_8;
    }
}

/**
  * @brief Creates and sets up specified UART device.
  * @param uart Number of UART device to setup.
  */
void kprv_uart_dev_init(KUARTNum uart)
{
    KUART * k_uart = kprv_uart_get(uart);

    hal_uart_config config = {
        .device = uart_dev(uart),
        .baudrate = uart_baud(k_uart->conf.baud_rate),
        .parity = uart_parity(k_uart->conf.parity),
        .wordlen = uart_wordlen(k_uart->conf.word_len),
        .stopbits = uart_stopbits(k_uart->conf.stop_bits)
    };

    hal_uart_handle * handle = hal_uart_init(config);
    hal_uart_setup(handle);
}


/**
  * @brief Enables UART TX interrupt.
  * @param uart Number of UART device.
  */
void kprv_uart_enable_tx_int(KUARTNum uart)
{
    hal_uart_handle * handle = uart_handle(uart);
    handle->reg->interruptEnable |= UCTXIE;
}


/**
  * @brief This function handles the rx/tx interrupts using FreeRTOS queues.
  * @param handle UART handle
  */
void hal_uart_interrupt(hal_uart_handle * handle)
{
    static portBASE_TYPE task_woken;
    KUART *k_uart = kprv_uart_get(handle->config.device);
    task_woken = pdFALSE;

    // Check for UCxRXBUF overrun error
    // According to MSP430x5xx Family Guide Section 40.4.3
    // Reading from UCxRXBUF will clear this up.
    if (HAL_UART_STAT(handle, UCOE))
    {
        // Force a read to clear error
        hal_uart_read_raw(handle);
    }

    // RX Interrupt
    if (HAL_UART_INT_FLAG(handle, UCRXIFG))
    {
        char c = hal_uart_read_raw(handle);
        xQueueSendToBackFromISR(k_uart->rx_queue, (void*) &c, &task_woken);
    }

    // TX Interrupt
    if (HAL_UART_INT_FLAG(handle, UCTXIFG))
    {
        char c;
        BaseType_t result = xQueueReceiveFromISR(k_uart->tx_queue,
                                                (void *) &c,
                                                &task_woken);

        if (result == pdTRUE)
        {
            hal_uart_write_raw(handle, c);
        }
        else
        {
            handle->reg->interruptEnable &= ~UCTXIE;
        }
    }


    if (task_woken != pdFALSE)
    {
        portYIELD();
    }
}

/* @} */
