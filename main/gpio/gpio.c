#include "gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

#include "freertos/task.h"

#include "freertos/queue.h"

static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

void gpioInit() {
    gpio_config_t io_conf;
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    gpio_install_isr_service(0);

    for (int i = 0; i < GPIO_NUMBER; ++i) {
        gpio_isr_handler_remove(gpioConfig[i][ID]);
        if (gpioConfig[i][MODE] == 0)
            io_conf.intr_type = GPIO_INTR_ANYEDGE; //Enable interrupt for input pins
        else
            io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
        if (gpioConfig[i][MODE] == 0)
            io_conf.mode = GPIO_MODE_INPUT;
        else
            io_conf.mode = GPIO_MODE_OUTPUT;

        io_conf.pin_bit_mask = (1ULL<<gpioConfig[i][ID]);
        io_conf.pull_down_en = gpioConfig[i][PULL_U];
        io_conf.pull_up_en = gpioConfig[i][PULL_D];
        gpio_config(&io_conf);
        
        //hook isr handler for specific gpio pin

        gpio_isr_handler_add(gpioConfig[i][ID], gpio_isr_handler, (void*) gpioConfig[i][ID]);

    }


}