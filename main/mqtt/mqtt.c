#include "mqtt.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "mqtt_client.h"
#include "gpio/gpio.h"

static const char *TAG = "MQTT Tag";

typedef struct mqtt_config {
    bool enable;
    char* broker;
    char* host;
} mqtt_config_t;

static mqtt_config_t mqtt_module_config;
static esp_mqtt_client_handle_t client = NULL;
static char* mqtt2gpio_topic = NULL;
xQueueHandle gpio2mqtt_queue = NULL;
xQueueHandle mqtt2gpio_queue = NULL;
static TaskHandle_t health_status_task_handle = NULL;

static mqtt_config_t init_with_default_config() {
    mqtt_config_t config = {
        .enable = false,
        .broker = (char *)malloc(sizeof(char) * (50)),
        .host = (char *)malloc(sizeof(char) * (50)),
    };
    return config;
}

esp_err_t mqtt_init_from_json(cJSON *config) {
    ESP_LOGD(TAG, "Init MQTT from JSON");
    if (mqtt_module_config.broker == NULL) {
        mqtt_module_config = init_with_default_config();
    }
    mqtt_module_config.enable = cJSON_GetObjectItem(config, "enable")->valueint;
    //mqtt_module_config.broker = cJSON_GetObjectItem(config, "broker")->valuestring;
    //free(mqtt_module_config.host);
    
    strncpy(mqtt_module_config.broker, cJSON_GetObjectItem(config, "broker")->valuestring, 50);
    strncpy(mqtt_module_config.host, cJSON_GetObjectItem(config, "host")->valuestring, 50);
    ESP_LOGI(TAG, "mqtt_init, host config=%s", mqtt_module_config.host);
    ESP_LOGI(TAG, "mqtt_init, brocker config=%s", mqtt_module_config.broker);
    if (mqtt_module_config.enable == 1) {
        ESP_LOGD(TAG, "MQTT enabled in  json, try to start mqtt client");
        return start_mqtt_client();
    }

    return ESP_OK;
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    //char topic[50];
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            //snprintf(topic, sizeof topic, "/%s/test/qos1", mqtt_module_config.host);
            //msg_id = esp_mqtt_client_publish(client, topic, "data_3", 0, 1, 0);
            //ESP_LOGI(TAG, "sent publish successful, topic=%s, msg_id=%d", topic, msg_id);
            //snprintf(topic, sizeof topic, "/%s/test/qos0", mqtt_module_config.host);
            msg_id = esp_mqtt_client_subscribe(client, mqtt2gpio_topic, 0);
            ESP_LOGI(TAG, "subscribe successful, topic=%s, msg_id=%d", mqtt2gpio_topic, msg_id);
            //snprintf(topic, sizeof topic, "/%s/test/qos1", mqtt_module_config.host);
            //msg_id = esp_mqtt_client_subscribe(client, topic, 1);
            //ESP_LOGI(TAG, "sent subscribe successful, topic=%s, msg_id=%d", topic, msg_id);
            //snprintf(topic, sizeof topic, "/%s/test/qos1", mqtt_module_config.host);
            //msg_id = esp_mqtt_client_unsubscribe(client, topic);
            //ESP_LOGI(TAG, "sent unsubscribe successful, topic=%s, msg_id=%d", topic, msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            //snprintf(topic, sizeof topic, "/%s/test/qos0", mqtt_module_config.host);
            //msg_id = esp_mqtt_client_publish(client, topic, "data", 0, 0, 0);
            //ESP_LOGI(TAG, "sent publish successful, topic=%s, msg_id=%d", topic, msg_id);
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

            cJSON *data = cJSON_Parse(event->data);
            int pin = cJSON_GetObjectItem(data, "pin")->valueint;
            int state = cJSON_GetObjectItem(data, "state")->valueint;
            cJSON_Delete(data);
            ESP_LOGI(TAG, "MQTT2GPIO_EVENT, pinn=%d, state=%d", pin, state);
            pin_state_msg_t msg = {
                .pin = pin,
                .state = state
            };
            if (xQueueSend(mqtt2gpio_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
                    ESP_LOGE(TAG, "send pin state message failed or timeout");
            } 
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

static void gpio2mqtt_task() {
    pin_state_msg_t msg;
    for(;;) {
        if (gpio2mqtt_queue != NULL) {
            if(xQueueReceive(gpio2mqtt_queue, &msg, portMAX_DELAY)) {
                ESP_LOGD(TAG, "MQTT received GPIO[%d] intr, val:%d", msg.pin, msg.state);
                char topic[50];
                char data[10];
                snprintf(topic, sizeof topic, "/%s/gpio/state/%d", mqtt_module_config.host, msg.pin);
                snprintf(data, sizeof data, "%d", msg.state);
                int msg_id = esp_mqtt_client_publish(client, topic, data, 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, topic=%s, msg_id=%d", topic, msg_id);
            }
        } else {
            vTaskDelay(50);
        }
    }
}

static esp_err_t subscribe_to_gpio() {
    if (mqtt2gpio_topic == NULL) {
        mqtt2gpio_topic = (char *)malloc(sizeof(char) * (50));
    }
    snprintf(mqtt2gpio_topic, 50, "/%s/gpio/command", mqtt_module_config.host);
    if (gpio2mqtt_queue == NULL) {
        gpio2mqtt_queue = xQueueCreate(5, sizeof(pin_state_msg_t));
        BaseType_t ret = xTaskCreate(gpio2mqtt_task, "gpio2mqtt_task", 4096, NULL, 5, NULL);
        if (ret != pdTRUE) {
            ESP_LOGE(TAG, "create gpio2mqtt_task task failed");
            return ESP_FAIL;
        }
    }
    if (mqtt2gpio_queue == NULL) {
        mqtt2gpio_queue = xQueueCreate(5, sizeof(pin_state_msg_t));
        BaseType_t ret = xTaskCreate(gpio2mqtt_task, "gpio2mqtt_task", 4096, NULL, 5, NULL);
        if (ret != pdTRUE) {
            ESP_LOGE(TAG, "create gpio2mqtt_task task failed");
            return ESP_FAIL;
        }
    }
    set_mqtt_gpio_evt_queue(gpio2mqtt_queue, mqtt2gpio_queue);
    return ESP_OK;
}

void health_status_task(void *pvParameter)
{
    const TickType_t xDelay30s = pdMS_TO_TICKS(30000);
    while(1) {
        vTaskDelay(xDelay30s);
        ESP_LOGD(TAG, "MQTT status updating");
        char topic[50];
        snprintf(topic, sizeof topic, "/%s/module/state", mqtt_module_config.host);
        int msg_id = esp_mqtt_client_publish(client, topic, "OK", 0, 0, 0);
        ESP_LOGD(TAG, "MQTT status updated");
    }
    vTaskDelete(NULL);
}

static esp_err_t start_mqtt_client() {
    if (client != NULL) {
        ESP_LOGD(TAG, "Stop existing client");
        esp_mqtt_client_stop(client);
    }
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_module_config.broker
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_err_t ret = esp_mqtt_client_start(client);
    if (ret == ESP_OK) {
        subscribe_to_gpio();
        if (health_status_task_handle == NULL) {
            xTaskCreate(&health_status_task, "health_status_task", 10000, NULL, 5, NULL);
        }
    }
    return ret;
}

cJSON * get_mqtt_config() {
    if (mqtt_module_config.broker == NULL) {
        mqtt_module_config = init_with_default_config();
    }
    strncpy(mqtt_module_config.broker, "mqtt://host", 50);
    strncpy(mqtt_module_config.host, "iot-module", 50);

    cJSON *config = cJSON_CreateObject();
    cJSON_AddNumberToObject(config, "enable", mqtt_module_config.enable);
    cJSON_AddStringToObject(config, "broker", mqtt_module_config.broker);
    cJSON_AddStringToObject(config, "host", mqtt_module_config.host);
    return config;
}


