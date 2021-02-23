
#ifndef MQTT_H
#define MQTT_H

#ifdef __cplusplus
extern "C" {
#endif
    #include "cJSON.h"
    #include "esp_err.h"
    #include "mqtt_client.h"
#ifdef __cplusplus
}
#endif

typedef struct mqtt_config
{
    bool enable;
    char *broker;
    char *host;
} mqtt_config_t;

class IOTMqtt {
    private:
        IOTMqtt(const IOTMqtt&);
        IOTMqtt& operator =(const IOTMqtt&);
        static esp_mqtt_client_handle_t client;
        static mqtt_config_t mqtt_module_config;
        static char *mqtt2gpio_topic;
        static char *mqtt2curtains_topic;
        static xQueueHandle gpio2mqtt_queue;
        static xQueueHandle mqtt2gpio_queue;
        static TaskHandle_t health_status_task_handle;
        static mqtt_config_t init_with_default_config();
        static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);
        static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, esp_mqtt_event_handle_t event_data);
        static void gpio2mqtt_task();
        static esp_err_t subscribe_to_gpio();
        static void health_status_task(void *pvParameter);
        static esp_err_t start_mqtt_client();
        static void decode_mqtt_message(esp_mqtt_event_handle_t event);
    public:
        IOTMqtt(cJSON *curtains);
        static esp_err_t mqtt_json_init(cJSON *gpio);
        static cJSON *get_mqtt_config();
        ~IOTMqtt(void);
};

#endif /* MQTT_H */
