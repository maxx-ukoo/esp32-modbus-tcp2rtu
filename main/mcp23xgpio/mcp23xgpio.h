#ifndef MCP23xGPIO_V2_H
#define MCP23xGPIO_V2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <mcp23x17.h>


class Mcp23xGpio {
    public: 
        Mcp23xGpio(int addr_config, uint16_t mode, int start_io, gpio_num_t sda_gpio, gpio_num_t scl_gpio, gpio_num_t inta_gpio);
        esp_err_t set_level(int pin, bool level);
        static void (*state_cb)(int, int);
    private:
        static mcp23x17_t dev;
        static xQueueHandle mcp23x_gpio_isr_evt_queue;
        static TaskHandle_t gpio_isr_evt_task_xHandle;

        static void IRAM_ATTR mcp23x_intr_handler(void *arg);
        static void mcp23x_isr_evt_task(void* arg);
        static uint16_t io_state;
        static uint16_t io_mode;
        static uint16_t io_start_addr;
    ~Mcp23xGpio(void);
};

#ifdef __cplusplus
}
#endif
#endif /* MCP23xGPIO_V2_H */


