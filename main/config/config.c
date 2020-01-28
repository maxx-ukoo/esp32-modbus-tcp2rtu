#include "config.h"
#include <stdio.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "esp_log.h"
#include "gpio\gpio.h"


static const char *CONFIG_TAG = "IOT DIN Config";
static const char *CONFIG_FILE = "/www/config.json";

void writeConfig(cJSON *config) {
    FILE* f = fopen(CONFIG_FILE, "w+");
    if (f == NULL) {
        ESP_LOGE(CONFIG_TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, cJSON_Print(config));
    fclose(f);
}

cJSON * createModbusConfig(bool e, int s) {
    cJSON *speed = NULL;
    cJSON *enable = NULL;
    cJSON *modbus = cJSON_CreateObject();
    if (modbus == NULL)
    {
        goto end;
    }
    speed = cJSON_CreateNumber(s);
    if (speed == NULL)
    {
        goto end;
    }
    cJSON_AddItemToObject(modbus, "speed", speed);
    enable = cJSON_CreateBool(e);
    if (enable == NULL)
    {
        goto end;
    }
    cJSON_AddItemToObject(modbus, "enable", enable);
    return modbus;
end:
    cJSON_Delete(modbus);
    return NULL;
}

cJSON * createGpioConfig(bool e, int pinsConfig[GPIO_NUMBER][5]) {
    cJSON *gpio = cJSON_CreateObject();
    cJSON *enable = NULL;
    cJSON *config = NULL;
    cJSON *pin = NULL;
    cJSON *pin_item1 = NULL;
    cJSON *pin_item2 = NULL;
    cJSON *pin_item3 = NULL;
    cJSON *pin_item4 = NULL;
    if (gpio == NULL)
    {
        goto end;
    }
    enable = cJSON_CreateBool(e);
    if (enable == NULL)
    {
        goto end;
    }
    cJSON_AddItemToObject(gpio, "enable", enable);
    config = cJSON_CreateArray();
    if (config == NULL)
    {
        goto end;
    }
    cJSON_AddItemToObject(gpio, "config", config);
    for (int i = 0; i < GPIO_NUMBER; ++i) {
        ESP_LOGE(CONFIG_TAG, "Creating config for pin %d", pinsConfig[i][ID]);
        pin = cJSON_CreateObject();
        if (pin == NULL)
        {
            goto end;
        }
        cJSON_AddItemToArray(config, pin);
        pin_item1 = cJSON_CreateNumber(pinsConfig[i][ID]);
        if (pin_item1 == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(pin, "id", pin_item1);
        pin_item2 = cJSON_CreateNumber(pinsConfig[i][MODE]);
        if (pin_item2 == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(pin, "mode", pin_item2);
        pin_item3 = cJSON_CreateNumber(pinsConfig[i][PULL_U]);
        if (pin_item3 == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(pin, "pull_up", pin_item3);
        pin_item4 = cJSON_CreateNumber(pinsConfig[i][PULL_D]);
        if (pin_item4 == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(pin, "pull_down", pin_item4);
    }
    return gpio;
end:
    ESP_LOGE(CONFIG_TAG, "Error on creating gpio config");
    cJSON_Delete(gpio);
    return NULL;
}


cJSON * createDefaultConfig() {
    cJSON *config = cJSON_CreateObject();
    if (config == NULL)
    {
        goto end;
    }
    cJSON *modbus = createModbusConfig(false, 9600);
    cJSON_AddItemToObject(config, "modbus", modbus);
    cJSON *gpio = createGpioConfig(false, gpioDefaultConfig);
    cJSON_AddItemToObject(config, "gpio", gpio);
    writeConfig(config);
    return config;
    end:
        cJSON_Delete(config);
        return NULL;
}

cJSON * readConfig() {
    FILE* f = fopen(CONFIG_FILE, "r");
    if (f == NULL) {
        ESP_LOGE(CONFIG_TAG, "Failed to open file for reading");
        return createDefaultConfig();
    }

    char * buffer = 0;
    long length;
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = malloc(length+1);
    if (buffer)
    {
        fread (buffer, 1, length, f);
    }
    buffer[length] = 0;
    fclose(f);

    cJSON *monitor_json = cJSON_Parse(buffer);
    free(buffer);
    if (monitor_json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        goto end;
    }
    return monitor_json;
end: 
    return NULL;    
}

void writeModbusConfig(bool enable, int speed) {
    ESP_LOGD(CONFIG_TAG, "Modbus control update: enable = %d, speed = %d", enable, speed);

    cJSON *modbus = createModbusConfig(enable, speed);
    cJSON *config = readConfig();

    cJSON_ReplaceItemInObject(config,"modbus",modbus);
    writeConfig(config);
    cJSON_Delete(config);
}











