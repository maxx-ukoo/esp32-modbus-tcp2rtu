#include "mqtt.h"
#ifdef __cplusplus
extern "C"
{
#endif
    #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
    #include "esp_log.h"
#ifdef __cplusplus
}
#endif
#include "gpio/gpio.h"
#include "curtains/curtains.h"
#include "time_utils.h"

static const char *TAG = "MQTT Tag";

esp_mqtt_client_handle_t IOTMqtt::client = NULL;
char *IOTMqtt::mqtt2gpio_topic = NULL;
char *IOTMqtt::mqtt2curtains_topic = NULL;
xQueueHandle IOTMqtt::gpio2mqtt_queue = NULL;
xQueueHandle IOTMqtt::mqtt2gpio_queue = NULL;
TaskHandle_t IOTMqtt::health_status_task_handle = NULL;
mqtt_config_t IOTMqtt::mqtt_module_config;

mqtt_config_t IOTMqtt::init_with_default_config()
{
    mqtt_config_t config = {
        .enable = false,
        .broker = (char *)malloc(sizeof(char) * (50)),
        .host = (char *)malloc(sizeof(char) * (50)),
    };
    return config;
}

esp_err_t IOTMqtt::mqtt_json_init(cJSON *config)
{

    ESP_LOGD(TAG, "Init MQTT from JSON");
    if (mqtt_module_config.broker == NULL)
    {
        mqtt_module_config = init_with_default_config();
    }
    mqtt_module_config.enable = cJSON_GetObjectItem(config, "enable")->valueint;
    strncpy(mqtt_module_config.broker, cJSON_GetObjectItem(config, "broker")->valuestring, 50);
    strncpy(mqtt_module_config.host, cJSON_GetObjectItem(config, "host")->valuestring, 50);
    ESP_LOGI(TAG, "mqtt_init, host config=%s", mqtt_module_config.host);
    ESP_LOGI(TAG, "mqtt_init, brocker config=%s", mqtt_module_config.broker);

    if (mqtt2curtains_topic == NULL) {
        mqtt2curtains_topic = (char *)malloc(sizeof(char) * (50));
    }
    snprintf(mqtt2curtains_topic, 50, "/%s/curtains/command", mqtt_module_config.host);
    if (mqtt2gpio_topic == NULL) {
        mqtt2gpio_topic = (char *)malloc(sizeof(char) * (50));
    }
    snprintf(mqtt2gpio_topic, 50, "/%s/gpio/command", mqtt_module_config.host);
    if (mqtt_module_config.enable == 1)
    {
        ESP_LOGD(TAG, "MQTT enabled in json, try to start mqtt client");
        return start_mqtt_client();
    }
    return ESP_OK;
}

esp_err_t IOTMqtt::mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    //char topic[50];
    switch (event->event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, mqtt2gpio_topic, 0);
            ESP_LOGI(TAG, "subscribe successful, topic=%s, msg_id=%d", mqtt2gpio_topic, msg_id);
            msg_id = esp_mqtt_client_subscribe(client, mqtt2curtains_topic, 0);
            ESP_LOGI(TAG, "subscribe successful, topic=%s, msg_id=%d", mqtt2curtains_topic, msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA: {
            decode_mqtt_message(event);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
        }
    }
    return ESP_OK;
}

void IOTMqtt::decode_mqtt_message(esp_mqtt_event_handle_t event) {
    ESP_LOGD(TAG, "MQTT message received");
    printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    printf("DATA=%.*s\r\n", event->data_len, event->data);
    cJSON *data = cJSON_Parse(event->data);
    if (data == NULL) {
        return;
    }
    if (strstr(event->topic, "/gpio/command") != NULL) {
        ESP_LOGD(TAG, "Decode gpio message");
        int pin = cJSON_GetObjectItem(data, "pin")->valueint;
        int state = cJSON_GetObjectItem(data, "state")->valueint;
        ESP_LOGI(TAG, "MQTT2GPIO_EVENT, pinn=%d, state=%d", pin, state);
        pin_state_msg_t msg = {
            .pin = pin,
            .state = state
        };
        if (xQueueSend(mqtt2gpio_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE)
        {
            ESP_LOGE(TAG, "send pin command message failed or timeout");
        }
    }
    if (strstr(event->topic, "/curtains/command") != NULL) {
        ESP_LOGD(TAG, "Decode curtains message");
            int curtain = cJSON_GetObjectItem(data, "curtain")->valueint;
            int command = cJSON_GetObjectItem(data, "command")->valueint;
            int param1 = 0;
            if (cJSON_GetObjectItem(data, "param1") != NULL) {
                param1 = cJSON_GetObjectItem(data, "param1")->valueint;
            }
            int param2 = 0;
            if (cJSON_GetObjectItem(data, "param2") != NULL) {
                param2 = cJSON_GetObjectItem(data, "param2")->valueint;
            }
            ESP_LOGD(TAG, "Decode curtains message => decoded");
            curtains_command_msg_t msg = {
                .id = curtain,
                .command = command,
                .param1 = param1,
                .param2 = param2
            };
            if (xQueueSend(IOTCurtains::curtains_command2executor_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGE(TAG, "send curtains command message failed or timeout");
            }
    }
    cJSON_Delete(data);
}

void IOTMqtt::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, esp_mqtt_event_handle_t event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void IOTMqtt::gpio2mqtt_task()
{
    pin_state_msg_t msg;
    for (;;)
    {
        if (gpio2mqtt_queue != NULL)
        {
            if (xQueueReceive(gpio2mqtt_queue, &msg, portMAX_DELAY))
            {
                ESP_LOGD(TAG, "MQTT received GPIO[%d] intr, val:%d", msg.pin, msg.state);
                char topic[50];
                char data[10];
                snprintf(topic, sizeof topic, "/%s/gpio/state/%d", mqtt_module_config.host, msg.pin);
                snprintf(data, sizeof data, "%d", msg.state);
                int msg_id = esp_mqtt_client_publish(client, topic, data, 0, 0, 0);
                ESP_LOGI(TAG, "sent publish successful, topic=%s, msg_id=%d", topic, msg_id);
            }
        }
        else
        {
            vTaskDelay(50);
        }
    }
}

esp_err_t IOTMqtt::subscribe_to_gpio()
{
    if (gpio2mqtt_queue == NULL || mqtt2gpio_queue == NULL)
    {
        gpio2mqtt_queue = xQueueCreate(5, sizeof(pin_state_msg_t));
        mqtt2gpio_queue = xQueueCreate(5, sizeof(pin_state_msg_t));
        BaseType_t ret = xTaskCreate((TaskFunction_t)gpio2mqtt_task, "gpio2mqtt_task", 4096, NULL, 5, NULL);
        if (ret != pdTRUE)
        {
            ESP_LOGE(TAG, "create gpio2mqtt_task task failed");
            return ESP_FAIL;
        }
    }

    IOTGpio::set_mqtt_gpio_evt_queue(gpio2mqtt_queue, mqtt2gpio_queue);
    return ESP_OK;
}

void IOTMqtt::health_status_task(void *pvParameter)
{
    const TickType_t xDelay30s = pdMS_TO_TICKS(30000);
    while (1)
    {
        vTaskDelay(xDelay30s);
        char* str = get_current_local_time();
        ESP_LOGD(TAG, "MQTT status updating at time %s", str);
        free(str);
        char topic[50];

        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "status", esp_get_free_heap_size());
        cJSON *res = IOTGpio::getGpioState();
        cJSON_AddItemToObject(root, "gpio", res);

        const char *status_body = cJSON_Print(root);
        snprintf(topic, sizeof topic, "/%s/module/state", mqtt_module_config.host);
        esp_mqtt_client_publish(client, topic, status_body, 0, 0, 0);
        cJSON_Delete(root);
        free((void *)status_body);
        ESP_LOGD(TAG, "MQTT status updated");
    }
    vTaskDelete(NULL);
}

esp_err_t IOTMqtt::start_mqtt_client()
{
    if (client != NULL)
    {
        ESP_LOGD(TAG, "Stop existing client");
        esp_mqtt_client_stop(client);
    }
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_module_config.broker};
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t) ESP_EVENT_ANY_ID, (esp_event_handler_t) mqtt_event_handler, client);
    esp_err_t ret = esp_mqtt_client_start(client);
    if (ret == ESP_OK)
    {
        subscribe_to_gpio();
        if (health_status_task_handle == NULL)
        {
            xTaskCreate(&health_status_task, "health_status_task", 10000, NULL, 5, NULL);
        }
    }
    return ret;
}

cJSON *IOTMqtt::get_mqtt_config()
{
    if (mqtt_module_config.broker == NULL)
    {
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
