#include "curtains.h"
#include "gpio/gpio.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#define COMMAND_QUEUE_LENGTH (5)
#define FLOW_CONTROL_QUEUE_TIMEOUT_MS (100)
#define COMMAND_CALIBRATE   0
#define COMMAND_STEP        1
#define COMMAND_MOVE        2
static const char *TAG = "CURTAINS";


IOT4988Stepper* IOTCurtains::curtains[3];
int IOTCurtains::curtains_count = 0;
xQueueHandle IOTCurtains::command2executor_queue = NULL;
TaskHandle_t IOTCurtains::command_executor_task_xHandle = NULL;
IOTCurtains::IOTCurtains(cJSON *config)
{
    curtains_json_init(config);
}

esp_err_t IOTCurtains::curtains_json_init(cJSON *config) {
    const cJSON *curtain = NULL;
    curtains_count = 0;
    command2executor_queue = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(curtains_command_msg_t));
    if (!command2executor_queue) {
        ESP_LOGE(TAG, "create command2executor_queue queue failed");
        return ESP_FAIL;
    }
    if (command_executor_task_xHandle == NULL) {
        command_executor_task_xHandle = (TaskHandle_t)xTaskCreate(IOTCurtains::curtains_command_executor_task, "curtains_command_executor_task", 4096, NULL, 5, NULL);
        //mqtt2gpio_task_xHandle = (TaskHandle_t)xTaskCreate((TaskFunction_t)mqtt2gpio_task, "mqtt2gpio_task", 2048, NULL, 10, NULL);
    }

    cJSON_ArrayForEach(curtain, config)
    {
        int io_step = cJSON_GetObjectItemCaseSensitive(curtain, "io_step")->valueint;
        int io_dir = cJSON_GetObjectItemCaseSensitive(curtain, "io_dir")->valueint;
        int io_hi_pos = cJSON_GetObjectItemCaseSensitive(curtain, "io_hi_pos")->valueint;
        int lenght = cJSON_GetObjectItemCaseSensitive(curtain, "lenght")->valueint;
        curtains[curtains_count] = new IOT4988Stepper(io_step, io_dir, io_hi_pos);
        IOTGpio::reservePin(io_step, MODE_CURTAINS);
        IOTGpio::reservePin(io_dir, MODE_CURTAINS);
        IOTGpio::reservePin(io_hi_pos, MODE_CURTAINS);
        curtains_command_msg_t msg = {
            .id = curtains_count,
            .command = COMMAND_CALIBRATE,
            .param1 = lenght
        };
        if (xQueueSend(command2executor_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) != pdTRUE) {
                ESP_LOGE(TAG, "send flow control message failed or timeout");
        }
        ESP_LOGI(TAG, "Curtain initialized, count: %d, lenght: %d", curtains_count, lenght);
        curtains_count++;
    }
    return ESP_OK;
}

cJSON *IOTCurtains::get_curtains_config() {
    cJSON *config = cJSON_CreateArray();
    for (int i=0; i<curtains_count; i++) {
        cJSON *curtain = cJSON_CreateObject();
        cJSON_AddItemToArray(config, curtain);
        cJSON *io_step = cJSON_CreateNumber(curtains[i]->get_stepper_config()->step_io);
        cJSON_AddItemToObject(curtain, "io_step", io_step);
        cJSON *io_dir = cJSON_CreateNumber(curtains[i]->get_stepper_config()->dir_io);
        cJSON_AddItemToObject(curtain, "io_dir", io_dir);
        cJSON *io_hi_pos = cJSON_CreateNumber(curtains[i]->get_stepper_config()->hi_pos_io);
        cJSON_AddItemToObject(curtain, "io_hi_pos", io_hi_pos);
        cJSON *lenght = cJSON_CreateNumber(curtains[i]->get_stepper_config()->lenght);
        cJSON_AddItemToObject(curtain, "lenght", lenght);
    }
    return config;
}

esp_err_t IOTCurtains::step(int curtain, int direction, int steps) {
    curtains_command_msg_t msg = {
        .id = curtain,
        .command = COMMAND_STEP,
        .param1 = direction,
        .param2 = steps
    };
    if (xQueueSend(command2executor_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "send flow control message failed or timeout");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t IOTCurtains::calibrate(int curtain) {
    curtains_command_msg_t msg = {
        .id = curtain,
        .command = COMMAND_CALIBRATE,
        .param1 = 0
    };
    if (xQueueSend(command2executor_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "send flow control message failed or timeout");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t IOTCurtains::move(int curtain, int position) {
     curtains_command_msg_t msg = {
        .id = curtain,
        .command = COMMAND_MOVE,
        .param1 = position
    };
    if (xQueueSend(command2executor_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "send flow control message failed or timeout");
        return ESP_FAIL;
    }
    return ESP_OK;
}

IOTCurtains::~IOTCurtains()
{
    for (int i=0; i<curtains_count; i++) {
        free(curtains[i]);
        curtains[i] = NULL;
    }
}

void IOTCurtains::curtains_command_executor_task(void *pvParameters) {
    curtains_command_msg_t msg;
    int bytes;
    char tx_buffer[128];
    while (1) {
        if (xQueueReceive(command2executor_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) == pdTRUE) {
            ESP_LOGI(TAG, "Curtains command received, id: %d, command: %d, param1: %d, param2: %d", msg.id, msg.command, msg.param1, msg.param2);
            if (msg.command == COMMAND_CALIBRATE) {
                if (curtains[msg.id] != NULL) {
                    curtains[msg.id]->calibrate(msg.param1);        
                }
            }
            if (msg.command == COMMAND_STEP) {
                if (curtains[msg.id] != NULL) {
                    curtains[msg.id]->step(msg.param1, msg.param2);
                }
            }
            if (msg.command == COMMAND_MOVE) {
                if (curtains[msg.id] != NULL) {
                    curtains[msg.id]->move(msg.param1);
                }
            }
            ESP_LOGI(TAG, "Curtains command finished");
        } else {
            vTaskDelay(1);
        }
    }
    vTaskDelete(NULL);

}