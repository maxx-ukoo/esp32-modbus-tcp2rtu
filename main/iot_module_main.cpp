/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include <stdio.h>
#include <string.h>

extern "C" {
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_netif.h"
    #include "tcpip_adapter_types.h"
    #include "cJSON.h"
    #include "esp_sntp.h"
    #include "esp_event.h"
    #include "esp_log.h"
    #include "esp_spiffs.h"
    #include "sdkconfig.h"
    #include "esp_partition.h"
    #include "modbus\modbus_const.h"
    #include <i2cdev.h>
    #include "mcp23xgpio\mcp23xgpio.h"

    //#include "mbcontroller.h"       // for mbcontroller defines and api
    //#include "mb.h"
    //#include "modbus_params.h"      // for modbus parameters structures
    //#include "esp_check.h"

}

#include "config/config.h"
#include "gpiov2/gpio_v2.h"
#include "mqtt/mqtt.h"
#include "ui/ui_server.h"
#include "ui/ota_handler.h"
#include "time_utils.h"
#include "modbus/modbus.h"
#include "curtains/curtains.h"

//#include <esp_https_server.h>

#define MB_TCP_PORT_NUMBER  502
#define MB_PAR_INFO_GET_TOUT                (10) // Timeout for get parameter info
#define MB_CHAN_DATA_MAX_VAL                (10)
#define MB_CHAN_DATA_OFFSET                 (1.1f)

#define MB_READ_MASK (MB_EVENT_INPUT_REG_RD | MB_EVENT_HOLDING_REG_RD | MB_EVENT_DISCRETE_RD | MB_EVENT_COILS_RD)

#define MB_WRITE_MASK (MB_EVENT_HOLDING_REG_WR | MB_EVENT_COILS_WR)

#define MB_READ_WRITE_MASK  (MB_READ_MASK | MB_WRITE_MASK)

#define SDA_GPIO 33
#define SCL_GPIO 5
#define INTA_1_GPIO 35
#define INTB_2_GPIO 34

// Make app_main look like a C function to the linker.
extern "C" {
   void app_main(void);
}

static const char *TAG = "IOT DIN Module";
static esp_netif_t *eth_netif = NULL;
static GpioV2* gpio = new GpioV2();
static Mcp23xGpio* mcp[8];
static bool i2c_ok = false;

static void connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        *server = ui_http_webserver_start(CONFIG_EXAMPLE_WEB_MOUNT_POINT);
    }
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        httpd_ssl_stop(*server);
        *server = NULL;
    }
}

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
    case ETHERNET_EVENT_START: {
        ESP_LOGI(TAG, "Ethernet Started");
        cJSON *config = IOTConfig::readConfig();
        cJSON *mqtt = cJSON_GetObjectItem(config, "mqtt");
        ESP_LOGI(TAG, "Set hostname to %s", cJSON_GetObjectItem(mqtt, "host")->valuestring);
        esp_netif_set_hostname(eth_netif, cJSON_GetObjectItem(mqtt, "host")->valuestring);
        cJSON_Delete(config);
        break;
    }
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
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    
}

esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_EXAMPLE_WEB_MOUNT_POINT,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ESP_OK;
}

void gpio_cb_manager(int pin, int state) {
    if (gpio->set_pin_state(pin, state) == ESP_OK) {
        ESP_LOGI(TAG, "Level set OK for GPIO");
        return;
    }
    ESP_LOGI(TAG, "Set level to MCP23");
    for (int i=0; i<8; i++) {
        if (mcp[i] == 0) {
            break;
        }
        if (mcp[i]->set_level(pin, state) == ESP_OK) {
            break;
        }
    }
}

void curtains_cb_manager(int curtain, int command, int param1, int param2) {
    IOTCurtains::command(curtain, command, param1, param2);
}

void components_start(void) {
    cJSON *config = IOTConfig::readConfig();
    cJSON *modbus = cJSON_GetObjectItem(config, "modbus");
    ESP_LOGI(TAG, "Initializing GpioV2 5");
    if (cJSON_GetObjectItem(modbus, "enable")->valueint) {
        gpio->configure_pin(CONFIG_MB_UART_RXD, MODE_MODBUS);
        gpio->configure_pin(CONFIG_MB_UART_TXD, MODE_MODBUS);
        gpio->configure_pin(CONFIG_MB_UART_RTS, MODE_MODBUS);
        ESP_ERROR_CHECK(IOTModbus::modbus_start(cJSON_GetObjectItem(modbus, "speed")->valueint));
    }

    cJSON *curtains = cJSON_GetObjectItem(config, "curtains");
    ESP_LOGI(TAG, "Initializing curtains");
    int curtains_count = 0;
    const cJSON *curtain = NULL;
    cJSON_ArrayForEach(curtain, curtains)
    {
        int io_step = cJSON_GetObjectItemCaseSensitive(curtain, "io_step")->valueint;
        int io_dir = cJSON_GetObjectItemCaseSensitive(curtain, "io_dir")->valueint;
        int io_hi_pos = cJSON_GetObjectItemCaseSensitive(curtain, "io_hi_pos")->valueint;
        gpio->configure_pin(io_step, MODE_CURTAINS);
        gpio->configure_pin(io_dir, MODE_CURTAINS);
        gpio->configure_pin(io_hi_pos, MODE_CURTAINS);
        curtains_count++;
    }
    ESP_LOGI(TAG, "Found %d curtains", curtains_count);
    IOTCurtains::curtains_json_init(curtains);
    
    cJSON *gpioConfig = cJSON_GetObjectItem(config, "gpio");
    cJSON *mqtt = cJSON_GetObjectItem(config, "mqtt");
    IOTMqtt::mqtt_json_init(mqtt);

    IOTMqtt::gpio_command_cb = &gpio_cb_manager;
    IOTMqtt::curtains_cb_manager = &curtains_cb_manager;
    
    gpio->configure_pin(SDA_GPIO, MODE_I2C);
    gpio->configure_pin(SCL_GPIO, MODE_I2C);


    const cJSON *pin = NULL;
    cJSON_ArrayForEach(pin, gpioConfig)
    {
        int gpioPin = cJSON_GetObjectItemCaseSensitive(pin, "id")->valueint;
        int mode = cJSON_GetObjectItemCaseSensitive(pin, "mode")->valueint;
        int pull_up = cJSON_GetObjectItemCaseSensitive(pin, "pull_up")->valueint;
        int pull_down = cJSON_GetObjectItemCaseSensitive(pin, "pull_down")->valueint;
        gpio->configure_pin(gpioPin, mode);
    }

    cJSON *gpioMCP = cJSON_GetObjectItem(config, "pcf8574");
    const cJSON *mcpConfig = NULL;
    int mcpNum = 0;
    ESP_LOGI(TAG, "MCP check config");
    cJSON_ArrayForEach(mcpConfig, gpioMCP)
    {
        int intIO = cJSON_GetObjectItemCaseSensitive(mcpConfig, "io_int")->valueint;
        ESP_LOGI(TAG, "MCP start init 0");
        gpio->configure_pin(intIO, MODE_EX_INT);
        mcpNum++;
    }
    if (mcpNum > 0) {
        ESP_LOGI(TAG, "I2C init");
        if (i2cdev_init() == ESP_OK) {
            i2c_ok = true;
        }
        ESP_LOGI(TAG, "I2C init finished");        
    }
    ESP_LOGI(TAG, "Initializing GpioV2");
    gpio->state_cb = &IOTMqtt::gpio_update_state_cb;
    gpio->start();

    mcpNum = 0;
    ESP_LOGI(TAG, "MCP starting");
    cJSON_ArrayForEach(mcpConfig, gpioMCP)
    {
        int intIO = cJSON_GetObjectItemCaseSensitive(mcpConfig, "io_int")->valueint;
        ESP_LOGI(TAG, "MCP start init 0");
        int startIO = cJSON_GetObjectItemCaseSensitive(mcpConfig, "io_start")->valueint;
        ESP_LOGI(TAG, "MCP start init 1");
        int addrOffset = cJSON_GetObjectItemCaseSensitive(mcpConfig, "addr_offset")->valueint;
        ESP_LOGI(TAG, "MCP start init 2");
        int ioConfig = cJSON_GetObjectItemCaseSensitive(mcpConfig, "io_config")->valueint;
        ESP_LOGI(TAG, "MCP start init 3");
        ESP_LOGI(TAG, "Initializing MCP addr: %d, conf: %d, startIO: %d, intIO: %d", addrOffset, ioConfig, startIO, intIO);
        mcp[mcpNum] = new Mcp23xGpio(addrOffset, ioConfig, startIO, (gpio_num_t)SDA_GPIO, (gpio_num_t)SCL_GPIO, (gpio_num_t)intIO);
        ESP_LOGI(TAG, "MCP start init 4");
        mcp[mcpNum]->state_cb = &IOTMqtt::gpio_update_state_cb;
        ESP_LOGI(TAG, "MCP start init 5");
        mcpNum++;
    }

}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

void app_main(void)
{
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(init_fs());
    vTaskDelay(100);
    
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&cfg);
    
    // Set default handlers to process TCP/IP stuffs
    ESP_ERROR_CHECK(esp_eth_set_default_handlers(eth_netif));
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
    // phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
    mac_config.smi_mdc_gpio_num = CONFIG_EXAMPLE_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_EXAMPLE_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
    //esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    
    /* attach Ethernet driver to TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    /* start Ethernet driver state machine */
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    
    xTaskCreate(&ota_system_reboot_task, "ota_system_reboot_task", 2048, NULL, 5, NULL);
    
    initialize_sntp();
    components_start();
}
