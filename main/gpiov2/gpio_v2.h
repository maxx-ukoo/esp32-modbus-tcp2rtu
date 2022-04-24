#ifndef GPIO_V2_H
#define GPIO_V2_H

#include <vector>
#include <esp_event.h>
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPIO_NUMBER         15
#define GPIO_PARAMS_NUMBER  8

typedef struct {
    u_int32_t io_pin;
    int io_state;
} io_isr_message_t;

enum pin_mode_t {
    MODE_DISABLED = 0,
    MODE_INPUT = 1,
    MODE_OUTPUT = 2,
    MODE_PWM = 4,
    MODE_MODBUS = 8,
    MODE_I2C = 16,
    MODE_CURTAINS = 32,
    MODE_EX_INT = 64
};

class GpioV2 {
    public: 
        GpioV2();
        esp_err_t configure_pin(int gpio_pin, int gpio_pin_mode);
        esp_err_t start();
        static void (*state_cb)(int, int);
        static esp_err_t set_pin_state(int pin, int state);
    private:
        static xQueueHandle gpio_isr_evt_queue;
        static TaskHandle_t gpio_isr_evt_task_xHandle;
        static esp_err_t esr_service_status;
        static int gpio_config_arr[GPIO_NUMBER][GPIO_PARAMS_NUMBER];
        esp_err_t configure_pins();
        static int convert_gpio2pinidx(int gpio_pin);
        static void IRAM_ATTR gpio_isr_handler(void* arg);
        static void gpio_isr_evt_task(void* arg);
        static bool is_pwm_enabled();
        static ledc_channel_config_t ledc_channel1;
        static ledc_channel_config_t ledc_channel2;
        static ledc_timer_config_t ledc_timer1;
        static esp_err_t fade_service_status;
    
    ~GpioV2(void);
};

#ifdef __cplusplus
}
#endif
#endif /* GPIO_V2_H */


