#include "mcp23xgpio.h"

#ifdef __cplusplus
extern "C"
{
#endif


#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include <string.h>


#ifdef __cplusplus
}
#endif

static const char *MCP23X_TAG = "MCP23X_TAG";

xQueueHandle Mcp23xGpio::mcp23x_gpio_isr_evt_queue = NULL;
TaskHandle_t Mcp23xGpio::gpio_isr_evt_task_xHandle = NULL;
uint16_t Mcp23xGpio::io_state = 0;
uint16_t Mcp23xGpio::io_mode = 0;
uint16_t Mcp23xGpio::io_start_addr = 0;
mcp23x17_t Mcp23xGpio::dev;
void (*Mcp23xGpio::state_cb)(int, int) = NULL;

Mcp23xGpio::Mcp23xGpio(int addr_config, uint16_t mode, int start_io, gpio_num_t sda_gpio, gpio_num_t scl_gpio, gpio_num_t inta_gpio) {
    io_mode = mode;
    io_start_addr = start_io;
    mcp23x_gpio_isr_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate((TaskFunction_t)mcp23x_isr_evt_task, "mcp23x_isr_evt_task", 4096, NULL, 10, NULL);
    memset(&dev, 0, sizeof(mcp23x17_t));
    
    ESP_ERROR_CHECK(mcp23x17_init_desc(&dev, 0, MCP23X17_ADDR_BASE + addr_config, sda_gpio, scl_gpio));

    
    mcp23x17_port_set_mode(&dev, io_mode);

    /*mcp23x17_set_mode(&dev, 0, MCP23X17_GPIO_INPUT);
    mcp23x17_set_mode(&dev, 1, MCP23X17_GPIO_INPUT);
    mcp23x17_set_mode(&dev, 2, MCP23X17_GPIO_INPUT);
    mcp23x17_set_mode(&dev, 3, MCP23X17_GPIO_INPUT);
    mcp23x17_set_mode(&dev, 4, MCP23X17_GPIO_INPUT);
    mcp23x17_set_mode(&dev, 5, MCP23X17_GPIO_INPUT);
    mcp23x17_set_mode(&dev, 6, MCP23X17_GPIO_INPUT);
    mcp23x17_set_mode(&dev, 7, MCP23X17_GPIO_INPUT);*/

    ESP_LOGI(MCP23X_TAG, "MCP23017 PORT OK with mode: %d", io_mode);
    mcp23x17_port_set_interrupt(&dev, mode, MCP23X17_INT_ANY_EDGE);
    gpio_set_direction(inta_gpio, GPIO_MODE_INPUT);
    gpio_set_intr_type(inta_gpio, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(inta_gpio, mcp23x_intr_handler, (void*)inta_gpio);
    ESP_LOGI(MCP23X_TAG, "MCP23017 ISR OK");

    mcp23x17_port_read(&dev, &io_state);
    io_state = io_mode & io_state; //ignore output
    uint16_t dev_mode = 0;
    mcp23x17_port_get_mode(&dev, &dev_mode);
    ESP_LOGI(MCP23X_TAG, "Mode configured as %d", dev_mode);
}

void IRAM_ATTR Mcp23xGpio::mcp23x_intr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(mcp23x_gpio_isr_evt_queue, &gpio_num, NULL);
}

void Mcp23xGpio::mcp23x_isr_evt_task(void* arg) {
        int io_num;
        ESP_LOGI(MCP23X_TAG, "gpio_isr_evt_man_task started");
        for(;;) {
                if (pdTRUE == xQueueReceive(mcp23x_gpio_isr_evt_queue, &io_num, portMAX_DELAY)) {
                        int int_pin_state = gpio_get_level((gpio_num_t)io_num);
                        ESP_LOGI(MCP23X_TAG, "GPIO[%d] intr, val:%d", io_num, int_pin_state);
                        uint16_t current_pin_state;
                        mcp23x17_port_read(&dev, &current_pin_state);
                        current_pin_state = current_pin_state & io_mode;
                        uint16_t diff = current_pin_state ^ io_state;
                        for(int i = 0; i < 16; i++, diff = diff << 1) {                                    
                                if (diff & 32768) {
                                    // 1
                                    int bit_number = 16 - (i + 1);
                                    int bit_value = (current_pin_state & ( 1 << bit_number )) >> bit_number;
                                    ESP_LOGI(MCP23X_TAG, "GPIO MCP pin[%d] changed to: %d", bit_number + io_start_addr, bit_value);
                                    ESP_LOGI(MCP23X_TAG, "DEBUG current[%d] diff is %d", current_pin_state, diff);
                                    if (state_cb != NULL) {
                                        state_cb( bit_number + io_start_addr, bit_value);
                                    }
                                }
                        }
                        io_state = current_pin_state;
                }
        }
}

esp_err_t Mcp23xGpio::set_level(int pin, bool level) {
    //ESP_LOGI(MCP23X_TAG, "dev%d", &dev);
    int mcp_pin = pin - io_start_addr;
    ESP_LOGI(MCP23X_TAG, "Set pin %d to level %d, mcp_pin=%d", pin, level, mcp_pin);
    if (mcp_pin < 0 || mcp_pin > 15) {
        return ESP_FAIL;
    }
    mcp23x17_set_level(&dev, mcp_pin, level);
    return ESP_OK;
}

