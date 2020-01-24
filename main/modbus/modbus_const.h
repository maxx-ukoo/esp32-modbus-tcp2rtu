/** @file module.h
 * 
 * @brief tcp2rtu gateway
 *
 * @par       
 * COPYRIGHT NOTICE: (c) 2020 Maksym Krasovskyi.  All rights reserved.
 */ 

#ifndef MODBUS_CONST_H
#define MODBUS_CONST_H

//#include <stdio.h>
#include "esp_err.h"

#define MB_PORTNUM 2
#define MB_PARITY UART_PARITY_DISABLE

#define CONFIG_MB_UART_RXD 18
#define CONFIG_MB_UART_TXD 13
#define CONFIG_MB_UART_RTS 4

#define PORT 502
#define FLOW_CONTROL_QUEUE_TIMEOUT_MS (100)
#define FLOW_CONTROL_QUEUE_LENGTH (5)

// Tested functions
#define MB_FUNC_READ_INPUT_REGISTER 4
#define MB_FUNC_READ_HOLDING_REGISTER 3

//#define MB_FC_NONE 0
//#define MB_FC_READ_REGISTERS 3 //implemented
//#define MB_FC_WRITE_REGISTER 6 //implemented
//#define MB_FC_WRITE_MULTIPLE_REGISTERS 16 //implemented

//
// MODBUS MBAP offsets
//
#define MB_TCP_TID 0
#define MB_TCP_PID 2
#define MB_TCP_LEN 4
#define MB_TCP_UID 6
#define MB_TCP_FUNC 7
#define MB_TCP_REGISTER_START 8
#define MB_TCP_REGISTER_NUMBER 10

typedef struct {
    char* message;
    uint16_t length;
} flow_control_msg_t;

esp_err_t modbus_start(int port_speed);
esp_err_t modbus_init(int port_speed);
void tcp_server_task(void *pvParameters);
void eth2rtu_flow_control_task(void *args);
esp_err_t initialize_flow_control(void);

#endif /* MODBUS_CONST_H */
