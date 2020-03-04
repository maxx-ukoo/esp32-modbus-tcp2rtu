#include "gpio.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

static const char *GPIO_TAG = "GPIO Tag";

static int gpioConfig[GPIO_NUMBER][5] = {
    {2, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {4, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {5, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {12, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {13, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {14, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {15, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {18, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {23, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {32, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {33, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM},
    {34, -1, 0, 0, MODE_INPUT},
    {35, -1, 0, 0, MODE_INPUT},
    {36, -1, 0, 0, MODE_INPUT},
    {39, -1, 0, 0, MODE_INPUT}
};

static xQueueHandle gpio_evt_queue = NULL;
static TaskHandle_t gpio_task_xHandle = NULL;
static TaskHandle_t mqtt2gpio_task_xHandle = NULL;

static esp_err_t fade_service_status = ESP_FAIL;
static esp_err_t esr_service_status = ESP_FAIL;
static xQueueHandle gpio2mqtt_queue = NULL;
static xQueueHandle mqtt2gpio_queue = NULL;


static ledc_channel_config_t ledc_channel = 
        {
            .channel    = LEDC_CHANNEL_0,
            .duty       = 1023,
            .gpio_num   = 0,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER_0
        };

ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
    .freq_hz = 64000,                      // frequency of PWM signal
    .speed_mode = LEDC_HIGH_SPEED_MODE,   // timer mode
    .timer_num = LEDC_TIMER_0,            // timer index
    .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
};

//ledc_timer_config(&ledc_timer);

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void mqtt2gpio_task(void* arg)
{
    pin_state_msg_t msg;
    for(;;) {
        if (mqtt2gpio_queue != NULL) {
            if(xQueueReceive(mqtt2gpio_queue, &msg, portMAX_DELAY)) {
                ESP_LOGD(GPIO_TAG, "MQTT2GPIO received GPIO[%d] intr, val:%d", msg.pin, msg.state);
                setPinState(msg.pin, msg.state);
            }
        } else {
            vTaskDelay(50);
        }
    }
}

static int getPinIndex(int id) {
    for (int i = 0; i < GPIO_NUMBER; ++i) {
        if (gpioConfig[i][ID] == id)
            return i;
    }
    return -1;
}

esp_err_t setPinState(int pin, int state) {
    ESP_LOGI(GPIO_TAG, "Set gpio state: pin = %d, level = %d", pin, state);
    int idx = getPinIndex(pin);
    if (idx == -1) {
        return ESP_FAIL;
    }
    if (gpioConfig[idx][MODE] == MODE_OUTPUT) {
        gpio_set_level(pin, state);
        return ESP_OK;
    } else if (gpioConfig[idx][MODE] == MODE_PWM) {
        ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, state);
        ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        return ESP_OK;
    }

    return ESP_FAIL;
}

void set_mqtt_gpio_evt_queue(xQueueHandle gpio2mqtt_queue_handler, xQueueHandle mqtt2gpio_queue_handler){
    gpio2mqtt_queue = gpio2mqtt_queue_handler;
    mqtt2gpio_queue = mqtt2gpio_queue_handler;
    if (mqtt2gpio_task_xHandle == NULL) {
        ESP_LOGD(GPIO_TAG, "GPIO TASK creating");
        mqtt2gpio_task_xHandle = xTaskCreate(mqtt2gpio_task, "mqtt2gpio_task", 2048, NULL, 10, NULL);
    }
}

static void gpio_task(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGD(GPIO_TAG, "GPIO[%d] intr, val:%d", io_num, gpio_get_level(io_num));
            //printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
            if (gpio2mqtt_queue != NULL) {
                pin_state_msg_t msg = {
                    .pin = io_num,
                    .state = gpio_get_level(io_num)
                };
                if (xQueueSend(gpio2mqtt_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
                    ESP_LOGE(GPIO_TAG, "send pin state message failed or timeout");
                }                
            }
        }
    }
}

static esp_err_t gpioInit() {
    gpio_config_t io_conf;
    if (gpio_evt_queue == NULL) {
        ESP_LOGD(GPIO_TAG, "GPIO QUEUE created");
        gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    }
    if (gpio_task_xHandle == NULL) {
        ESP_LOGD(GPIO_TAG, "GPIO TASK created");
        gpio_task_xHandle = xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    }
    ESP_LOGD(GPIO_TAG, "esr_service_status: %d", esr_service_status);
    if (esr_service_status != ESP_OK) {
        ESP_LOGD(GPIO_TAG, "instaling isr service");
        esr_service_status = gpio_install_isr_service(0);
        ESP_LOGD(GPIO_TAG, "installed");
    }

    if (fade_service_status != ESP_OK) {
        fade_service_status = ledc_fade_func_install(1);
    }

    for (int i = 0; i < GPIO_NUMBER; ++i) {
        ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as %d", gpioConfig[i][ID], gpioConfig[i][MODE]);
        gpio_isr_handler_remove(gpioConfig[i][ID]);
        if (gpioConfig[i][MODE] > 0) {
            if (gpioConfig[i][MODE] == MODE_INPUT) {
                io_conf.intr_type = GPIO_INTR_ANYEDGE;
                io_conf.mode = GPIO_MODE_INPUT;
            }
            if (gpioConfig[i][MODE] == MODE_OUTPUT) {
                io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
                io_conf.mode = GPIO_MODE_OUTPUT;
            }
            if ((gpioConfig[i][MODE] & (MODE_OUTPUT | MODE_INPUT)) != 0) {
                io_conf.pin_bit_mask = (1ULL<<gpioConfig[i][ID]);
                io_conf.pull_down_en = gpioConfig[i][PULL_D];
                io_conf.pull_up_en = gpioConfig[i][PULL_U];
                gpio_config(&io_conf);
                gpio_isr_handler_add(gpioConfig[i][ID], gpio_isr_handler, (void*) gpioConfig[i][ID]);
            }
            if (gpioConfig[i][MODE] == MODE_PWM) {
                ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as PWM", gpioConfig[i][ID]);
                ledc_timer_config(&ledc_timer);
                ledc_channel.gpio_num = gpioConfig[i][ID];
                ledc_channel_config(&ledc_channel);
                ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as PWM DONE", gpioConfig[i][ID]);
            }

            
        }
    }
    ESP_LOGD(GPIO_TAG, "All pins configured");
    return ESP_OK;
}

static bool configurePin(int id, int mode, int pull_up, int pull_down) {
    ESP_LOGD(GPIO_TAG, "configurePin: id=%d", id);
    int idx = getPinIndex(id);
    ESP_LOGD(GPIO_TAG, "configurePin: idx=%d", idx);
    if (idx == -1) {
        return false;
    }
    ESP_LOGD(GPIO_TAG, "configurePin: mode=%d", mode);
    ESP_LOGD(GPIO_TAG, "configurePin: max_mode=%d", gpioConfig[idx][SUPPORTED_MODES]);
    if ((mode & gpioConfig[idx][SUPPORTED_MODES]) == 0) {
        return false;
    }
    gpioConfig[idx][MODE] = mode;
    gpioConfig[idx][PULL_D] = pull_down;
    gpioConfig[idx][PULL_U] = pull_up;
    return true;
}

int gpioInitFromJson(cJSON *pins) {
    const cJSON *pin = NULL;
    int status = -1;
    //ESP_LOGI(TAG, "Init gpio from JSON"Partition size: total: %d, used: %d", total, used");

    ESP_LOGD(GPIO_TAG, "Init gpio from JSON");
    cJSON_ArrayForEach(pin, pins)
    {
        int id = cJSON_GetObjectItemCaseSensitive(pin, "id")->valueint;
        ESP_LOGD(GPIO_TAG, "Init gpio, configuring pin %d", id);
        int mode = cJSON_GetObjectItemCaseSensitive(pin, "mode")->valueint;
        ESP_LOGD(GPIO_TAG, "Init gpio, mode: %d", mode);
        int pull_up = cJSON_GetObjectItemCaseSensitive(pin, "pull_up")->valueint;
        ESP_LOGD(GPIO_TAG, "Init gpio, pull_up: %d", pull_up);
        int pull_down = cJSON_GetObjectItemCaseSensitive(pin, "pull_down")->valueint;
        ESP_LOGD(GPIO_TAG, "Init gpio, pull_down: %d", pull_down);
        if (!configurePin(id, mode, pull_up, pull_down)) {
            ESP_LOGD(GPIO_TAG, "Init gpio, pin config Error");
            status = id;
            goto end;
        }
        ESP_LOGD(GPIO_TAG, "Init gpio, pin config OK");
    }
    gpioInit();    
end:
    return status;    
}

cJSON * getGpioConfig() {
    cJSON *pin = NULL;
    cJSON *pin_item1 = NULL;
    cJSON *pin_item2 = NULL;
    cJSON *pin_item3 = NULL;
    cJSON *pin_item4 = NULL;
    cJSON *gpio = cJSON_CreateArray();
    if (gpio == NULL)
    {
        goto end;
    }
    for (int i = 0; i < GPIO_NUMBER; ++i) {
        ESP_LOGE(GPIO_TAG, "Creating config for pin %d", gpioConfig[i][ID]);
        pin = cJSON_CreateObject();
        if (pin == NULL)
        {
            goto end;
        }
        cJSON_AddItemToArray(gpio, pin);
        pin_item1 = cJSON_CreateNumber(gpioConfig[i][ID]);
        if (pin_item1 == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(pin, "id", pin_item1);
        pin_item2 = cJSON_CreateNumber(gpioConfig[i][MODE]);
        if (pin_item2 == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(pin, "mode", pin_item2);
        pin_item3 = cJSON_CreateNumber(gpioConfig[i][PULL_U]);
        if (pin_item3 == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(pin, "pull_up", pin_item3);
        pin_item4 = cJSON_CreateNumber(gpioConfig[i][PULL_D]);
        if (pin_item4 == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(pin, "pull_down", pin_item4);
    }
    return gpio;
end:
    ESP_LOGE(GPIO_TAG, "Error on creating gpio config");
    cJSON_Delete(gpio);
    return NULL;
}

cJSON * setPinState(int pin, int state) {
    ESP_LOGI(GPIO_TAG, "Update gpio state: pin = %d, level = %d", pin, state);
    int idx = getPinIndex(pin);
    cJSON *root = cJSON_CreateObject();
    if (idx == -1) {
        cJSON_AddNumberToObject(root, "error_no_pin", 1);
    }
    cJSON_AddNumberToObject(root, "pin", pin);
    if (gpioConfig[idx][MODE] == MODE_OUTPUT) {
        gpio_set_level(pin, state);
        cJSON_AddNumberToObject(root, "state", state);
    } else if (gpioConfig[idx][MODE] == MODE_PWM) {
        //ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, state);
        //ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
        ledc_set_fade_with_time(ledc_channel.speed_mode, ledc_channel.channel, state, 1000);
        ledc_fade_start(ledc_channel.speed_mode,ledc_channel.channel, LEDC_FADE_NO_WAIT);
    } else {
        cJSON_AddNumberToObject(root, "error_wrong_mode", 1);
    }

    return root;
}

cJSON * getPinState(int pin) {
    int idx = getPinIndex(pin);
    cJSON *root = cJSON_CreateObject();
    if (idx == -1) {
        cJSON_AddNumberToObject(root, "error_no_pin", 1);
    }
    cJSON_AddNumberToObject(root, "pin", pin);
    int state = -1;
    if (gpioConfig[idx][MODE] != 0) {
        cJSON_AddNumberToObject(root, "error_wrong_mode", 1);
    }

    if (gpioConfig[idx][MODE] == MODE_OUTPUT) {
        state = gpio_get_level(pin);
    } else if (gpioConfig[idx][MODE] == MODE_PWM) {
        state = ledc_get_duty(ledc_channel.speed_mode, ledc_channel.channel);
    } else {
        cJSON_AddNumberToObject(root, "error_wrong_mode", 1);
    }

    cJSON_AddNumberToObject(root, "state", state);
    return root;
}

