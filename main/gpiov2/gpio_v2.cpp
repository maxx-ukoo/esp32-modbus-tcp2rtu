#include "gpio_v2.h"

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

#define ID                  0
#define MODE                1
#define PULL_U              2
#define PULL_D              3
#define SUPPORTED_MODES     4
#define PWM_CHANNEL         5
#define DEP_VALUE           6
#define DEP_LAST_STATE      7

static const char *GPIO_TAG = "GPIOV2";

xQueueHandle GpioV2::gpio_isr_evt_queue = NULL;
TaskHandle_t GpioV2::gpio_isr_evt_task_xHandle = NULL;
esp_err_t GpioV2::esr_service_status = ESP_FAIL;
void (*GpioV2::state_cb)(int, int) = NULL;
esp_err_t GpioV2::fade_service_status = ESP_FAIL;

int GpioV2::gpio_config_arr[GPIO_NUMBER][GPIO_PARAMS_NUMBER] = {
        {2, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0, 0, 0},
        {4, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM | MODE_MODBUS, 0, 0, 0},
        {5, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0, 0, 0},
        {12, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0, 0, 0},
        {13, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM | MODE_MODBUS, 0, 0, 0},
        {14, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0, 0, 0},
        {15, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0, 0, 0},  
        {18, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM | MODE_MODBUS, 0, 0, 0},
        {23, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0, 0, 0},
        {32, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0, 0, 0},
        {33, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_OUTPUT | MODE_PWM, 0, 0, 0},
        {34, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_EX_INT, 0, 0, 0},
        {35, MODE_DISABLED, 0, 0, MODE_INPUT | MODE_EX_INT, 0, 0, 0},
        {36, MODE_DISABLED, 0, 0, MODE_INPUT, 0, 0, 0},
        {39, MODE_DISABLED, 0, 0, MODE_INPUT, 0, 0, 0}
};

ledc_channel_config_t GpioV2::ledc_channel1 = {
            .gpio_num   = 0,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .channel    = LEDC_CHANNEL_0,
            .timer_sel  = LEDC_TIMER_1,
            .duty       = 1023,
            .hpoint     = 0 
        };
ledc_channel_config_t GpioV2::ledc_channel2 =  {
            .gpio_num   = 0,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .channel    = LEDC_CHANNEL_1,
            .timer_sel  = LEDC_TIMER_1,
            .duty       = 1023,
            .hpoint     = 0 
        };        
ledc_timer_config_t GpioV2::ledc_timer1 = {
            .speed_mode = LEDC_HIGH_SPEED_MODE,   // timer mode
            .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
            .timer_num = LEDC_TIMER_1,            // timer index
            .freq_hz = 64000,                      // frequency of PWM signal
            .clk_cfg = LEDC_AUTO_CLK             // Auto select the source clock
        };

GpioV2::GpioV2() {

}

void IRAM_ATTR GpioV2::gpio_isr_handler(void* arg)
{
        uint32_t gpio_num = (uint32_t) arg;
        int io_state = gpio_get_level((gpio_num_t)gpio_num);
        io_isr_message_t msg_out = {
                .io_pin = gpio_num,
                .io_state = io_state
                };
        xQueueSendFromISR(gpio_isr_evt_queue, &msg_out, NULL);
}

esp_err_t GpioV2::configure_pin(int gpio_pin, int gpio_pin_mode) {
        ESP_LOGI(GPIO_TAG, "Configuring pin: %d as mode %d", gpio_pin, gpio_pin_mode);
        int pin_idx = convert_gpio2pinidx(gpio_pin);
        if (pin_idx == -1) {
                return ESP_FAIL;
        }
        int mode = gpio_config_arr[pin_idx][MODE];
        if (mode != MODE_DISABLED) {
                ESP_LOGE(GPIO_TAG, "Configured pin error, current mode is: %d", mode);
                return ESP_FAIL;
        }
        gpio_config_arr[pin_idx][MODE] = gpio_pin_mode;
        ESP_LOGI(GPIO_TAG, "Configured pin: %d with idx %d as mode %d", gpio_pin, pin_idx, gpio_config_arr[pin_idx][mode]);
        //for (int i = 0; i < GPIO_NUMBER; ++i) {
        //        ESP_LOGW(GPIO_TAG, "Pin idx [%d] configured as %d", i, gpio_config_arr[i][MODE]);
        //}
        return ESP_OK;
}

esp_err_t GpioV2::start() {
        ESP_LOGW(GPIO_TAG, "GPIOV2 starting P1");
        if (gpio_isr_evt_queue == NULL) {
                ESP_LOGI(GPIO_TAG, "GPIO QUEUE creating");
                gpio_isr_evt_queue = xQueueCreate(20, sizeof(io_isr_message_t));
        }
        ESP_LOGW(GPIO_TAG, "GPIOV2 starting P2");
        if (gpio_isr_evt_task_xHandle == NULL) {
                ESP_LOGI(GPIO_TAG, "GPIO TASK creating");
                gpio_isr_evt_task_xHandle = (TaskHandle_t)xTaskCreate((TaskFunction_t)gpio_isr_evt_task, "gpio_isr_evt_task", 4096, NULL, 10, NULL);
        }
        ESP_LOGD(GPIO_TAG, "esr_service_status: %d", esr_service_status);
        if (esr_service_status != ESP_OK) {
                ESP_LOGI(GPIO_TAG, "instaling isr service");
                esr_service_status = gpio_install_isr_service(0);
                ESP_LOGI(GPIO_TAG, "installed");
        }
        if (is_pwm_enabled() && fade_service_status != ESP_OK) {
                fade_service_status = ledc_fade_func_install(1);
                ESP_LOGD(GPIO_TAG, "Fade service installed");
        }
        ESP_LOGW(GPIO_TAG, "GPIOV2 starting P3");
        return configure_pins();
}

esp_err_t GpioV2::configure_pins() {
        gpio_config_t io_conf;
        int pwm_count = 0;
        for (int i = 0; i < GPIO_NUMBER; ++i) {
                ESP_LOGI(GPIO_TAG, "Pin idx [%d] configured as %d", i, gpio_config_arr[i][MODE]);
                int mode = gpio_config_arr[i][MODE];
                if (mode == MODE_INPUT) {
                        io_conf.intr_type = GPIO_INTR_ANYEDGE;
                        io_conf.mode = GPIO_MODE_INPUT;
                }
                if (mode == MODE_OUTPUT) {
                        io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
                        io_conf.mode = GPIO_MODE_OUTPUT;
                }
                if (mode == MODE_PWM) {
                        ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as PWM", gpio_config_arr[i][ID]);
                        if (pwm_count == 0) {
                        ledc_timer_config(&ledc_timer1);
                        ledc_channel1.gpio_num = gpio_config_arr[i][ID];
                        ledc_channel_config(&ledc_channel1);
                        gpio_config_arr[i][PWM_CHANNEL] = 1;
                        pwm_count++;
                        } else {
                        ledc_timer_config(&ledc_timer1);
                        ledc_channel2.gpio_num = gpio_config_arr[i][ID];
                        ledc_channel_config(&ledc_channel2);
                        gpio_config_arr[i][PWM_CHANNEL] = 2;
                        }
                        ESP_LOGD(GPIO_TAG, "GPIO[%d] configuring as PWM DONE", gpio_config_arr[i][ID]);
                }
                ESP_LOGW (GPIO_TAG, "GPIOV2 mode %d %d %d",  mode, (MODE_OUTPUT | MODE_INPUT), mode & (MODE_OUTPUT | MODE_INPUT));
                if ((mode & (MODE_INPUT | MODE_OUTPUT)) != 0) {
                        ESP_LOGI  (GPIO_TAG, "GPIOV2 configureing int for pin %d",  gpio_config_arr[i][ID]);
                        io_conf.pin_bit_mask = (1ULL<<gpio_config_arr[i][ID]);
                        io_conf.pull_down_en = (gpio_pulldown_t)gpio_config_arr[i][PULL_D];
                        io_conf.pull_up_en = (gpio_pullup_t)gpio_config_arr[i][PULL_U];
                        gpio_config(&io_conf);
                        //gpio_isr_handler_add((gpio_num_t)gpio_config_arr[i][ID], (gpio_isr_t)&GpioV2::gpio_isr_handler, (void*) gpio_config_arr[i][ID]);
                        if (mode == MODE_INPUT) {
                                gpio_isr_handler_add((gpio_num_t)gpio_config_arr[i][ID], gpio_isr_handler, (void*) gpio_config_arr[i][ID]);     
                        }
                }
                ESP_LOGI(GPIO_TAG, "Pin [%d] configured as %d", gpio_config_arr[i][ID], mode);
        }
        return ESP_OK;
}

int GpioV2::convert_gpio2pinidx(int gpio_pin) {
        for (int i = 0; i < GPIO_NUMBER; ++i) {
                if (gpio_config_arr[i][ID] == gpio_pin)
                        return i;
        }
        return -1;
}

esp_err_t GpioV2::set_pin_state(int pin, int state) {
        ESP_LOGD(GPIO_TAG, "Set pin[%d] state as %d", pin, state);
        int pin_idx = convert_gpio2pinidx(pin);
        if (pin_idx == -1) {
                return ESP_FAIL;
        }
        if (gpio_config_arr[pin_idx][MODE] == MODE_OUTPUT) {
                ESP_LOGI(GPIO_TAG, "Set pin[%d] state as %d", pin_idx, state);
                gpio_set_level((gpio_num_t)pin, state);
                return ESP_OK;
        }
        if (gpio_config_arr[pin_idx][MODE] == MODE_PWM) {
                ESP_LOGD(GPIO_TAG, "Duty for channel 1 before update %d", ledc_get_duty(ledc_channel1.speed_mode, ledc_channel1.channel));
                ESP_LOGD(GPIO_TAG, "Duty for channel 2 before update %d", ledc_get_duty(ledc_channel2.speed_mode, ledc_channel2.channel));
                if (gpio_config_arr[pin_idx][PWM_CHANNEL] == 1) {
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
                return ESP_OK;
        }
        ESP_LOGE(GPIO_TAG, "Wrong pin state to set output level pin[%d] state: %d", pin, state);
        return ESP_OK; // return OK, it is expected        
}

bool GpioV2::is_pwm_enabled() {
        for (int i = 0; i < GPIO_NUMBER; ++i) {
                if (gpio_config_arr[i][MODE] == MODE_PWM)
                        return true;
                }
        return false; 
}

void GpioV2::gpio_isr_evt_task(void* arg)
{
//#define DEP_VALUE           6
//#define DEP_LAST_STATE      7
        int pin_idx = 0;
        io_isr_message_t msg;
        for(;;) {
                if (pdTRUE == xQueueReceive(gpio_isr_evt_queue, &msg, 5)) {
                        ESP_LOGI(GPIO_TAG, "GPIO[%d] intr, val:%d", msg.io_pin, msg.io_state);
                        pin_idx = convert_gpio2pinidx(msg.io_pin);
                        gpio_config_arr[pin_idx][DEP_VALUE] = 1;
                        gpio_config_arr[pin_idx][DEP_LAST_STATE] = msg.io_state;
                } else {
                        for (int i = 0; i < GPIO_NUMBER; ++i) {
                                if (gpio_config_arr[pin_idx][DEP_VALUE] > 0) {
                                        if (gpio_config_arr[pin_idx][DEP_VALUE] == 2) {
                                                gpio_config_arr[pin_idx][DEP_VALUE] = 0;
                                                ESP_LOGW(GPIO_TAG, "GPIO[%d] intr, SEND val:%d", msg.io_pin, msg.io_state);
                                                int io_state = gpio_get_level((gpio_num_t)gpio_config_arr[pin_idx][ID]);
                                                if (state_cb != NULL) {
                                                        state_cb(gpio_config_arr[pin_idx][ID], io_state);
                                                }
                                        } else {
                                                gpio_config_arr[pin_idx][DEP_VALUE] = gpio_config_arr[pin_idx][DEP_VALUE] + 1;
                                        }
                                }
                        }
                }
                //vTaskDelay(5);
        }
}