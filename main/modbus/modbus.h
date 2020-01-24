/** @file module.h
 * 
 * @brief tcp2rtu gateway
 *
 * @par       
 * COPYRIGHT NOTICE: (c) 2020 Maksym Krasovskyi.  All rights reserved.
 */ 

#ifndef MODBUS_H
#define MODBUS_H

#include <stdio.h>
#include "esp_err.h"

esp_err_t modbus_start(int port_speed);
esp_err_t modbus_init(int port_speed);
void tcp_server_task(void *pvParameters);
void eth2rtu_flow_control_task(void *args);
esp_err_t initialize_flow_control(void);

#endif /* MODBUS_H */
