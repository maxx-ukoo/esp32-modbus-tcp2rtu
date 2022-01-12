#ifndef GPIO_H
#define GPIO_H

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

#include <i2cdev.h>

#include "cJSON.h"
#include "esp_event.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define GPIO_NUMBER 15
#define GPIO_ARRAY_SIZE 6
#define GPIO_EX_MAX_NUMBER 4

#define ID 0
#define MODE 1
#define PULL_U 2
#define PULL_D 3
#define SUPPORTED_MODES 4
#define PWM_CHANNEL 5


#define PCF_ADDR 0
#define PCF_IO_START_ADDR 1
#define PCF_IO_CONFIG 2
#define PCF_IO_LAST_VALUE 3
#define PCF_IO_OUT_VALUE 4

#define MODE_INPUT 1
#define MODE_OUTPUT 2
#define MODE_PWM 4
#define MODE_MODBUS 8
#define MODE_I2C 16
#define MODE_CURTAINS 32
#define MODE_EX_INPUT 64
#define MODE_EX_OUTPUT 128
#define MODE_EX_INT 256

typedef struct {
    int pin;
    int state;
} pin_state_msg_t;

class IOTGpio {
    private:
        static int gpioConfig[GPIO_NUMBER][6];
        static int gpioExConfig[GPIO_EX_MAX_NUMBER][5];
        static i2c_dev_t gpioExDevices[GPIO_EX_MAX_NUMBER];
        static int *io_array;
        static int extra_pins;
        static int getGpioConfigValue(int row, int col);
        static void setGpioConfigValue(int row, int col, int value);
        static ledc_channel_config_t ledc_channel1;
        static ledc_channel_config_t ledc_channel2;
        static ledc_timer_config_t ledc_timer1;
        static void sendIoState(int io_num, int io_state);
        static xQueueHandle gpio_evt_queue;
        static TaskHandle_t gpio_task_xHandle;
        static TaskHandle_t mqtt2gpio_task_xHandle;

        static esp_err_t fade_service_status;
        static esp_err_t esr_service_status;
        static xQueueHandle gpio2mqtt_queue;
        static xQueueHandle mqtt2gpio_queue;
        static SemaphoreHandle_t xMutex;

        static void mqtt2gpio_task(void* arg);
        static bool configure_pin(int gpioPin, int mode, int pull_up, int pull_down);
        static void gpio_task(void* arg);
        static int getPinIndex(int id);
        static int ex_gpio_get_level(int pin);
        static void ex_gpio_set_level(int pin, int level);
        static void gpio_ext_int_handler();
        static void IRAM_ATTR gpio_isr_handler(void* arg);
        static int get_ex_gpio_device(int pin);
        IOTGpio(const IOTGpio&);
        IOTGpio& operator =(const IOTGpio&);
        static esp_err_t gpio_start();
    public:
        static void set_mqtt_gpio_evt_queue(xQueueHandle gpio2mqtt_queue_handler, xQueueHandle mqtt2gpio_queue_handler);
        static void freePin(int gpioPin);
        static void reservePin(int gpioPin, int mode);
        static esp_err_t gpio_json_init(cJSON *gpio, bool checkReservedPins);
        static esp_err_t gpio_jsonextender_init(cJSON *extender_config);
        static cJSON * get_gpio_config();
        static int readPinState(int pin);
        static cJSON * get_json_pin_state(int gpioPin);
        static cJSON * getGpioState();
        static esp_err_t setPinState(int pin, int state);
        IOTGpio(cJSON *config);
    
    ~IOTGpio(void);

};

#ifdef __cplusplus
}
#endif
#endif /* GPIO_H */


