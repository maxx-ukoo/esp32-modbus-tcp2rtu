/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "ui_server.h"
#include "ui_server_handler.h"
#include <string.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"

static const char *REST_TAG = "IOT DIN Server";

#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js.gz")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css.gz")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    ESP_LOGE(REST_TAG, "URI : %s", req->uri);
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }

    bool gzip = false;
    if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        gzip = true;
        strcat(filepath, ".gz");
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        gzip = true;
        strcat(filepath, ".gz");
    }

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);
    if (gzip) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Simple handler for getting system handler */

static esp_err_t system_info_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddNumberToObject(root, "model", chip_info.model);
    cJSON_AddNumberToObject(root, "cores", chip_info.cores);
    esp_app_desc_t *app_desc = esp_ota_get_app_description();
    cJSON_AddStringToObject(root, "date", app_desc->date);
    cJSON_AddStringToObject(root, "version", app_desc->version);
    cJSON_AddStringToObject(root, "time", app_desc->time);
    cJSON_AddStringToObject(root, "idf_ver", app_desc->idf_ver);
    cJSON_AddStringToObject(root, "uptime", esp_log_system_timestamp());
    cJSON_AddNumberToObject(root, "memory", esp_get_free_heap_size());
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t system_reboot_post_handler(httpd_req_t *req)
{
    esp_restart();
    return ESP_OK;
}

httpd_handle_t ui_http_webserver_start(const char *base_path)
{
    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));
    
    httpd_handle_t server = NULL;

    // Start the httpd server
    ESP_LOGI(REST_TAG, "Starting server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.uri_match_fn = httpd_uri_match_wildcard;

    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[]   asm("_binary_cacert_pem_end");
    conf.cacert_pem = cacert_pem_start;
    conf.cacert_len = cacert_pem_end - cacert_pem_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        free(rest_context);
        ESP_LOGI(REST_TAG, "Error starting server!");
        return NULL;
    }

    // Set URI handlers
    //ESP_LOGI(REST_TAG, "Registering URI handlers");
    //httpd_register_uri_handler(server, &root);
        /* URI handler for getting web server files */

    /* URI handler for ModBus settings */
    httpd_uri_t modbus_control_post_uri = {
        .uri = "/api/modbus",
        .method = HTTP_POST,
        .handler = modbus_control_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &modbus_control_post_uri);

    /* URI handler for MQTT settings */
    httpd_uri_t mqtt_control_post_uri = {
        .uri = "/api/mqtt",
        .method = HTTP_POST,
        .handler = mqtt_control_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &mqtt_control_post_uri);

    /* URI handler for GPIO settings */
    httpd_uri_t gpio_control_post_uri = {
        .uri = "/api/gpio",
        .method = HTTP_POST,
        .handler = gpio_control_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &gpio_control_post_uri);

    /* URI handler for control pin state */
    httpd_uri_t gpio_control_state_post_uri = {
        .uri = "/api/gpio/state",
        .method = HTTP_POST,
        .handler = gpio_control_state_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &gpio_control_state_post_uri);

    /* URI handler for control pin state */
    httpd_uri_t gpio_control_state_get_uri = {
        .uri = "/api/gpio/state",
        .method = HTTP_GET,
        .handler = gpio_control_state_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &gpio_control_state_get_uri);

   /* URI handler for fetching system info */
    httpd_uri_t system_info_get_uri = {
        .uri = "/api/v1/system/info",
        .method = HTTP_GET,
        .handler = system_info_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_info_get_uri);

   /* URI handler for reboot system */
    httpd_uri_t system_reset_post_uri = {
        .uri = "/api/v1/system/reboot",
        .method = HTTP_POST,
        .handler = system_reboot_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &system_reset_post_uri);

    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &common_get_uri);
    return server;
err:
    return NULL;
}