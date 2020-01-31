/** @file config.h
 * 
 * @brief tcp2rtu gateway
 *
 * @par       
 * COPYRIGHT NOTICE: (c) 2020 Maksym Krasovskyi.  All rights reserved.
 */ 

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include "cJSON.h"

cJSON * readConfig();
void writeModbusConfig(bool enable, int speed);
void writeGpioConfig(cJSON *gpio);
void write_mqtt_config(cJSON *gpio);

#endif /* CONFIG_H */


