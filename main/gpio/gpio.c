#include "gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *GPIO_TAG = "GPIO Tag";

// {id, mode 0/1 (0 - input, 1 - output), pullup, pulldown, maxmode}
static int gpioConfig[GPIO_NUMBER][5];
static int gpioDefaultConfig[GPIO_NUMBER][5] = {
    {2, 0, 0, 0, 1},
    {4, 0, 0, 0, 1},
    {5, 0, 0, 0, 1},
    {12, 0, 0, 0, 1},
    {13, 0, 0, 0, 1},
    {14, 0, 0, 0, 1},
    {15, 0, 0, 0, 1},
    {18, 0, 0, 0, 1},
    {23, 0, 0, 0, 1},
    {32, 0, 0, 0, 1},
    {33, 0, 0, 0, 1},
    {34, 0, 0, 0, 0},
    {35, 0, 0, 0, 0},
    {36, 0, 0, 0, 0},
    {39, 0, 0, 0, 0}
};

static xQueueHandle gpio_evt_queue = NULL;

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
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

static int getPinIndex(int id) {
    for (int i = 0; i < GPIO_NUMBER; ++i) {
        if (gpioConfig[i][MODE] == id)
            return i;
    }
    return -1;
}

static bool configurePin(int id, int mode, int pull_up, int pull_down) {
    int idx = getPinIndex(id);
    if (idx == -1) {
        return false;
    }
    if (mode > gpioConfig[idx][MODE_MAX]) {
        return false;
    }
    gpioConfig[idx][MODE] = mode;
    gpioConfig[idx][PULL_D] = pull_down;
    gpioConfig[idx][PULL_U] = pull_up;
    return true;
}

int gpioInitFromJson(cJSON *gpio) {
    const cJSON *pin = NULL;
    const cJSON *pins = NULL;
    int status = -1;
    pins = cJSON_GetObjectItemCaseSensitive(gpio, "config");
    cJSON_ArrayForEach(pin, pins)
    {
        int id = cJSON_GetObjectItemCaseSensitive(pin, "id")->valueint;
        int mode = cJSON_GetObjectItemCaseSensitive(pin, "mode")->valueint;
        int pull_up = cJSON_GetObjectItemCaseSensitive(pin, "pull_up")->valueint;
        int pull_down = cJSON_GetObjectItemCaseSensitive(pin, "pull_down")->valueint;
        if (!configurePin(id, mode, pull_up, pull_down)) {
            status = id;
            goto end;
        }
    }    
end:
    cJSON_Delete(gpio);
    return status;    
}



static esp_err_t gpioInit() {
    gpio_config_t io_conf;
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    gpio_install_isr_service(0);

    for (int i = 0; i < GPIO_NUMBER; ++i) {
        gpio_isr_handler_remove(gpioConfig[i][ID]);
        if (gpioConfig[i][MODE] == 0)
            io_conf.intr_type = GPIO_INTR_ANYEDGE;
        else
            io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
        if (gpioConfig[i][MODE] == 0)
            io_conf.mode = GPIO_MODE_INPUT;
        else
            io_conf.mode = GPIO_MODE_OUTPUT;

        io_conf.pin_bit_mask = (1ULL<<gpioConfig[i][ID]);
        io_conf.pull_down_en = gpioConfig[i][PULL_U];
        io_conf.pull_up_en = gpioConfig[i][PULL_D];
        gpio_config(&io_conf);
        
        //hook isr handler for specific gpio pin

        gpio_isr_handler_add(gpioConfig[i][ID], gpio_isr_handler, (void*) gpioConfig[i][ID]);
    }
    return ESP_OK;
}

static cJSON * getGpioConfig(bool e, int pinsConfig[GPIO_NUMBER][5]) {
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
        ESP_LOGE(GPIO_TAG, "Creating config for pin %d", pinsConfig[i][ID]);
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
    ESP_LOGE(GPIO_TAG, "Error on creating gpio config");
    cJSON_Delete(gpio);
    return NULL;
}

cJSON * getGpioCurrentConfig() {
    return getGpioConfig(false, gpioConfig);
}

cJSON * getGpioDefaultConfig() {
    return getGpioConfig(false, gpioDefaultConfig);
}