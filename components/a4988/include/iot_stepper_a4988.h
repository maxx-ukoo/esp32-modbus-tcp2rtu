#ifndef _IOT_STEPPER_A4988_H_
#define _IOT_STEPPER_A4988_H_


#ifdef __cplusplus
extern "C" {
#endif

//#include <stdio.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
#include "esp_system.h"
//#include "nvs_flash.h"
//#include "nvs.h"
//#include "driver/gpio.h"

typedef struct
{
    int step_io;
    int dir_io;
    int hi_pos_io;
    int current_position;
    int lenght;
} stepper_dev_t;

class IOT4988Stepper
{
private:
    stepper_dev_t* pstepper;

    IOT4988Stepper(const IOT4988Stepper&);
    IOT4988Stepper& operator =(const IOT4988Stepper&);
    esp_err_t getHiState();

public:
    /**
     * @brief Constructor for CA4988Stepper class
     * @param step_io Output GPIO for step signal
     * @param dir_io Output GPIO for direction signal
     * @param hi_pos_io Input GPIO for up position signal
     */
    IOT4988Stepper(int step_io, int dir_io, int hi_pos_io);

    /**
     * @brief To turn the motor a specific number of steps.
     * @param steps The number of steps to turn, negative or positive values determine the direction
     * @return ESP_OK if success
     *         ESP_ERR_TIMEOUT if operation timeout
     */
    esp_err_t step(int direction, int steps);

    /**
     * @brief Calculate lenght in steps, find low and hi positon, set to low.
     */
    esp_err_t calibrate(int lenght = 0);

    /**
     * @brief Move to position.
     * @param position The position as percentage value. 0% - open, 100% - close
     */
    esp_err_t move(int position);

    /**
     * @brief Get current config
     */
    stepper_dev_t *get_stepper_config();

    /**
     * @brief Destructor function of CA4988Stepper object
     */
    ~IOT4988Stepper(void);
};

#ifdef __cplusplus
}
#endif


#endif /* _IOT_STEPPER_A4988_H_ */