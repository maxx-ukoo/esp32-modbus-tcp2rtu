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
#define SUPPORTED_MODES 4


#define MODE_INPUT 1
#define MODE_OUTPUT 2
#define MODE_PWM 4

#ifndef GPIO_H
#define GPIO_H

#include "esp_event.h"

typedef struct {
    int pin;
    int state;
} pin_state_msg_t;

int gpioInitFromJson(cJSON *gpio);
cJSON * getGpioConfig();
cJSON * getPinState(int pin);
esp_err_t setPinState(int pin, int state);
void set_mqtt_gpio_evt_queue(xQueueHandle gpio2mqtt_queue_handler, xQueueHandle mqtt2gpio_queue_handler);

#endif /* GPIO_H */


