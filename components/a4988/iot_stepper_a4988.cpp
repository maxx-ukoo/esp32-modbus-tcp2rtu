#ifdef __cplusplus
extern "C"
{
#endif

#include "iot_stepper_a4988.h"
#include "driver/gpio.h"
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define delay_ms(ms) vTaskDelay((ms) / portTICK_RATE_MS)
#define UP 1
#define DOWN -1

    static const char *TAG = "iot_a4988";

    int IOT4988Stepper::getHiState()
    {
        return gpio_get_level((gpio_num_t)pstepper->hi_pos_io);
    }

    IOT4988Stepper::IOT4988Stepper(int step_io, int dir_io, int hi_pos_io)
    {
        ets_update_cpu_frequency_rom(ets_get_detected_xtal_freq() / 1000000);
        pstepper = (stepper_dev_t *)calloc(sizeof(stepper_dev_t), 1);
        pstepper->step_io = step_io;
        pstepper->dir_io = dir_io;
        pstepper->hi_pos_io = hi_pos_io;

        gpio_config_t dir;
        dir.intr_type = GPIO_INTR_DISABLE;
        dir.mode = GPIO_MODE_OUTPUT;
        dir.pin_bit_mask = (1LL << pstepper->dir_io);
        dir.pull_down_en = GPIO_PULLDOWN_DISABLE;
        dir.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&dir);
        dir.intr_type = GPIO_INTR_DISABLE;
        dir.mode = GPIO_MODE_OUTPUT;
        dir.pin_bit_mask = (1LL << pstepper->step_io);
        dir.pull_down_en = GPIO_PULLDOWN_DISABLE;
        dir.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&dir);
        gpio_set_level((gpio_num_t)pstepper->step_io, 0);

        dir.intr_type = GPIO_INTR_DISABLE;
        dir.mode = GPIO_MODE_INPUT;
        dir.pin_bit_mask = (1LL << pstepper->hi_pos_io);
        dir.pull_down_en = GPIO_PULLDOWN_DISABLE;
        dir.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&dir);
        pstepper->current_position = 0;
        pstepper->lenght = 500000;
    }

    esp_err_t IOT4988Stepper::step(int direction, int steps)
    {
        if (direction > 0)
        {
            ESP_LOGD(TAG, "Stepper run, set direction to 0\n");
            gpio_set_level((gpio_num_t)pstepper->dir_io, 0);
        }
        else
        {
            ESP_LOGD(TAG, "Stepper run, set direction to 1\n");
            gpio_set_level((gpio_num_t)pstepper->dir_io, 1);
        }
        ESP_LOGD(TAG, "Steps number: %d\n", steps);
        for (size_t i = 0; i < steps; i++)
        {
            if (getHiState() == 0 && direction > 0) {
                ESP_LOGI(TAG, "FAIL with up state: %d, position: %d, steps: %d\n", getHiState(), pstepper->current_position , i);
                return ESP_FAIL;                        
            }
            if (pstepper->current_position >= pstepper->lenght && direction < 0) {
                ESP_LOGI(TAG, "FAIL with lenght state: %d, position: %d, lenght: %d, steps: %d\n", getHiState(), pstepper->current_position, pstepper->lenght, i);
                return ESP_FAIL;                        
            }
            if (direction > 0) {
                pstepper->current_position = pstepper->current_position - 1;
            } else {
                pstepper->current_position = pstepper->current_position + 1;
            }
            ESP_LOGD(TAG, "Checks passed, current step: %d\n", i);
            gpio_set_level((gpio_num_t)pstepper->step_io, 1);
            ets_delay_us(3000);
            gpio_set_level((gpio_num_t)pstepper->step_io, 0);
            ets_delay_us(3000);
            if (i % 500 == 0) {
                vTaskDelay(1);
            }
        }
        return ESP_OK;
    }

    esp_err_t IOT4988Stepper::calibrate(int lenght)
    {
        ESP_LOGI(TAG, "Start calibration, up state: %d, lenhgt param: %d\n", getHiState(), lenght);
        pstepper->current_position = 0;
        pstepper->lenght = 500000;
        int calibrated_lenght = 0;
        // move to up
        esp_err_t result;
        do
        {
            result = step(UP, 1);
            if (result == ESP_OK) {
                calibrated_lenght++;
            }
        } while (result == ESP_OK);
        ESP_LOGI(TAG, "Up position arrived, calibrated lenght = %d\n", calibrated_lenght);
        if (lenght == 0) {
            pstepper->lenght = calibrated_lenght;
        } else {
            pstepper->lenght = lenght;
        }
        pstepper->current_position = 0;
        return ESP_OK;
    }

    esp_err_t IOT4988Stepper::move(int position) {
        if (position < 0 || position > 100) {
            return ESP_FAIL;
        }
        if (pstepper->lenght == 500000) {
            return ESP_FAIL;
        }
        int expected_position = pstepper->lenght * (position/100.0);
        int steps = expected_position - pstepper->current_position;
        int direction;
        if (steps < 0) {
            // we should move up
            direction = UP;
            steps = abs(steps);
        } else {
            // we should mode down
            direction = DOWN;
        }
        ESP_LOGI(TAG, "Move current: %d, expected: %d, requested: %d%%, steps: %d, direction: %d\n", pstepper->current_position, expected_position, position, steps, direction);
        esp_err_t result;
        for (int i=0; i<steps; i++) {
            result = step(direction, 1);
            if (result == ESP_FAIL) {
                return ESP_FAIL;
            }
        }
        return ESP_OK;
    }
    stepper_dev_t *IOT4988Stepper::get_stepper_config() {
        return pstepper;
    }

    IOT4988Stepper::~IOT4988Stepper()
    {
        free(pstepper);
        pstepper = NULL;
    }

#ifdef __cplusplus
}
#endif