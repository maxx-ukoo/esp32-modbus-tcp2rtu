#include "config.h"


#ifdef __cplusplus
extern "C"
{
#endif
    #include <stdio.h>

    #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
    #include "esp_log.h"
    #include "modbus\modbus_const.h"
#ifdef __cplusplus
}
#endif

#include "gpio\gpio.h"
#include "mqtt\mqtt.h"

static const char *TAG = "IOT DIN Config";
static const char *CONFIG_FILE = "/www/config.json";

void IOTConfig::writeConfig(cJSON *config) {
    FILE* f = fopen(CONFIG_FILE, "w+");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, cJSON_Print(config));
    fclose(f);
}

cJSON *IOTConfig::createModbusConfig(bool e, int s) {
    cJSON *speed = NULL;
    cJSON *enable = NULL;
    cJSON *modbus = cJSON_CreateObject();
    if (modbus == NULL)
    {
        return NULL;
    }
    speed = cJSON_CreateNumber(s);
    if (speed == NULL)
    {
        return NULL;
    }
    cJSON_AddItemToObject(modbus, "speed", speed);
    enable = cJSON_CreateBool(e);
    if (enable == NULL)
    {
        return NULL;
    }
    cJSON_AddItemToObject(modbus, "enable", enable);
    return modbus;
}

cJSON *IOTConfig::createDefaultConfig() {
    cJSON *config = cJSON_CreateObject();
    if (config == NULL)
    {
        return NULL;
    }
    cJSON *modbus = createModbusConfig(false, 9600);
    cJSON_AddItemToObject(config, "modbus", modbus);
    cJSON *gpio = IOTGpio::get_gpio_config();
    cJSON_AddItemToObject(config, "gpio", gpio);
    cJSON *mqtt = IOTMqtt::get_mqtt_config();
    cJSON_AddItemToObject(config, "mqtt", mqtt);
    writeConfig(config);
    return config;
}

cJSON *IOTConfig::readConfig() {
    FILE* f = fopen(CONFIG_FILE, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return createDefaultConfig();
    }

    char* buffer = 0;
    long length;
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = (char*) malloc(length+1);
    if (buffer)
    {
        fread(buffer, 1, length, f);
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

void IOTConfig::writeModbusConfig(bool enable, int speed) {
    ESP_LOGD(TAG, "Modbus control update: enable = %d, speed = %d", enable, speed);
    cJSON *config = readConfig();
    cJSON *modbus = createModbusConfig(enable, speed);
    cJSON_ReplaceItemInObject(config,"modbus",modbus);
    writeConfig(config);
    cJSON_Delete(config);
    writeGpioConfig();
}

void IOTConfig::writeGpioConfig() {
    cJSON *config = readConfig();
    ESP_LOGD(TAG, "Read");
    cJSON *gpio = IOTGpio::get_gpio_config();
    cJSON_ReplaceItemInObject(config,"gpio",gpio);
    ESP_LOGD(TAG, "Replaced");
    writeConfig(config);
    ESP_LOGD(TAG, "write");
    cJSON_Delete(config);
    ESP_LOGD(TAG, "deleted");
}

void IOTConfig::write_mqtt_config(cJSON *mqtt) {
    cJSON *config = readConfig();
    ESP_LOGD(TAG, "Read");
    cJSON_ReplaceItemInObject(config,"mqtt",mqtt);
    ESP_LOGD(TAG, "Replaced");
    writeConfig(config);
    ESP_LOGD(TAG, "write");
    cJSON_Delete(config);
    ESP_LOGD(TAG, "deleted");
}