#include "ui_server_handler.h"
#ifdef __cplusplus
extern "C"
{
#endif
    #include "cJSON.h"
    #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
    #include "esp_log.h"
    #include "esp_image_format.h"
    #include "esp_ota_ops.h"
#ifdef __cplusplus
}
#endif
#include "config/config.h"
#include "mqtt/mqtt.h"
#include "iot_stepper_a4988.h"
#include "time_utils.h"

static const char *TAG = "REST Handler";

void (*gpio_command_cb)(int, int) = NULL;

static char *parse_uri(const char *uri)
{
    char *token = strtok((char *)uri, "/");
    char *latest = token;
    while (token != NULL)
    {
        latest = token;
        token = strtok(NULL, "/");
    }
    return latest;
}
esp_err_t component_control_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    ESP_LOGD(TAG, "API CONTROL URI : %s", req->uri);
    char *api_component_name = parse_uri(req->uri);

    if (strcmp(api_component_name, "modbus") == 0)
    {
        return modbus_control_post_handler(req, buf);
    }
    else if (strcmp(api_component_name, "mqtt") == 0)
    {
        return mqtt_control_post_handler(req, buf);
    }
    else if (strcmp(api_component_name, "gpio") == 0)
    {
        return gpio_control_post_handler(req, buf);
    }
    else if (strcmp(api_component_name, "reboot") == 0)
    {
        return system_reboot_post_handler(req, buf);
    }
    else if (strcmp(api_component_name, "state") == 0)
    {
        return gpio_control_state_post_handler(req, buf);
    }
    else if (strcmp(api_component_name, "level") == 0)
    {
        return gpio_control_level_post_handler(req, buf);
    }
    char resp[40];
    sprintf(resp, "Unknown component: %.20s", api_component_name);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
    return ESP_FAIL;
}

esp_err_t system_reboot_post_handler(httpd_req_t *req, char *buf)
{
    httpd_resp_sendstr(req, "OK");
    esp_restart();
    return ESP_OK;
}

esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddNumberToObject(root, "model", chip_info.model);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    cJSON_AddStringToObject(root, "date", app_desc->date);
    cJSON_AddStringToObject(root, "version", app_desc->version);
    cJSON_AddStringToObject(root, "time", app_desc->time);
    cJSON_AddStringToObject(root, "idf_ver", app_desc->idf_ver);
    cJSON_AddStringToObject(root, "uptime", esp_log_system_timestamp());
    cJSON_AddNumberToObject(root, "memory", esp_get_free_heap_size());

    cJSON_AddStringToObject(root, "start_time", get_start_time());
    char *str = get_current_local_time();
    cJSON_AddStringToObject(root, "current_time", str);

    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    free(str);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t modbus_control_post_handler(httpd_req_t *req, char *buf)
{
    cJSON *root = cJSON_Parse(buf);
    int enable = cJSON_GetObjectItem(root, "enable")->valueint;
    int speed = cJSON_GetObjectItem(root, "speed")->valueint;

    ESP_LOGI(TAG, "Modbus control: enable = %d, speed = %d", enable, speed);
    IOTConfig::writeModbusConfig(enable, speed);

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t mqtt_control_post_handler(httpd_req_t *req, char *buf)
{
    esp_err_t status = ESP_FAIL;
    cJSON *mqtt = cJSON_Parse(buf);
    if (mqtt == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        status = ESP_FAIL;
    }
    else
    {
        ESP_LOGD(TAG, "JSON received OK");
        cJSON *config = cJSON_GetObjectItem(mqtt, "mqtt");
        char *string = cJSON_Print(config);
        ESP_LOGD(TAG, "Got config: %s", string);
        status = IOTMqtt::mqtt_json_init(config);
        ESP_LOGD(TAG, "JSON processed OK with status %d", status);
        if (status == ESP_OK)
        {
            ESP_LOGD(TAG, "Writing mqtt config");
            IOTConfig::write_mqtt_config(config);
        }
        ESP_LOGD(TAG, "exiting,status %d", status);
    }

    // cJSON_Delete(gpio);
    if (status == ESP_OK)
    {
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }
    else
    {
        char resp[35];
        sprintf(resp, "MQTT configuring error: %d", status);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        return ESP_FAIL;
    }
}

esp_err_t gpio_control_post_handler(httpd_req_t *req, char *buf)
{
    esp_err_t status = ESP_FAIL;
    cJSON *gpio = cJSON_Parse(buf);
    if (gpio == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        status = 0;
    }
    else
    {
        ESP_LOGD(TAG, "JSON received OK");
        cJSON *pins = cJSON_GetObjectItem(gpio, "config");
        char *string = cJSON_Print(pins);
        ESP_LOGD(TAG, "Got pins: %s", string);
        status = ESP_OK;
        //status = IOTGpio::gpio_json_init(pins, true);
        ESP_LOGD(TAG, "JSON processed OK with status %d", status);
        if (status == ESP_OK)
        {
            ESP_LOGD(TAG, "Writing gpio config");
            IOTConfig::writeGpioConfig();
        }
        ESP_LOGD(TAG, "exiting,status %d", status);
    }

    // cJSON_Delete(gpio);
    if (status == ESP_OK)
    {
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }
    else
    {
        char resp[15];
        sprintf(resp, "Pin error %d", status);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        return ESP_FAIL;
    }
}

esp_err_t gpio_control_state_get_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    char param[32];

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    int pin = -1;
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = (char *)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "pin", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGD(TAG, "Found URL query parameter => query1=%s", param);
            }
        }
        free(buf);
        pin = atoi(param);
    }
    ESP_LOGD(TAG, "Get gpio state: pin = %d", pin);
    cJSON *res;
    if (pin != -1)
    {
        res = 0;//IOTGpio::get_json_pin_state(pin);
    }
    else
    {
        res = 0;//IOTGpio::getGpioState();
    }
    const char *resp_body = cJSON_Print(res);
    httpd_resp_sendstr(req, resp_body);
    free((void *)resp_body);
    cJSON_Delete(res);
    return ESP_OK;
}

esp_err_t gpio_control_state_post_handler(httpd_req_t *req, char *buf)
{
    cJSON *json_body = cJSON_Parse(buf);
    int pin = cJSON_GetObjectItem(json_body, "pin")->valueint;
    int state = cJSON_GetObjectItem(json_body, "state")->valueint;
    if (gpio_command_cb != NULL) {
            ESP_LOGI(TAG, "MQTT2GPIO_EVENT CB, pinn=%d, state=%d", pin, state);
            gpio_command_cb(pin, state);
    }
    cJSON *res = cJSON_CreateObject();
    //TODO cJSON_AddNumberToObject(res, "result", IOTGpio::setPinState(pin, state));
    const char *resp_body = cJSON_Print(res);
    httpd_resp_sendstr(req, resp_body);
    free((void *)resp_body);
    cJSON_Delete(json_body);
    cJSON_Delete(res);
    return ESP_OK;
}

esp_err_t gpio_control_level_post_handler(httpd_req_t *req, char *buf)
{
    cJSON *json_body = cJSON_Parse(buf);
    int pin = cJSON_GetObjectItem(json_body, "pin")->valueint;
    int level = cJSON_GetObjectItem(json_body, "level")->valueint;
    cJSON *res = cJSON_CreateObject();
    //TODO cJSON_AddNumberToObject(res, "result", IOTGpio::setPinState(pin, level));
    const char *resp_body = cJSON_Print(res);
    httpd_resp_sendstr(req, resp_body);
    free((void *)resp_body);
    cJSON_Delete(json_body);
    cJSON_Delete(res);
    return ESP_OK;
}