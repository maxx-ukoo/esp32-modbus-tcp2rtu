#ifndef GPIO_H
#define GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include "esp_event.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define GPIO_NUMBER 15

#define ID 0
#define MODE 1
#define PULL_U 2
#define PULL_D 3
#define SUPPORTED_MODES 4
#define PWM_CHANNEL 5

#define MODE_INPUT 1
#define MODE_OUTPUT 2
#define MODE_PWM 4
#define MODE_MODBUS 8
#define MODE_I2C 16
#define MODE_CURTAINS 32

typedef struct {
    int pin;
    int state;
} pin_state_msg_t;

class IOTGpio {
    private:
        static int gpioConfig[GPIO_NUMBER][6];
        static ledc_channel_config_t ledc_channel1;
        static ledc_channel_config_t ledc_channel2;
        static ledc_timer_config_t ledc_timer1;

        static xQueueHandle gpio_evt_queue;
        static TaskHandle_t gpio_task_xHandle;
        static TaskHandle_t mqtt2gpio_task_xHandle;

        static esp_err_t fade_service_status;
        static esp_err_t esr_service_status;
        static xQueueHandle gpio2mqtt_queue;
        static xQueueHandle mqtt2gpio_queue;

        static void mqtt2gpio_task(void* arg);
        static bool configure_pin(int gpioPin, int mode, int pull_up, int pull_down);
        static void gpio_task(void* arg);
        static int getPinIndex(int id);
        static void IRAM_ATTR gpio_isr_handler(void* arg);
        IOTGpio(const IOTGpio&);
        IOTGpio& operator =(const IOTGpio&);
        static esp_err_t gpio_start();
    public:
        static void set_mqtt_gpio_evt_queue(xQueueHandle gpio2mqtt_queue_handler, xQueueHandle mqtt2gpio_queue_handler);
        static void freePin(int gpioPin);
        static void reservePin(int gpioPin, int mode);
        static esp_err_t gpio_json_init(cJSON *gpio, bool checkReservedPins);
        static cJSON * get_gpio_config();
        static int readPinState(int pin);
        static cJSON * get_json_pin_state(int gpioPin);
        static cJSON * getGpioState();
        static esp_err_t setPinState(int pin, int state);
        IOTGpio(cJSON *config);
    
    ~IOTGpio(void);

};

/*cJSON * getGpioConfig();

cJSON * getGpioState();
esp_err_t setPinState(int pin, int state);
esp_err_t reserve_pin_state(int pin, int state);
void set_mqtt_gpio_evt_queue(xQueueHandle gpio2mqtt_queue_handler, xQueueHandle mqtt2gpio_queue_handler);*/

#ifdef __cplusplus
}
#endif
#endif /* GPIO_H */


