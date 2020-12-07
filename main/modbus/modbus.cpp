#include "modbus_const.h"
#include "modbus.h"
#ifdef __cplusplus
extern "C"
{
#endif
    #include <stdio.h>
    #include <string.h>
    #include "freertos/FreeRTOS.h"
    #include "esp_netif.h"
    #include "esp_log.h"
    #include "esp_err.h"
    #include "lwip/sockets.h"
    #include "esp_modbus_master.h"
#ifdef __cplusplus
}
#endif

#include "gpio/gpio.h"


static const char *MODBUS_TAG = "modbus";

#define SENSE_MB_CHECK(a, ret_val, str, ...) \
    if (!(a)) { \
        ESP_LOGE(MODBUS_TAG, "%s(%u): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        return (ret_val); \
    }

xQueueHandle IOTModbus::tcp2rtu_queue = NULL;
xQueueHandle IOTModbus::rtu2tcp_queue = NULL;

esp_err_t IOTModbus::modbus_start(int port_speed) {
    esp_err_t err = modbus_init(port_speed);
    if (err != 0) {
        ESP_LOGE(MODBUS_TAG, "Error occurred during init modbus: err %d", err);
        return ESP_FAIL;
    }
    err = initialize_flow_control();
    if (err != 0) {
        ESP_LOGE(MODBUS_TAG, "Error occurred during init modbus flow: err %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t IOTModbus::modbus_init(int port_speed)
{
    mb_communication_info_t comm;
    comm.port = MB_PORTNUM;
    comm.mode = MB_MODE_RTU;
    comm.baudrate = port_speed;
    comm.parity = MB_PARITY;

    void* master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    SENSE_MB_CHECK((master_handler != NULL), ESP_ERR_INVALID_STATE,
                                "mb controller initialization fail.");
    SENSE_MB_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            "mb controller initialization fail, returns(0x%x).",
                            (uint32_t)err);
    err = mbc_master_setup((void*)&comm);
    SENSE_MB_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            "mb controller setup fail, returns(0x%x).",
                            (uint32_t)err);
    err = mbc_master_start();
    SENSE_MB_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            "mb controller start fail, returns(0x%x).",
                            (uint32_t)err);
    // Set UART pin numbers
    err = uart_set_pin(MB_PORTNUM, CONFIG_MB_UART_TXD, CONFIG_MB_UART_RXD,
                                    CONFIG_MB_UART_RTS, UART_PIN_NO_CHANGE);
    SENSE_MB_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
            "mb serial set pin failure, uart_set_pin() returned (0x%x).", (uint32_t)err); 
    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORTNUM, UART_MODE_RS485_HALF_DUPLEX);
    SENSE_MB_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
            "mb serial set mode failure, uart_set_mode() returned (0x%x).", (uint32_t)err);
    vTaskDelay(5);
    IOTGpio::reservePin(CONFIG_MB_UART_RXD, MODE_MODBUS);
    IOTGpio::reservePin(CONFIG_MB_UART_TXD, MODE_MODBUS);
    IOTGpio::reservePin(CONFIG_MB_UART_RTS, MODE_MODBUS);

    return err;
}

void IOTModbus::modbus_tcp_slave_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
    inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(MODBUS_TAG, "Unable to create socket: errno %d", errno);
        //break;
    }
    ESP_LOGI(MODBUS_TAG, "Socket created");
    //setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(MODBUS_TAG, "Socket unable to bind: errno %d", errno);
    }
    ESP_LOGI(MODBUS_TAG, "Socket bound, port %d", PORT);

    while (1) {
        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(MODBUS_TAG, "Error occurred during listen: errno %d", errno);
            break;
        }
        ESP_LOGI(MODBUS_TAG, "Socket listening");

        struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
        uint addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(MODBUS_TAG, "Unable to accept connection: errno %d", errno);
            break;
        }
        ESP_LOGI(MODBUS_TAG, "Socket accepted");

        while (1) {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(MODBUS_TAG, "recv failed: errno %d", errno);
                break;
            }
            // Connection closed
            else if (len == 0) {
                ESP_LOGI(MODBUS_TAG, "Connection closed");
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(MODBUS_TAG, "Received %d bytes from %s:", len, addr_str);
                //ESP_LOGI(MODBUS_TAG, "%s", rx_buffer);

                flow_control_msg_t msg = {
                    .message = rx_buffer,
                    .length = (uint16_t)len
                };
                if (xQueueSend(tcp2rtu_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) != pdTRUE) {
                    ESP_LOGE(MODBUS_TAG, "send flow control message failed or timeout");
                    //free(buffer);
                }
                ESP_LOGI(MODBUS_TAG, "Flow message send");
                vTaskDelay(50);
                if (xQueueReceive(rtu2tcp_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) == pdTRUE) {
                    ESP_LOGW(MODBUS_TAG, "Received packet from rtu, len: %d", msg.length);
                        //ESP_LOGI(MODBUS_TAG, "==========  DUMP RTU IN START==========");
                        //for (int i = 0; i<msg.length; i++) 
                        //    ESP_LOGI(MODBUS_TAG, "=>: %d", *(msg.message + i));
                        //ESP_LOGI(MODBUS_TAG, "==========  DUMP RTU IN END ==========");
                    int err = send(sock, msg.message, msg.length, 0);
                    if (err < 0) {
                        ESP_LOGE(MODBUS_TAG, "Error occurred during sending tcp responce: errno %d", errno);
                        //break;
                    }
                }
                ESP_LOGI(MODBUS_TAG, "Responce to host send");
            }
        }

        if (sock != -1) {
            ESP_LOGE(MODBUS_TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void IOTModbus::eth2rtu_flow_control_task(void *args)
{
    flow_control_msg_t msg;
    //uint32_t timeout = 0;
    int bytes;
    char tx_buffer[128];
    while (1) {
        if (xQueueReceive(tcp2rtu_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) == pdTRUE) {
            //Therefore, *(balance + 4) is a legitimate way of accessing the data at balance[4].
            ESP_LOGI(MODBUS_TAG, "Request for modbus received");

            //// rutine Modbus TCP
            //int _msg_id = (*(msg.message + MB_TCP_TID) << 8) + *(msg.message + MB_TCP_TID+1);
            //int protocol = (*(msg.message + MB_TCP_PID) << 8) + *(msg.message + MB_TCP_PID+1);
            int start = (*(msg.message + MB_TCP_REGISTER_START) << 8) + *(msg.message + MB_TCP_REGISTER_START+1);
            int len = (*(msg.message + MB_TCP_LEN) << 8) + *(msg.message + MB_TCP_LEN+1);
            int regNumber = (*(msg.message + MB_TCP_REGISTER_NUMBER) << 8) + *(msg.message + MB_TCP_REGISTER_NUMBER+1);
            int slaveId = *(msg.message + MB_TCP_UID);
            int function = *(msg.message + MB_TCP_FUNC);

            //ESP_LOGI(TAG, "==========  Modbus message ==========");
            //ESP_LOGI(TAG, "_msg_id: %d", _msg_id);
            //ESP_LOGI(TAG, "protocol: %d", protocol);
            //ESP_LOGI(TAG, "start: %d", start);
            //ESP_LOGI(TAG, "regNumber: %d", regNumber);
            //ESP_LOGI(TAG, "len: %d", len);
            //ESP_LOGI(TAG, "Slave ID: %d", slaveId);
            //ESP_LOGI(TAG, "Function: %d", function);
   
            tx_buffer[0] = *(msg.message + MB_TCP_TID);
            tx_buffer[1] = *(msg.message + MB_TCP_TID + 1);
            
            tx_buffer[2] = 0;
            tx_buffer[3] = 0;                    
            
            bytes = regNumber * 2;
            tx_buffer[4] = 0;
            tx_buffer[5] = bytes + 3; //Number of bytes after this one.
            
            tx_buffer[6] = slaveId;
            tx_buffer[7] = function;
            
            // Execute modbus request
            mb_param_request_t modbus_request = {
                (uint8_t)slaveId,
                (uint8_t)function,
                (uint16_t)start,
                (uint16_t)len
            };

            uint8_t param_buffer[24] = { 0 };
            uint8_t length = bytes + 9;
            esp_err_t err = mbc_master_send_request(&modbus_request, &param_buffer[0]);

            if (err == ESP_OK) {
                ESP_LOGI(MODBUS_TAG, "Got responce from modbus slave OK");
                tx_buffer[8] = bytes; //Number of bytes after this one (or number of bytes of data).
                switch(function)    {
                    case MB_FUNC_READ_INPUT_REGISTER:
                    case MB_FUNC_READ_HOLDING_REGISTER:
                        //ESP_LOGI(MODBUS_TAG, "==========  DUMP  IN ==========");
                        //for (int i = 0; i<msg.length; i++) 
                        //    ESP_LOGI(MODBUS_TAG, "=>: %d", *(msg.message + i));
                        //ESP_LOGI(MODBUS_TAG, "==========  DUMP IN ==========");
                        ESP_LOGI(MODBUS_TAG, "==========  DUMP FROM SLAVE START ==========");
                        for (int i = 0; i<regNumber; i++) {
                            ESP_LOGI(MODBUS_TAG, "=>: %d", *(param_buffer + i));
                        }
                        ESP_LOGI(MODBUS_TAG, "==========  DUMP FROM SLAVE END ==========");
                        for (int i = 0; i<regNumber; i++) {
                            tx_buffer[ 9 + i*2] = *(param_buffer + (i*2));
                            tx_buffer[ 9 + (i*2 + 1)] = *(param_buffer + i*2 + 1);
                        }    
                        break;
                    /*case MB_FUNC_READ_COILS:
                    case MB_FUNC_WRITE_SINGLE_COIL:
                    case MB_FUNC_WRITE_MULTIPLE_COILS:
                    case MB_FUNC_READ_DISCRETE_INPUTS:
                    
                    case MB_FUNC_WRITE_REGISTER:
                    case MB_FUNC_WRITE_MULTIPLE_REGISTERS:
                    case MB_FUNC_READWRITE_MULTIPLE_REGISTERS:*/
                    default:
                        for (int i = 0; i<bytes; i++) {
                            tx_buffer[ 9 + i] = *(param_buffer + i);
                        }  
                        break;
                }
            } else {
                ESP_LOGE(MODBUS_TAG, "Got responce from modbus slave with error %s", esp_err_to_name(err));
                tx_buffer[5] = 4;
                tx_buffer[7] = function + 128;;
                
                tx_buffer[8] = 1; //Number of bytes after this one (or number of bytes of data).
                tx_buffer[9] = 11; //Error code: Gateway Target Device Failed to Respond
                length = 10;
            }

            // return data to TCP socket
            flow_control_msg_t msg = {
                .message = tx_buffer,
                .length = length
            };
            if (xQueueSend(rtu2tcp_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) != pdTRUE) {
                ESP_LOGE(MODBUS_TAG, "send flow control message failed or timeout");
            }
            ESP_LOGI(MODBUS_TAG, "Response from modbus processed witj OK");
        }
    }
    vTaskDelete(NULL);
}

esp_err_t IOTModbus::initialize_flow_control(void)
{
    tcp2rtu_queue = xQueueCreate(FLOW_CONTROL_QUEUE_LENGTH, sizeof(flow_control_msg_t));
    if (!tcp2rtu_queue) {
        ESP_LOGE(MODBUS_TAG, "create tcp2rtu_queue queue failed");
        return ESP_FAIL;
    }
    rtu2tcp_queue = xQueueCreate(FLOW_CONTROL_QUEUE_LENGTH, sizeof(flow_control_msg_t));
    if (!rtu2tcp_queue) {
        ESP_LOGE(MODBUS_TAG, "create rtu2tcp_queue queue failed");
        return ESP_FAIL;
    }
    BaseType_t ret = xTaskCreate(IOTModbus::eth2rtu_flow_control_task, "flow_ctl", 2048, NULL, (tskIDLE_PRIORITY + 2), NULL);
    if (ret != pdTRUE) {
        ESP_LOGE(MODBUS_TAG, "create tcp2rtu_queue task failed");
        return ESP_FAIL;
    }
    ret = xTaskCreate(IOTModbus::modbus_tcp_slave_task, "modbus_tcp_slave_task", 4096, NULL, 5, NULL);
    if (ret != pdTRUE) {
        ESP_LOGE(MODBUS_TAG, "create modbus_tcp_slave_task task failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}