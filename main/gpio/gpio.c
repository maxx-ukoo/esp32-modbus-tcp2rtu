#include "gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"



static const char *GPIO_TAG = "GPIO Tag";

// {id, mode 0/1 (0 - input, 1 - output), pullup, pulldown, maxmode}
static int gpioConfig[GPIO_NUMBER][5] = {
    {2, -1, 0, 0, 1},
    {4, -1, 0, 0, 1},
    {5, -1, 0, 0, 1},
    {12, -1, 0, 0, 1},
    {13, -1, 0, 0, 1},
    {14, -1, 0, 0, 1},
    {15, -1, 0, 0, 1},
    {18, -1, 0, 0, 1},
    {23, -1, 0, 0, 1},
    {32, -1, 0, 0, 1},
    {33, -1, 0, 0, 1},
    {34, -1, 0, 0, 0},
    {35, -1, 0, 0, 0},
    {36, -1, 0, 0, 0},
    {39, -1, 0, 0, 0}
};

static xQueueHandle gpio_evt_queue = NULL;
static TaskHandle_t gpio_task_xHandle = NULL;
static esp_err_t esr_service_status = ESP_FAIL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGD(GPIO_TAG, "GPIO[%d] intr, val:%d", io_num, gpio_get_level(io_num));
            //printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
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

    for (int i = 0; i < GPIO_NUMBER; ++i) {
        gpio_isr_handler_remove(gpioConfig[i][ID]);
        if (gpioConfig[i][MODE] != -1) {
            if (gpioConfig[i][MODE] == 0)
                io_conf.intr_type = GPIO_INTR_ANYEDGE;
            else
                io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
            if (gpioConfig[i][MODE] == 0)
                io_conf.mode = GPIO_MODE_INPUT;
            else
                io_conf.mode = GPIO_MODE_OUTPUT;

            io_conf.pin_bit_mask = (1ULL<<gpioConfig[i][ID]);
            io_conf.pull_down_en = gpioConfig[i][PULL_D];
            io_conf.pull_up_en = gpioConfig[i][PULL_U];
            gpio_config(&io_conf);
            
            //hook isr handler for specific gpio pin

            gpio_isr_handler_add(gpioConfig[i][ID], gpio_isr_handler, (void*) gpioConfig[i][ID]);
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
    ESP_LOGD(GPIO_TAG, "configurePin: max_mode=%d", gpioConfig[idx][MODE_MAX]);
    if (mode > gpioConfig[idx][MODE_MAX]) {
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
    int idx = getPinIndex(pin);
    cJSON *root = cJSON_CreateObject();
    if (idx == -1) {
        cJSON_AddNumberToObject(root, "error_no_pin", 1);
    }
    cJSON_AddNumberToObject(root, "pin", pin);
    if (gpioConfig[idx][MODE] != 1) {
        cJSON_AddNumberToObject(root, "error_wrong_mode", 1);
    }

    gpio_set_level(pin, state);
    cJSON_AddNumberToObject(root, "state", state);
    return root;
}

cJSON * getPinState(int pin) {
    int idx = getPinIndex(pin);
    cJSON *root = cJSON_CreateObject();
    if (idx == -1) {
        cJSON_AddNumberToObject(root, "error_no_pin", 1);
    }
    cJSON_AddNumberToObject(root, "pin", pin);
    if (gpioConfig[idx][MODE] != 0) {
        cJSON_AddNumberToObject(root, "error_wrong_mode", 1);
    }

    cJSON_AddNumberToObject(root, "state", gpio_get_level(pin));
    return root;
}

