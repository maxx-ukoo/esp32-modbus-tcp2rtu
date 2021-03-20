#include "gpio.h"
#ifdef __cplusplus
extern "C"
{
#endif
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#ifdef __cplusplus
}
#endif



static const char *GPIO_TAG = "GPIO Tag";

int IOTGpio::gpioConfig[GPIO_NUMBER][6] = {
                {2, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {4, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {5, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {12, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {13, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {14, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {15, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {18, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {23, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {32, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {33, -1, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0},
                {34, -1, 0, 0, MODE_INPUT, 0},
                {35, -1, 0, 0, MODE_INPUT, 0},
                {36, -1, 0, 0, MODE_INPUT, 0},
                {39, -1, 0, 0, MODE_INPUT, 0}
        };

ledc_channel_config_t IOTGpio::ledc_channel1 = {
            .gpio_num   = 0,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .channel    = LEDC_CHANNEL_0,
            .timer_sel  = LEDC_TIMER_1,
            .duty       = 1023,
            .hpoint     = 0 
        };
ledc_channel_config_t IOTGpio::ledc_channel2 =  {
            .gpio_num   = 0,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .channel    = LEDC_CHANNEL_1,
            .timer_sel  = LEDC_TIMER_1,
            .duty       = 1023,
            .hpoint     = 0 
        };        
ledc_timer_config_t IOTGpio::ledc_timer1 = {
            .speed_mode = LEDC_HIGH_SPEED_MODE,   // timer mode
            .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
            .timer_num = LEDC_TIMER_1,            // timer index
            .freq_hz = 64000,                      // frequency of PWM signal
            .clk_cfg = LEDC_AUTO_CLK             // Auto select the source clock
        };        

xQueueHandle IOTGpio::gpio_evt_queue = NULL;
TaskHandle_t IOTGpio::gpio_task_xHandle = NULL;
TaskHandle_t IOTGpio::mqtt2gpio_task_xHandle = NULL;
esp_err_t IOTGpio::fade_service_status = ESP_FAIL;
esp_err_t IOTGpio::esr_service_status = ESP_FAIL;
xQueueHandle IOTGpio::gpio2mqtt_queue = NULL;
xQueueHandle IOTGpio::mqtt2gpio_queue = NULL;

IOTGpio::IOTGpio(cJSON *config)
{
    gpio_json_init(config, true);
}

esp_err_t IOTGpio::gpio_json_init(cJSON *gpio, bool checkReservedPins)
{
    const cJSON *pin = NULL;
    ESP_LOGD(GPIO_TAG, "Init gpio from JSON");
    cJSON_ArrayForEach(pin, gpio)
    {
        int gpioPin = cJSON_GetObjectItemCaseSensitive(pin, "id")->valueint;
        ESP_LOGD(GPIO_TAG, "Init gpio, configuring pin %d", gpioPin);
        int mode = cJSON_GetObjectItemCaseSensitive(pin, "mode")->valueint;
        ESP_LOGD(GPIO_TAG, "Init gpio, mode: %d", mode);
        int pull_up = cJSON_GetObjectItemCaseSensitive(pin, "pull_up")->valueint;
        ESP_LOGD(GPIO_TAG, "Init gpio, pull_up: %d", pull_up);
        int pull_down = cJSON_GetObjectItemCaseSensitive(pin, "pull_down")->valueint;
        ESP_LOGD(GPIO_TAG, "Init gpio, pull_down: %d", pull_down);
        if (checkReservedPins == true) {
            int idx = getPinIndex(gpioPin);
            if (idx == -1) {
                return ESP_FAIL;
            }   
            ESP_LOGW(GPIO_TAG, "Init gpio, pin check failed with current mode: %d", ((MODE_MODBUS + MODE_I2C) & gpioConfig[idx][MODE]));
            if (((MODE_MODBUS + MODE_I2C) & gpioConfig[idx][MODE]) > 0 && gpioConfig[idx][MODE] != -1) {
                ESP_LOGW(GPIO_TAG, "Init gpio, pin check failed with current mode: %d", gpioConfig[idx][MODE]);
                continue;
            }
        }
        if (!configure_pin(gpioPin, mode, pull_up, pull_down)) {
            ESP_LOGD(GPIO_TAG, "Init gpio, pin config Error");
            return ESP_FAIL;
        }
        ESP_LOGD(GPIO_TAG, "Init gpio, pin config OK");
    }
    gpio_start();    
    return ESP_OK;    
}

esp_err_t IOTGpio::gpio_start() {
    gpio_config_t io_conf;
    if (gpio_evt_queue == NULL) {
        ESP_LOGD(GPIO_TAG, "GPIO QUEUE created");
        gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    }
    if (gpio_task_xHandle == NULL) {
        ESP_LOGD(GPIO_TAG, "GPIO TASK created");
        gpio_task_xHandle = (TaskHandle_t)xTaskCreate((TaskFunction_t)gpio_task, "gpio_task", 4096, NULL, 10, NULL);
    }
    ESP_LOGD(GPIO_TAG, "esr_service_status: %d", esr_service_status);
    if (esr_service_status != ESP_OK) {
        ESP_LOGD(GPIO_TAG, "instaling isr service");
        esr_service_status = gpio_install_isr_service(0);
        ESP_LOGD(GPIO_TAG, "installed");
    }

    if (fade_service_status != ESP_OK) {
        fade_service_status = ledc_fade_func_install(1);
        ESP_LOGD(GPIO_TAG, "Fade service installed");
    }

    int pwm_count = 0;
    for (int i = 0; i < GPIO_NUMBER; ++i) {
        ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as %d", gpioConfig[i][ID], gpioConfig[i][MODE]);
        gpio_isr_handler_remove((gpio_num_t)gpioConfig[i][ID]);
        if (gpioConfig[i][MODE] > 0) {
            if (gpioConfig[i][MODE] == MODE_INPUT) {
                io_conf.intr_type = GPIO_INTR_ANYEDGE;
                io_conf.mode = GPIO_MODE_INPUT;
            }
            if (gpioConfig[i][MODE] == MODE_OUTPUT) {
                io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
                io_conf.mode = GPIO_MODE_OUTPUT;
            }
            if ((gpioConfig[i][MODE] & (MODE_OUTPUT | MODE_INPUT)) != 0) {
                io_conf.pin_bit_mask = (1ULL<<gpioConfig[i][ID]);
                io_conf.pull_down_en = (gpio_pulldown_t)gpioConfig[i][PULL_D];
                io_conf.pull_up_en = (gpio_pullup_t)gpioConfig[i][PULL_U];
                gpio_config(&io_conf);
                gpio_isr_handler_add((gpio_num_t)gpioConfig[i][ID], gpio_isr_handler, (void*) gpioConfig[i][ID]);
            }
            if (gpioConfig[i][MODE] == MODE_PWM) {
                ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as PWM", gpioConfig[i][ID]);
                if (pwm_count == 0) {
                    ledc_timer_config(&ledc_timer1);
                    ledc_channel1.gpio_num = gpioConfig[i][ID];
                    ledc_channel_config(&ledc_channel1);
                    gpioConfig[i][PWM_CHANNEL] = 1;
                    pwm_count++;
                } else {
                    ledc_timer_config(&ledc_timer1);
                    ledc_channel2.gpio_num = gpioConfig[i][ID];
                    ledc_channel_config(&ledc_channel2);
                    gpioConfig[i][PWM_CHANNEL] = 2;
                }
                ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as PWM DONE", gpioConfig[i][ID]);
            }
        }
    }
    ESP_LOGD(GPIO_TAG, "All pins configured");
    return ESP_OK;
}

void IRAM_ATTR IOTGpio::gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void IOTGpio::mqtt2gpio_task(void* arg)
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

void IOTGpio::set_mqtt_gpio_evt_queue(xQueueHandle gpio2mqtt_queue_handler, xQueueHandle mqtt2gpio_queue_handler)
{
    gpio2mqtt_queue = gpio2mqtt_queue_handler;
    mqtt2gpio_queue = mqtt2gpio_queue_handler;
    if (mqtt2gpio_task_xHandle == NULL) {
        ESP_LOGD(GPIO_TAG, "GPIO TASK creating");
        mqtt2gpio_task_xHandle = (TaskHandle_t)xTaskCreate((TaskFunction_t)mqtt2gpio_task, "mqtt2gpio_task", 4096, NULL, 10, NULL);
    }
}

void IOTGpio::sendIoState(int io_num, int io_state) {
    if (gpio2mqtt_queue != NULL) {
        pin_state_msg_t msg = {
            .pin = io_num,
            .state = io_state
        };
        ESP_LOGD(GPIO_TAG, "GPIO[%d] intr message sending, val:%d", io_num, io_state);
        if (xQueueSend(gpio2mqtt_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGE(GPIO_TAG, "send pin state message failed or timeout");
        }  
    }
}

void IOTGpio::gpio_task(void* arg)
{
    int io_num;
    int delay = portMAX_DELAY;

    int last_io_num = -1;
    int last_io_state = 0;
    for(;;) {
        if (pdTRUE == xQueueReceive(gpio_evt_queue, &io_num, delay)) {
            ESP_LOGD(GPIO_TAG, "GPIO[%d] intr, val:%d", io_num, gpio_get_level((gpio_num_t)io_num));
            int current_state = gpio_get_level((gpio_num_t)io_num);
            if (last_io_num != -1) {
                if (last_io_num != io_num) { // pin chaged, send last state for last pin 
                    ESP_LOGD(GPIO_TAG, "GPIO[%d] intr message if1, val:%d", io_num, current_state);
                    sendIoState(last_io_num, last_io_state);
                    last_io_num = io_num;
                    last_io_state = current_state;
                } else if (last_io_state != current_state) { // state changed, send old state
                    ESP_LOGD(GPIO_TAG, "GPIO[%d] intr message if2, val:%d", io_num, current_state);
                    sendIoState(last_io_num, last_io_state);
                    last_io_num = io_num;
                    last_io_state = current_state;
                } else {
                    // the same state for the same pin, skip
                }
            } else {
                ESP_LOGD(GPIO_TAG, "GPIO[%d] intr message if3, val:%d", io_num, current_state);
                last_io_num = io_num;
                last_io_state = current_state;
                sendIoState(last_io_num, last_io_state); // send state to avoid delay
            }
            delay = 50;
        } else {
            // timeout, state already processed
            last_io_num = -1;
            delay = portMAX_DELAY;
        }
    }
}


int IOTGpio::getPinIndex(int id)
{
    for (int i = 0; i < GPIO_NUMBER; ++i) {
        if (gpioConfig[i][ID] == id)
            return i;
    }
    return -1;
}

bool IOTGpio::configure_pin(int gpioPin, int mode, int pull_up, int pull_down) {
    ESP_LOGD(GPIO_TAG, "configurePin: gpioPin=%d", gpioPin);
    int idx = getPinIndex(gpioPin);
    ESP_LOGD(GPIO_TAG, "configurePin: idx=%d", idx);
    if (idx == -1) {
        return false;
    }
    ESP_LOGD(GPIO_TAG, "configurePin: mode=%d", mode);
    ESP_LOGD(GPIO_TAG, "configurePin: max_mode=%d", gpioConfig[idx][SUPPORTED_MODES]);
    if ((mode & gpioConfig[idx][SUPPORTED_MODES]) == 0) {
        if (mode == 0) {
            gpioConfig[idx][MODE] = 0;
        } else {
            gpioConfig[idx][MODE] = gpioConfig[idx][MODE];
        }
        gpioConfig[idx][PULL_D] = 0;
        gpioConfig[idx][PULL_U] = 0;
        return true;
    }
    gpioConfig[idx][MODE] = mode;
    gpioConfig[idx][PULL_D] = pull_down;
    gpioConfig[idx][PULL_U] = pull_up;
    return true;
}

void IOTGpio::reservePin(int gpioPin, int mode)
{
    int idx = getPinIndex(gpioPin);
    if (idx == -1) {
        return;
    }
    gpioConfig[idx][MODE] = mode;
    gpioConfig[idx][PULL_D] = 0;
    gpioConfig[idx][PULL_U] = 0;
}

void IOTGpio::freePin(int gpioPin)
{
    int idx = getPinIndex(gpioPin);
    if (idx == -1) {
        return;
    }
    gpioConfig[idx][MODE] = -1;
    gpioConfig[idx][PULL_D] = 0;
    gpioConfig[idx][PULL_U] = 0;
}

cJSON * IOTGpio::get_gpio_config()
{
    cJSON *pin = NULL;
    cJSON *pin_item1 = NULL;
    cJSON *pin_item2 = NULL;
    cJSON *pin_item3 = NULL;
    cJSON *pin_item4 = NULL;
    cJSON *gpio = cJSON_CreateArray();
    if (gpio == NULL)
    {
        return NULL;
    }
    for (int i = 0; i < GPIO_NUMBER; ++i) {
        ESP_LOGE(GPIO_TAG, "Creating config for pin %d, mode: %d", gpioConfig[i][ID], gpioConfig[i][MODE]);
        pin = cJSON_CreateObject();
        if (pin == NULL)
        {
            return NULL;
        }
        cJSON_AddItemToArray(gpio, pin);
        pin_item1 = cJSON_CreateNumber(gpioConfig[i][ID]);
        if (pin_item1 == NULL)
        {
            return NULL;
        }
        cJSON_AddItemToObject(pin, "id", pin_item1);
        pin_item2 = cJSON_CreateNumber(gpioConfig[i][MODE]);
        if (pin_item2 == NULL)
        {
            return NULL;
        }
        cJSON_AddItemToObject(pin, "mode", pin_item2);
        pin_item3 = cJSON_CreateNumber(gpioConfig[i][PULL_U]);
        if (pin_item3 == NULL)
        {
            return NULL;
        }
        cJSON_AddItemToObject(pin, "pull_up", pin_item3);
        pin_item4 = cJSON_CreateNumber(gpioConfig[i][PULL_D]);
        if (pin_item4 == NULL)
        {
            return NULL;
        }
        cJSON_AddItemToObject(pin, "pull_down", pin_item4);
    }
    return gpio;
}

int IOTGpio::readPinState(int pin)
{
    int idx = getPinIndex(pin);
    if (idx == -1) {
        return -1;
    }
    if (gpioConfig[idx][MODE] == MODE_OUTPUT) {
        return gpio_get_level((gpio_num_t)pin);
    } else if (gpioConfig[idx][MODE] == MODE_PWM) {
        if (gpioConfig[idx][PWM_CHANNEL] == 1) {
            return ledc_get_duty(ledc_channel1.speed_mode, ledc_channel1.channel);
        } else {
            return ledc_get_duty(ledc_channel2.speed_mode, ledc_channel2.channel);
        }
    } else if (gpioConfig[idx][MODE] == MODE_INPUT) {
        return gpio_get_level((gpio_num_t)pin);
    }
    return -2;
}

cJSON * IOTGpio::get_json_pin_state(int gpioPin)
{
    int idx = getPinIndex(gpioPin);
    cJSON *root = cJSON_CreateObject();
    if (idx == -1) {
        cJSON_AddNumberToObject(root, "error_no_pin", 1);
        return root;
    }
    cJSON *json_pin = cJSON_CreateObject();
    char pinName[10];
    snprintf(pinName, sizeof pinName, "pin%d", gpioPin);
    cJSON_AddItemToObject(root, pinName, json_pin);    
    int state = readPinState(gpioPin);
    if (state == -1) {
        cJSON_AddNumberToObject(json_pin, "error_no_pin", 1);
    } else if (state == -2) {
        cJSON_AddNumberToObject(json_pin, "error_wrong_mode", 1);
    } else {
        cJSON_AddNumberToObject(json_pin, "state", state);
        cJSON_AddNumberToObject(json_pin, "mode", gpioConfig[idx][MODE]);
    }
    return root;
}

cJSON * IOTGpio::getGpioState()
{
    cJSON *gpio = cJSON_CreateArray();
    if (gpio == NULL)
    {
        ESP_LOGE(GPIO_TAG, "Error on creating gpio config");
        return NULL;
    }
    for (int i = 0; i < GPIO_NUMBER; ++i) {
        int pin_state = readPinState(gpioConfig[i][ID]);
        if (pin_state > -1) {
            cJSON *json_pin_state = get_json_pin_state(gpioConfig[i][ID]);
            cJSON_AddItemToArray(gpio, json_pin_state);
        }
    }
    return gpio;
}

esp_err_t IOTGpio::setPinState(int pin, int state)
{
    ESP_LOGI(GPIO_TAG, "Set gpio state: pin = %d, level = %d", pin, state);
    int idx = getPinIndex(pin);
    if (idx == -1) {
        return ESP_FAIL;
    }
    if (gpioConfig[idx][MODE] == MODE_OUTPUT) {
        gpio_set_level((gpio_num_t)pin, state);
        return ESP_OK;
    } else if (gpioConfig[idx][MODE] == MODE_PWM) {
        //ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, state);
        //ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);

        ESP_LOGD(GPIO_TAG, "Duty for channel 1 before update %d", ledc_get_duty(ledc_channel1.speed_mode, ledc_channel1.channel));
        ESP_LOGD(GPIO_TAG, "Duty for channel 2 before update %d", ledc_get_duty(ledc_channel2.speed_mode, ledc_channel2.channel));
        if (gpioConfig[idx][PWM_CHANNEL] == 1) {
            ESP_LOGI(GPIO_TAG, "Update PWM for channel 1");
            //ledc_set_fade_step_and_start()
            //ledc_set_fade_step_and_start(ledc_channel1.speed_mode, ledc_channel1.channel, state, 1, 1, LEDC_FADE_NO_WAIT);
            ledc_set_fade_with_time(ledc_channel1.speed_mode, ledc_channel1.channel, state, 1000);
            ledc_fade_start(ledc_channel1.speed_mode,ledc_channel1.channel, LEDC_FADE_NO_WAIT);
        } else {
            ESP_LOGI(GPIO_TAG, "Update PWM for channel 2");
            ledc_set_fade_with_time(ledc_channel2.speed_mode, ledc_channel2.channel, state, 1000);
            ledc_fade_start(ledc_channel2.speed_mode,ledc_channel2.channel, LEDC_FADE_NO_WAIT);
        }
        
        ESP_LOGD(GPIO_TAG, "Duty for channel 1 after update %d", ledc_get_duty(ledc_channel1.speed_mode, ledc_channel1.channel));
        ESP_LOGD(GPIO_TAG, "Duty for channel 2 after update %d", ledc_get_duty(ledc_channel2.speed_mode, ledc_channel2.channel));
        return ESP_OK;
    }

    return ESP_FAIL;
}