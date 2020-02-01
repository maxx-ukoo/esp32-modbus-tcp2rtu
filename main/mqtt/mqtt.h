/** @file mqtt.h
 * 
 * @brief tcp2rtu gateway
 *
 * @par       
 * COPYRIGHT NOTICE: (c) 2020 Maksym Krasovskyi.  All rights reserved.
 */ 
#ifndef MQTT_H
#define MQTT_H

#include "cJSON.h"
#include "esp_err.h"

esp_err_t mqtt_init_from_json(cJSON *gpio);
cJSON * get_mqtt_config();

static esp_err_t start_mqtt_client();

#endif /* MQTT_H */


