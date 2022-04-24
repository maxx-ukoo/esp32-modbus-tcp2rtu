#ifndef MODBUS_H
#define MODBUS_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include "esp_err.h"
#include "esp_event.h"

class IOTModbus
{
    private:
        IOTModbus(const IOTModbus&);
        IOTModbus& operator =(const IOTModbus&);
        static xQueueHandle tcp2rtu_queue;
        static xQueueHandle rtu2tcp_queue;
    public:
        static esp_err_t reserve_pins(esp_err_t (*func)(int, int));
        static esp_err_t modbus_start(int port_speed);
        static esp_err_t modbus_init(int port_speed);
        static void tcp_server_task(void *pvParameters);
        static void modbus_tcp_slave_task(void *pvParameters);
        static void eth2rtu_flow_control_task(void *args);
        static esp_err_t initialize_flow_control(void);

    ~IOTModbus(void);
};

#ifdef __cplusplus
}
#endif
#endif /* MODBUS_H */
