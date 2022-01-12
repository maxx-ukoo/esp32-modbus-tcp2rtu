#include "gpio.h"

//#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "config/config.h"

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
#include "pcf8574.h"
#ifdef __cplusplus
}
#endif

#define I2C_ADDR 0b0100000
#define SDA_GPIO 33
#define SCL_GPIO 5


static const char *GPIO_TAG = "GPIO Tag";

int pcf8574_i2c_init_ok = 0;

int IOTGpio::extra_pins = 0;
int *IOTGpio::io_array = (int *)malloc(((GPIO_NUMBER + extra_pins) * GPIO_ARRAY_SIZE) * sizeof(int));

int IOTGpio::gpioExConfig[GPIO_EX_MAX_NUMBER][5] = {
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0}
};

i2c_dev_t IOTGpio::gpioExDevices[GPIO_EX_MAX_NUMBER] = {{0}};

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
                {34, -1, 0, 0, MODE_INPUT | MODE_EX_INT, 0},
                {35, -1, 0, 0, MODE_INPUT | MODE_EX_INT, 0},
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
SemaphoreHandle_t IOTGpio::xMutex = NULL;

IOTGpio::IOTGpio(cJSON *config)
{
    gpio_json_init(config, true);
}


int IOTGpio::getGpioConfigValue(int row, int col) {
    return io_array[row * GPIO_ARRAY_SIZE + col ];
}

void IOTGpio::setGpioConfigValue(int row, int col, int value) {
    ESP_LOGD(GPIO_TAG, "get value for [%d][%d] => %d ", row, col, row * GPIO_NUMBER + col);
    io_array[row * GPIO_ARRAY_SIZE + col ] = value;
}

esp_err_t IOTGpio::gpio_jsonextender_init(cJSON *extender_config) {
    
    int ext_int_io = cJSON_GetObjectItemCaseSensitive(extender_config, "int_io")->valueint;
    ESP_LOGD(GPIO_TAG, "Init pcf8574 interupt with io pin %d", ext_int_io);
    
     if (configure_pin(ext_int_io, MODE_EX_INT, 1, 0)) {
        gpio_start();
        ESP_LOGI(GPIO_TAG, "Init pcf8574 interrupt OK");
    } else {
        ESP_LOGE(GPIO_TAG, "Init pcf8574 interrupt error");
    }
     
    cJSON *pcf8574_list = cJSON_GetObjectItem(extender_config, "devs");

    const cJSON *pcf_config = NULL;
    extra_pins = 0;
    int current_pin = GPIO_NUMBER;
  
    //char *string = cJSON_Print(extender_config);
    ESP_LOGD(GPIO_TAG, "Got pcf configs");
    int n = 0;
    cJSON_ArrayForEach(pcf_config, pcf8574_list)
    {
        if (n  == GPIO_EX_MAX_NUMBER) {
            ESP_LOGW(GPIO_TAG, "Init pcf8574 finished due to many devices");
            return ESP_OK;
        }
        ESP_LOGD(GPIO_TAG, "Init pcf with next device");
        int exAddr = cJSON_GetObjectItemCaseSensitive(pcf_config, "ex_addr")->valueint;
        ESP_LOGD(GPIO_TAG, "Init pcf with addr %d", exAddr);
        int ioAddr = cJSON_GetObjectItemCaseSensitive(pcf_config, "io_addr")->valueint;
        ESP_LOGD(GPIO_TAG, "Init pcf with start pin %d", ioAddr);

        int ioConfig = cJSON_GetObjectItemCaseSensitive(pcf_config, "io_config")->valueint;
        ESP_LOGD(GPIO_TAG, "Init pcf with pin config %d", ioConfig);

        gpioExConfig[n][PCF_ADDR] = exAddr;
        gpioExConfig[n][PCF_IO_START_ADDR] = ioAddr;
        gpioExConfig[n][PCF_IO_CONFIG] = ioConfig;

        extra_pins = extra_pins + 8;
        io_array = (int*) realloc(io_array,((GPIO_NUMBER + extra_pins) * 6) * sizeof(int));
        int offset = 0;
        for (int p = current_pin; p < GPIO_NUMBER + extra_pins; p++ ) {
            setGpioConfigValue(p, ID, ioAddr++);
            ESP_LOGD(GPIO_TAG, "Creating array config for pin n=%d, id=%d", p, ioAddr);
            int pinConfig = (ioConfig & ( 1 << offset )) >> offset;
            if (pinConfig == 1) {
                setGpioConfigValue(p, MODE, MODE_EX_INPUT);
            } else {
                setGpioConfigValue(p, MODE, MODE_EX_OUTPUT);
            }
            setGpioConfigValue(p, PULL_U, 0);
            setGpioConfigValue(p, PULL_D, 0);
            setGpioConfigValue(p, SUPPORTED_MODES, MODE_EX_INPUT | MODE_EX_OUTPUT);
            setGpioConfigValue(p, PWM_CHANNEL, 0);
            offset++;
        }

        if (pcf8574_i2c_init_ok == 0) {
            pcf8574_i2c_init_ok = 1;
            ESP_LOGI(GPIO_TAG, "Init I2C for pcf8574");
            ESP_ERROR_CHECK(i2cdev_init());
        }
        
        ESP_LOGI(GPIO_TAG, "Init pcf8574 [%d]", n);
        i2c_dev_t pcf8574;
        memset(&pcf8574, 0, sizeof(i2c_dev_t));      

        ESP_ERROR_CHECK(pcf8574_init_desc(&pcf8574, 0, exAddr, (gpio_num_t)SDA_GPIO, (gpio_num_t)SCL_GPIO));
        gpioExDevices[n] = pcf8574;
        ESP_LOGI(GPIO_TAG, "Init pcf8574 [%d] with addr: %d", n, gpioExDevices[n].addr);
       
        pcf8574_port_write(&pcf8574, ioConfig);
        uint8_t val;

        pcf8574_port_read(&pcf8574, &val);
        gpioExConfig[n][PCF_IO_LAST_VALUE] = val;
        n++;
        current_pin +=8;
    }

    ESP_LOGI(GPIO_TAG, "Init pcf8574 finished with %d devices", n);
    

    return ESP_OK;
}

esp_err_t IOTGpio::gpio_json_init(cJSON *gpio, bool checkReservedPins)
{
    ESP_LOGD(GPIO_TAG, "Gpio memory allocated %d", heap_caps_get_allocated_size(io_array));
    
    for (int r = 0; r < GPIO_NUMBER; r++ ) {
        for (int c = 0; c < 6; c++) {
            setGpioConfigValue(r, c, gpioConfig[r][c]);
        }
    }

    const cJSON *pin = NULL;
    if( xMutex == NULL ) {
        xMutex = xSemaphoreCreateMutex();
    }
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
            ESP_LOGW(GPIO_TAG, "Init gpio, pin check failed with current mode: %d", ((MODE_MODBUS + MODE_I2C) & getGpioConfigValue(idx,MODE)));
            if (((MODE_MODBUS + MODE_I2C) & getGpioConfigValue(idx, MODE)) > 0 && getGpioConfigValue(idx,MODE) != -1) {
                ESP_LOGE(GPIO_TAG, "Init gpio, pin check failed with current mode: %d", getGpioConfigValue(idx,MODE));
                continue;
            }
        }
        if(mode == MODE_EX_INPUT || mode == MODE_EX_OUTPUT) {
            //skip config ext ping
            continue;
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
        ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as %d", getGpioConfigValue(i,ID), getGpioConfigValue(i,MODE));
        gpio_isr_handler_remove((gpio_num_t)getGpioConfigValue(i,ID));
        if (getGpioConfigValue(i,MODE) > 0) {
            if (getGpioConfigValue(i,MODE) == MODE_INPUT || getGpioConfigValue(i,MODE) == MODE_EX_INT) {
                io_conf.intr_type = GPIO_INTR_ANYEDGE;
                io_conf.mode = GPIO_MODE_INPUT;
            }
            if (getGpioConfigValue(i,MODE) == MODE_OUTPUT) {
                io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
                io_conf.mode = GPIO_MODE_OUTPUT;
            }
            if ((getGpioConfigValue(i,MODE) & (MODE_OUTPUT | MODE_INPUT | MODE_EX_INT)) != 0) {
                io_conf.pin_bit_mask = (1ULL<<getGpioConfigValue(i,ID));
                io_conf.pull_down_en = (gpio_pulldown_t)getGpioConfigValue(i,PULL_D);
                io_conf.pull_up_en = (gpio_pullup_t)getGpioConfigValue(i,PULL_U);
                gpio_config(&io_conf);
                gpio_isr_handler_add((gpio_num_t)getGpioConfigValue(i,ID), gpio_isr_handler, (void*) getGpioConfigValue(i,ID));
            }
            if (getGpioConfigValue(i,MODE) == MODE_PWM) {
                ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as PWM", getGpioConfigValue(i,ID));
                if (pwm_count == 0) {
                    ledc_timer_config(&ledc_timer1);
                    ledc_channel1.gpio_num = getGpioConfigValue(i,ID);
                    ledc_channel_config(&ledc_channel1);
                    setGpioConfigValue(i,PWM_CHANNEL,1);
                    pwm_count++;
                } else {
                    ledc_timer_config(&ledc_timer1);
                    ledc_channel2.gpio_num = getGpioConfigValue(i,ID);
                    ledc_channel_config(&ledc_channel2);
                    setGpioConfigValue(i,PWM_CHANNEL,2);
                }
                ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as PWM DONE", getGpioConfigValue(i,ID));
            }
        }
    }

    /*cJSON *config = IOTConfig::readConfig();
    cJSON *pcf8574 = cJSON_GetObjectItem(config, "pcf8574");
    

    const cJSON *pcf_config = NULL;
    cJSON_ArrayForEach(pcf_config, pcf8574)
    {
        int exAddr = cJSON_GetObjectItemCaseSensitive(pcf_config, "ex_addr")->valueint;
        int ioAddr = cJSON_GetObjectItemCaseSensitive(pcf_config, "io_addr")->valueint;
        ESP_LOGD(GPIO_TAG, "Init pcf with addr %d pin %d", exAddr, ioAddr);

        uint8_t pcfConfig = 0;
        for (int p=0; p<8; p++) {
            int pIdx = getPinIndex(p + ioAddr);
            int mode = getGpioConfigValue(pIdx, MODE);
            if (mode == MODE_EX_INPUT || mode == MODE_EX_OUTPUT) {
                if (mode == MODE_EX_INPUT) {
                    pcfConfig |= 1UL << p;
                }
            }
        }
    }
    cJSON_Delete(config);*/
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
            int idx = getPinIndex(io_num);
            if (idx == -1) {
                continue;
            }
            int mode = getGpioConfigValue(idx, MODE);
            int current_state = gpio_get_level((gpio_num_t)io_num);
            if (mode == MODE_EX_INT && current_state == 0) {
                ESP_LOGD(GPIO_TAG, "Processing state change for pcf device");
                gpio_ext_int_handler();
                last_io_num = -1;
                delay = portMAX_DELAY;
            }
            
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
    for (int i = 0; i < GPIO_NUMBER + extra_pins; ++i) {
        if (getGpioConfigValue(i,ID) == id)
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
    ESP_LOGD(GPIO_TAG, "configurePin: max_mode=%d", getGpioConfigValue(idx,SUPPORTED_MODES));
    if ((mode & getGpioConfigValue(idx,SUPPORTED_MODES)) == 0) {
        ESP_LOGD(GPIO_TAG, "configurePin: wrong mode=%d", mode);
        if (mode == 0) {
            setGpioConfigValue(idx,MODE,0);
        } else {
            //gpioConfig[idx][MODE] = gpioConfig[idx][MODE];
        }
        setGpioConfigValue(idx,PULL_D,0);
        setGpioConfigValue(idx,PULL_U,0);
        return true;
    }
    setGpioConfigValue(idx,MODE,mode);
    setGpioConfigValue(idx,PULL_D,pull_down);
    setGpioConfigValue(idx,PULL_U,pull_up);
    return true;
}

void IOTGpio::reservePin(int gpioPin, int mode)
{
    int idx = getPinIndex(gpioPin);
    if (idx == -1) {
        return;
    }
    setGpioConfigValue(idx,MODE,mode);
    setGpioConfigValue(idx,PULL_D,0);
    setGpioConfigValue(idx,PULL_U,0);
}

void IOTGpio::freePin(int gpioPin)
{
    int idx = getPinIndex(gpioPin);
    if (idx == -1) {
        return;
    }
    setGpioConfigValue(idx,MODE,-1);
    setGpioConfigValue(idx,PULL_D,0);
    setGpioConfigValue(idx,PULL_U,0);
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
    for (int i = 0; i < GPIO_NUMBER + extra_pins; ++i) {
        ESP_LOGD(GPIO_TAG, "Creating config for pin %d, mode: %d", getGpioConfigValue(i,ID), getGpioConfigValue(i,MODE));
        pin = cJSON_CreateObject();
        if (pin == NULL)
        {
            return NULL;
        }
        cJSON_AddItemToArray(gpio, pin);
        pin_item1 = cJSON_CreateNumber(getGpioConfigValue(i,ID));
        if (pin_item1 == NULL)
        {
            return NULL;
        }
        cJSON_AddItemToObject(pin, "id", pin_item1);
        pin_item2 = cJSON_CreateNumber(getGpioConfigValue(i,MODE));
        if (pin_item2 == NULL)
        {
            return NULL;
        }
        cJSON_AddItemToObject(pin, "mode", pin_item2);
        pin_item3 = cJSON_CreateNumber(getGpioConfigValue(i,PULL_U));
        if (pin_item3 == NULL)
        {
            return NULL;
        }
        cJSON_AddItemToObject(pin, "pull_up", pin_item3);
        pin_item4 = cJSON_CreateNumber(getGpioConfigValue(i,PULL_D));
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
    if (getGpioConfigValue(idx,MODE) == MODE_OUTPUT) {
        return gpio_get_level((gpio_num_t)pin);
    } else if (getGpioConfigValue(idx,MODE) == MODE_PWM) {
        if (getGpioConfigValue(idx,PWM_CHANNEL) == 1) {
            int duty = ledc_get_duty(ledc_channel1.speed_mode, ledc_channel1.channel);
            if (duty > 1024) {
                ESP_LOGD(GPIO_TAG, "Duty for channel 1 OVF: %d", duty);
            }
            return duty;
        } else {
            int duty = ledc_get_duty(ledc_channel2.speed_mode, ledc_channel2.channel);
            if (duty > 1024) {
                ESP_LOGD(GPIO_TAG, "Duty for channel 2 OVF: %d", duty);
            }
            return duty;
        }
    } else if (getGpioConfigValue(idx,MODE) == MODE_INPUT) {
        return gpio_get_level((gpio_num_t)pin);
    } else if (getGpioConfigValue(idx,MODE) == MODE_EX_INPUT || getGpioConfigValue(idx,MODE) == MODE_EX_OUTPUT) {
        return ex_gpio_get_level(pin);
    }
    return -2;
}

void IOTGpio::gpio_ext_int_handler() {
    for (int device=0; device<GPIO_EX_MAX_NUMBER; device++) {
        int devStartAddr = gpioExConfig[device][PCF_IO_START_ADDR];
        ESP_LOGD(GPIO_TAG, "devStartAddress during iterate %d, device = %d", devStartAddr, device);
        uint8_t val;
        i2c_dev_t pcf = gpioExDevices[device];
        ESP_LOGD(GPIO_TAG, "PCF Address during iterate = %d", pcf.addr);
        if (pcf.addr == 0) {
            return;
        }
        pcf8574_port_read(&pcf, &val);

        int last_val = gpioExConfig[device][PCF_IO_LAST_VALUE];
        if (last_val == val) {
            return;
        }
        gpioExConfig[device][PCF_IO_LAST_VALUE] = val;

        // iterate over pins

        for (int p = devStartAddr; p< devStartAddr + 8; p++) {
            int idx = getPinIndex(p);
            if (getGpioConfigValue(idx,MODE) == MODE_EX_INPUT) {
                int state = ex_gpio_get_level(getGpioConfigValue(idx,ID));
                ESP_LOGD(GPIO_TAG, "Update state %d for pin %d", state, getGpioConfigValue(idx,ID));
                sendIoState(getGpioConfigValue(idx,ID), state);
            }
        }

    }
}

int IOTGpio::get_ex_gpio_device(int pin) {
    for (int i=0; i<GPIO_EX_MAX_NUMBER; i++) {
        int devStartAddr = gpioExConfig[i][PCF_IO_START_ADDR];
        if ((devStartAddr <= pin) && (pin < (devStartAddr + 8))) {
            ESP_LOGD(GPIO_TAG, "Found device at position %d", i);
            return i;
        }
    }
    return -1;
}

int IOTGpio::ex_gpio_get_level(int pin)
{
    int device = get_ex_gpio_device(pin);
    // read value
    uint8_t val;
    i2c_dev_t pcf = gpioExDevices[device];

    pcf8574_port_read(&pcf, &val);
    ESP_LOGD(GPIO_TAG, "Got value %i from device", val);
    // calculate value for pin
    int bit = pin - gpioExConfig[device][PCF_IO_START_ADDR];
    ESP_LOGD(GPIO_TAG, "Getting value for position %d", bit);
    return (val & ( 1 << bit )) >> bit;
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
        cJSON_AddNumberToObject(json_pin, "mode", getGpioConfigValue(idx,MODE));
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
    for (int i = 0; i < GPIO_NUMBER + extra_pins; ++i) {
        int pin_state = readPinState(getGpioConfigValue(i,ID));
        if (pin_state > -1) {
            cJSON *json_pin_state = get_json_pin_state(getGpioConfigValue(i,ID));
            cJSON_AddItemToArray(gpio, json_pin_state);
        }
    }
    return gpio;
}

void IOTGpio::ex_gpio_set_level(int pin, int level) {

    int device = get_ex_gpio_device(pin);
    // get bit position 
    int bit = pin - gpioExConfig[device][PCF_IO_START_ADDR];
    // read value
    
    i2c_dev_t pcf = gpioExDevices[device];
    //ESP_LOGD(GPIO_TAG, "Got value %d from device", pcf.addr);
    //pcf8574_port_read(&pcf, &val);
    //ESP_LOGD(GPIO_TAG, "Got value %i from device", val);
    // apply mask to ignore input bits
    uint8_t val = gpioExConfig[device][PCF_IO_OUT_VALUE];  
    int curValue = (val >> bit) & 1U;
    ESP_LOGD(GPIO_TAG, "Current value is %i for bit %d", curValue, bit);
    if (level == 0) {
        val &= ~(1 << bit);
    } else  {
       val  |= 1 << bit;
    }
    gpioExConfig[device][PCF_IO_OUT_VALUE] = val;
    pcf8574_port_write(&pcf, val);   

}

esp_err_t IOTGpio::setPinState(int pin, int state)
{
    ESP_LOGI(GPIO_TAG, "Set gpio state: pin = %d, level = %d", pin, state);
    int idx = getPinIndex(pin);
    if (idx == -1) {
        return ESP_FAIL;
    }
    if (getGpioConfigValue(idx,MODE) == MODE_OUTPUT) {
        gpio_set_level((gpio_num_t)pin, state);
        return ESP_OK;
    } else if (getGpioConfigValue(idx,MODE) == MODE_EX_INPUT || getGpioConfigValue(idx,MODE) == MODE_EX_OUTPUT) {
        ex_gpio_set_level(pin, state);
    } else if (getGpioConfigValue(idx,MODE) == MODE_PWM) {
        xSemaphoreTake( xMutex, portMAX_DELAY );
        //ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, state);
        //ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);

        ESP_LOGD(GPIO_TAG, "Duty for channel 1 before update %d", ledc_get_duty(ledc_channel1.speed_mode, ledc_channel1.channel));
        ESP_LOGD(GPIO_TAG, "Duty for channel 2 before update %d", ledc_get_duty(ledc_channel2.speed_mode, ledc_channel2.channel));
        if (getGpioConfigValue(idx,PWM_CHANNEL) == 1) {
            ESP_LOGI(GPIO_TAG, "Update PWM for channel 1");
            //ledc_set_fade_step_and_start()
            //ledc_set_fade_step_and_start(ledc_channel1.speed_mode, ledc_channel1.channel, state, 1, 1, LEDC_FADE_NO_WAIT);
            ledc_set_fade_with_time(ledc_channel1.speed_mode, ledc_channel1.channel, state, 300);
            ledc_fade_start(ledc_channel1.speed_mode,ledc_channel1.channel, LEDC_FADE_WAIT_DONE);
        } else {
            ESP_LOGI(GPIO_TAG, "Update PWM for channel 2");
            ledc_set_fade_with_time(ledc_channel2.speed_mode, ledc_channel2.channel, state, 300);
            ledc_fade_start(ledc_channel2.speed_mode,ledc_channel2.channel, LEDC_FADE_WAIT_DONE);
        }
        ESP_LOGD(GPIO_TAG, "Duty for channel 1 after update %d", ledc_get_duty(ledc_channel1.speed_mode, ledc_channel1.channel));
        ESP_LOGD(GPIO_TAG, "Duty for channel 2 after update %d", ledc_get_duty(ledc_channel2.speed_mode, ledc_channel2.channel));
        xSemaphoreGive( xMutex );
        return ESP_OK;
    }

    return ESP_FAIL;
}