#ifndef CURTAINS_H
#define CURTAINS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "iot_stepper_a4988.h"

typedef struct {
    int id;
    int command;
    int param1;
    int param2;
} curtains_command_msg_t;

class IOTCurtains
{
private:
    static IOT4988Stepper* curtains[3];
    static int curtains_count;
    static TaskHandle_t command_executor_task_xHandle;
    static void curtains_command_executor_task(void *pvParameters);
    IOTCurtains(const IOTCurtains&);
    IOTCurtains& operator =(const IOTCurtains&);
    

public:
    static xQueueHandle curtains_command2executor_queue;
    /**
     * @brief Constructor for IOTCurtains class
     * @param curtains JSON object with parameters
     */
    IOTCurtains(cJSON *curtains);

    /**
     * @brief Return JSON onject with current object
     * @return ESP_OK if success
     *         ESP_FAIL if error occured
     */
    static cJSON *get_curtains_config();
    static esp_err_t step(int curtain, int direction, int steps);
    static esp_err_t calibrate(int curtain);
    static esp_err_t move(int curtain, int position);
    static esp_err_t curtains_json_init(cJSON *curtains);
    static esp_err_t command(int curtain, int command, int param1, int param2);
    
    /**
     * @brief Destructor function of IOTCurtains object
     */
    ~IOTCurtains(void);
};

#ifdef __cplusplus
}
#endif
#endif /* CURTAINS_H */
