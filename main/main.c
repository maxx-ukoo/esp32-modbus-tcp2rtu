/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tcpip_adapter.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "esp_modbus_master.h"       // for Modbus defines
#include "main.h"


static const char *TAG = "esp32-modbus-tcp2rtu";

static xQueueHandle tcp2rtu_queue = NULL;
static xQueueHandle rtu2tcp_queue = NULL;

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const tcpip_adapter_ip_info_t *ip_info = &event->ip_info;
    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

static void tcp_server_task(void *pvParameters)
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
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        //break;
    }
    ESP_LOGI(TAG, "Socket created");
    //setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    while (1) {
        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
        uint addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket accepted");

        while (1) {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
            // Connection closed
            else if (len == 0) {
                ESP_LOGI(TAG, "Connection closed");
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
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);

                flow_control_msg_t msg = {
                    .message = rx_buffer,
                    .length = len
                };
                if (xQueueSend(tcp2rtu_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) != pdTRUE) {
                    ESP_LOGE(TAG, "send flow control message failed or timeout");
                    //free(buffer);
                }
                vTaskDelay(50);
                if (xQueueReceive(rtu2tcp_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) == pdTRUE) {
                    //ESP_LOGW(TAG, "Received packet from rtu, len: %d", msg.length);
                    int err = send(sock, msg.message, msg.length, 0);
                    if (err < 0) {
                        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                        //break;
                    }
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

static void eth2rtu_flow_control_task(void *args)
{
    flow_control_msg_t msg;
    //uint32_t timeout = 0;
    int bytes;
    char tx_buffer[128];
    while (1) {
        if (xQueueReceive(tcp2rtu_queue, &msg, pdMS_TO_TICKS(FLOW_CONTROL_QUEUE_TIMEOUT_MS)) == pdTRUE) {
            //Therefore, *(balance + 4) is a legitimate way of accessing the data at balance[4].

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
                slaveId,
                function,
                start,
                len
            };

            uint8_t param_buffer[24] = { 0 };
            uint8_t length = bytes + 9;
            esp_err_t err = mbc_master_send_request(&modbus_request, &param_buffer[0]);

            if (err == ESP_OK) {
                tx_buffer[8] = bytes; //Number of bytes after this one (or number of bytes of data).
                switch(function)    {
                    case MB_FUNC_READ_INPUT_REGISTER:
                        //ESP_LOGI(TAG, "==========  DUMP ==========");
                        //for (int i = 0; i<msg.length; i++) 
                        //    ESP_LOGI(TAG, "=>: %d", *(msg.message + i));
                        //ESP_LOGI(TAG, "==========  DUMP ==========");
                        for (int i = 0; i<regNumber; i++) {
                            tx_buffer[ 9 + i*2] = *(param_buffer + (i*2 + 1));
                            tx_buffer[ 9 + (i*2 + 1)] = *(param_buffer + i*2);
                        }    
                        break;
                    /*case MB_FUNC_READ_COILS:
                    case MB_FUNC_WRITE_SINGLE_COIL:
                    case MB_FUNC_WRITE_MULTIPLE_COILS:
                    case MB_FUNC_READ_DISCRETE_INPUTS:
                    case MB_FUNC_READ_HOLDING_REGISTER:
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
                ESP_LOGE(TAG, "send flow control message failed or timeout");
            }
        }
    }
    vTaskDelete(NULL);
}

static esp_err_t initialize_flow_control(void)
{
    tcp2rtu_queue = xQueueCreate(FLOW_CONTROL_QUEUE_LENGTH, sizeof(flow_control_msg_t));
    if (!tcp2rtu_queue) {
        ESP_LOGE(TAG, "create tcp2rtu_queue queue failed");
        return ESP_FAIL;
    }
    rtu2tcp_queue = xQueueCreate(FLOW_CONTROL_QUEUE_LENGTH, sizeof(flow_control_msg_t));
    if (!rtu2tcp_queue) {
        ESP_LOGE(TAG, "create rtu2tcp_queue queue failed");
        return ESP_FAIL;
    }
    BaseType_t ret = xTaskCreate(eth2rtu_flow_control_task, "flow_ctl", 2048, NULL, (tskIDLE_PRIORITY + 2), NULL);
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "create tcp2rtu_queue task failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// Initialization of Modbus stack
esp_err_t sense_modbus_init(void)
{
    mb_communication_info_t comm = {
            .port = MB_PORTNUM,
            .mode = MB_MODE_RTU,
            .baudrate = MB_BAUDRATE,
            .parity = MB_PARITY
    };
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

    return err;
}



void app_main(void)
{
    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(tcpip_adapter_set_default_eth_handlers());
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
    phy_config.phy_addr = 0;
    esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    ESP_ERROR_CHECK(initialize_flow_control());

    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
    sense_modbus_init();
}
