#include "ui_server_handler.h"
#include "cJSON.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "config/config.h"
#include "gpio/gpio.h"
#include "mqtt/mqtt.h"

static const char *TAG = "REST Handler";

esp_err_t modbus_control_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    int enable = cJSON_GetObjectItem(root, "enable")->valueint;
    int speed = cJSON_GetObjectItem(root, "speed")->valueint;

    ESP_LOGI(TAG, "Modbus control: enable = %d, speed = %d", enable, speed);
    writeModbusConfig(enable, speed);

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t mqtt_control_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    esp_err_t status = ESP_FAIL;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *mqtt = cJSON_Parse(buf);
    if (mqtt == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        status = ESP_FAIL;
        goto end;
    }
    ESP_LOGD(TAG, "JSON received OK");
    cJSON *config = cJSON_GetObjectItem(mqtt, "mqtt");
    char *string = cJSON_Print(config);
    ESP_LOGD(TAG, "Got config: %s", string);
    status = mqtt_init_from_json(config);
    ESP_LOGD(TAG, "JSON processed OK with status %d", status);
    if (status == ESP_OK) {
        ESP_LOGD(TAG, "Writing mqtt config");
        write_mqtt_config(config);
    }
    ESP_LOGD(TAG, "exiting,status %d", status);

end:
   // cJSON_Delete(gpio);
    if (status == ESP_OK) {
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    } else {
        char resp[35];
        sprintf(resp, "MQTT configuring error: %d", status);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        return ESP_FAIL;
    }
}

esp_err_t gpio_control_post_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    int status = -1;
    
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *gpio = cJSON_Parse(buf);
    if (gpio == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        status = 0;
        goto end;
    }
    ESP_LOGD(TAG, "JSON received OK");
    cJSON *pins = cJSON_GetObjectItem(gpio, "config");
    char *string = cJSON_Print(pins);
    ESP_LOGD(TAG, "Got pins: %s", string);
    status = gpioInitFromJson(pins);
    ESP_LOGD(TAG, "JSON processed OK with status %d", status);
    if (status == -1) {
        ESP_LOGD(TAG, "Writing gpio config");
        writeGpioConfig(pins);
    }
    ESP_LOGD(TAG, "exiting,status %d", status);

end:
   // cJSON_Delete(gpio);
    if (status == -1) {
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    } else {
        char resp[15];
        sprintf(resp, "Pin error %d", status);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        return ESP_FAIL;
    }
}

esp_err_t gpio_control_state_get_handler(httpd_req_t *req) {
    char*  buf;
    size_t buf_len;
    char param[32];

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);           
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "pin", param, sizeof(param)) == ESP_OK) {
                ESP_LOGD(TAG, "Found URL query parameter => query1=%s", param);
            }
        }
        free(buf);
    }
    int pin = atoi(param);
    ESP_LOGD(TAG, "Get gpio state: pin = %d", pin);
    cJSON *res =  getPinState(pin);
    const char *resp_body = cJSON_Print(res);
    httpd_resp_sendstr(req, resp_body);
    free((void *)resp_body);
    cJSON_Delete(res);
    return ESP_OK;
}

esp_err_t gpio_control_state_post_handler(httpd_req_t *req) {
    int total_len = req->content_len;
    int cur_len = 0;
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *json_body = cJSON_Parse(buf);
    int pin = cJSON_GetObjectItem(json_body, "pin")->valueint;
    int state = cJSON_GetObjectItem(json_body, "state")->valueint;
    ESP_LOGI(TAG, "Update gpio state: pin = %d, state = %d", pin, state);
    cJSON *res =  setPinState(pin, state);
    const char *resp_body = cJSON_Print(res);
    httpd_resp_sendstr(req, resp_body);
    free((void *)resp_body);
    cJSON_Delete(json_body);
    cJSON_Delete(res);
    return ESP_OK;
}