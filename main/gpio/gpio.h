/** @file gpio.h
 * 
 * @brief tcp2rtu gateway
 *
 * @par       
 * COPYRIGHT NOTICE: (c) 2020 Maksym Krasovskyi.  All rights reserved.
 */ 
#include "cJSON.h"

#define GPIO_NUMBER 15

#define ID 0
#define MODE 1
#define PULL_U 2
#define PULL_D 3
#define MODE_MAX 4

#ifndef GPIO_H
#define GPIO_H

int gpioInitFromJson(cJSON *gpio);
cJSON * getGpioConfig();
cJSON * getPinState(int pin);
cJSON * setPinState(int pin, int state);

#endif /* GPIO_H */


