#include "mqtt.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT Tag";

typedef struct mqtt_config {
    bool enable;
    char* broker;
    char* host;
} mqtt_config_t;

mqtt_config_t mqtt_module_config = {
    .enable = false,
    .broker = "mqtt://",
    .host = "iot-module"
};

esp_err_t mqtt_init_from_json(cJSON *config) {
    ESP_LOGD(TAG, "Init MQTT from JSON");
    mqtt_module_config.enable = cJSON_GetObjectItem(config, "enable")->valueint;
    mqtt_module_config.broker = cJSON_GetObjectItem(config, "broker")->valuestring;
    mqtt_module_config.host = cJSON_GetObjectItem(config, "host")->valuestring;

    if (mqtt_module_config.enable == 1) {
        ESP_LOGD(TAG, "MQTT enabled in json, try to start mqtt client");
        return start_mqtt_client();
    }

    return ESP_OK;
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    char topic[35];
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            sprintf(topic, "/%s/test/qos1", mqtt_module_config.host);
            msg_id = esp_mqtt_client_publish(client, topic, "data_3", 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            sprintf(topic, "/%s/test/qos0", mqtt_module_config.host);
            msg_id = esp_mqtt_client_subscribe(client, topic, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            sprintf(topic, "/%s/test/qos1", mqtt_module_config.host);
            msg_id = esp_mqtt_client_subscribe(client, topic, 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            sprintf(topic, "/%s/test/qos1", mqtt_module_config.host);
            msg_id = esp_mqtt_client_unsubscribe(client, topic);
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            sprintf(topic, "/%s/test/qos0", mqtt_module_config.host);
            msg_id = esp_mqtt_client_publish(client, topic, "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}



static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static esp_err_t start_mqtt_client() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_module_config.broker
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    return esp_mqtt_client_start(client);
}


cJSON * get_mqtt_config() {
    cJSON *config = cJSON_CreateObject();
    cJSON_AddNumberToObject(config, "enable", mqtt_module_config.enable);
    cJSON_AddStringToObject(config, "broker", mqtt_module_config.broker);
    cJSON_AddStringToObject(config, "host", mqtt_module_config.host);
    return config;
}