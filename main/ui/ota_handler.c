#include "ota_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>
#include <sys/param.h>
#include <esp_http_server.h>
#include "esp_ota_ops.h"
#include "freertos/event_groups.h"
#include "cJSON.h"
#include "esp_vfs.h"
//#include <spiffs/spiffs.h>
//#include "spiffs/spiffs_nucleus.h"


#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

static const char *REST_TAG = "IOT DIN Server";

int8_t flash_status = 0;

EventGroupHandle_t reboot_event_group;
const int REBOOT_BIT = BIT0;

static char * parse_uri(const char * uri) {
   char * token = strtok(uri, "/");
   char * latest = token;
   while( token != NULL ) {
      latest = token;
      token = strtok(NULL, "/");
   }
   return latest;
}

esp_err_t ota_post_handler(httpd_req_t *req) {
	ESP_LOGD(REST_TAG, "API OTA URI : %s", req->uri);
    char * api_component_name = parse_uri(req->uri);
	if (strcmp(api_component_name, "firmware") == 0) 
    {
        return ota_upload_firmware_handler(req);
    }
	else if (strcmp(api_component_name, "file") == 0)
    {
        return upload_file_handler(req);
    }
	else if (strcmp(api_component_name, "reboot") == 0)
    {
        return ota_status_handler(req);
    }
    char resp[40];
    sprintf(resp, "Unknown path: %.20s", api_component_name);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
    return ESP_FAIL;  
}

esp_err_t ota_get_handler(httpd_req_t *req) {
	ESP_LOGD(REST_TAG, "API OTA URI : %s", req->uri);
    char * api_component_name = parse_uri(req->uri);
	if (strcmp(api_component_name, "list") == 0) 
    {
        return ota_list_files_handler(req);
    }
	else if (strcmp(api_component_name, "reboot") == 0)
    {
        return ESP_OK;
    }
	char resp[40];
	sprintf(resp, "Unknown path: %.20s", api_component_name);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
    return ESP_FAIL;  
}

static esp_err_t ota_list_files_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "application/json");
	DIR *dir = NULL;
    // Open directory
    dir = opendir(CONFIG_EXAMPLE_WEB_MOUNT_POINT);
    if (!dir) {
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error opening directory");
        return ESP_FAIL;
    }
    cJSON *file = NULL;
    cJSON *files = cJSON_CreateArray();
    if (files == NULL)
    {
        goto end;
    }

    struct dirent *ent;
	struct stat entry_stat;
	char entrypath[255];
    while ((ent = readdir(dir)) != NULL) {
		file = cJSON_CreateObject();
        if (file == NULL)
        {
            goto end;
        }
		cJSON_AddItemToArray(files, file);
		cJSON_AddStringToObject(file, "name", ent->d_name);

		sprintf(entrypath, CONFIG_EXAMPLE_WEB_MOUNT_POINT);
        if (CONFIG_EXAMPLE_WEB_MOUNT_POINT[strlen(CONFIG_EXAMPLE_WEB_MOUNT_POINT)-1] != '/') strcat(entrypath,"/");
        strcat(entrypath,ent->d_name);
		if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(REST_TAG, "Failed to stat %s", ent->d_name);
            continue;
        }
		cJSON_AddNumberToObject(file, "size", entry_stat.st_size);
    }

    const char *files_list = cJSON_Print(files);
    httpd_resp_sendstr(req, files_list);
    free((void *)files_list);
	return ESP_OK;
end:
    ESP_LOGE(REST_TAG, "Error on creating files list");
    cJSON_Delete(files);
    return ESP_FAIL;
}

/* Status */
static esp_err_t ota_status_handler(httpd_req_t *req)
{
	char ledJSON[100];
	
	ESP_LOGI("OTA", "Status Requested");
	
	sprintf(ledJSON, "{\"status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", flash_status, __TIME__, __DATE__);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, ledJSON, strlen(ledJSON));
	
	// This gets set when upload is complete
	if (flash_status == 1)
	{
		// We cannot directly call reboot here because we need the 
		// browser to get the ack back. 
		xEventGroupSetBits(reboot_event_group, REBOOT_BIT);		
	}

	return ESP_OK;
}
/* Receive .Bin file */
static esp_err_t ota_upload_firmware_handler(httpd_req_t *req)
{
	ESP_LOGI("OTA", "ota_update_post_handler started");
	esp_ota_handle_t ota_handle; 
	
	char ota_buff[1024];
	int content_length = req->content_len;
	int content_received = 0;
	int recv_len;
	bool is_req_body_started = false;
	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

	// Unsucessful Flashing
	flash_status = -1;
	
	do
	{
		/* Read the data for the request */
		if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0) 
		{
			if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) 
			{
				ESP_LOGI("OTA", "Socket Timeout");
				/* Retry receiving if timeout occurred */
				continue;
			}
			ESP_LOGI("OTA", "OTA Other Error %d", recv_len);
			return ESP_FAIL;
		}

		printf("OTA RX: %d of %d\r", content_received, content_length);
		
	    // Is this the first data we are receiving
		// If so, it will have the information in the header we need. 
		if (!is_req_body_started)
		{
			is_req_body_started = true;
			
			// Lets find out where the actual data staers after the header info		
			char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;	
			int body_part_len = recv_len - (body_start_p - ota_buff);
			
			//int body_part_sta = recv_len - body_part_len;
			//printf("OTA File Size: %d : Start Location:%d - End Location:%d\r\n", content_length, body_part_sta, body_part_len);
			printf("OTA File Size: %d\r\n", content_length);

			esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			if (err != ESP_OK)
			{
				printf("Error With OTA Begin, Cancelling OTA\r\n");
				return ESP_FAIL;
			}
			else
			{
				printf("Writing to partition subtype %d at offset 0x%x\r\n", update_partition->subtype, update_partition->address);
			}

			// Lets write this first part of data out
			esp_ota_write(ota_handle, body_start_p, body_part_len);
		}
		else
		{
			// Write OTA data
			esp_ota_write(ota_handle, ota_buff, recv_len);
			
			content_received += recv_len;
		}
 
	} while (recv_len > 0 && content_received < content_length);

	// End response
	//httpd_resp_send_chunk(req, NULL, 0);

	
	if (esp_ota_end(ota_handle) == ESP_OK)
	{
		// Lets update the partition
		if(esp_ota_set_boot_partition(update_partition) == ESP_OK) 
		{
			const esp_partition_t *boot_partition = esp_ota_get_boot_partition();

			// Webpage will request status when complete 
			// This is to let it know it was successful
			flash_status = 1;
		
			ESP_LOGI("OTA", "Next boot partition subtype %d at offset 0x%x", boot_partition->subtype, boot_partition->address);
			ESP_LOGI("OTA", "Please Restart System...");
		}
		else
		{
			ESP_LOGI("OTA", "\r\n\r\n !!! Flashed Error !!!");
		}
		
	}
	else
	{
		ESP_LOGI("OTA", "\r\n\r\n !!! OTA End Error !!!");
	}
	httpd_resp_sendstr(req, "OK");
	return ESP_OK;

}

static int calculate_data_lenght(char *data, int data_len, char *pattern, int pattern_len) {
    int cursor = 0;
	static int maches = 0;
	static int search_position = 0;

    while (cursor < data_len && maches != pattern_len) {	    
		//ESP_LOGD(REST_TAG, "Searching data in buffer cursor: %d, maches: %d, search_position: %d, %c == %c", cursor, maches, search_position, pattern[search_position], data[cursor]);
		if (pattern[search_position] == data[cursor]) {
			maches++;
			search_position++;
		} else {
			if (maches > 1) {
		        cursor = cursor - maches;
		    }
			maches = 0;
			search_position = 0;;
		}
		cursor++;
	}
	return cursor - maches;
}

/* Handler to upload a file onto the server */
static esp_err_t upload_file_handler(httpd_req_t *req)
{
	char filename[FILE_PATH_MAX];
    //char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;
	char tmp_filename[FILE_PATH_MAX];
	snprintf(tmp_filename, sizeof tmp_filename, "%s/%s", CONFIG_EXAMPLE_WEB_MOUNT_POINT, "upload.tmp");


    if (stat(tmp_filename, &file_stat) == 0) {
        // Delete it if it exists
        unlink(tmp_filename);
    }

	fd = fopen(tmp_filename, "w");
    if (!fd) {
        ESP_LOGE(REST_TAG, "Failed to create file : %s", tmp_filename);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

	//TODO add checking available space
	/*    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }*/
    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(REST_TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

	
    size_t boundary_len;
	char *boundary_start_p = NULL;
	char boundary_buff[128];
    boundary_len = httpd_req_get_hdr_value_len(req, "Content-Type") + 1;
    if (boundary_len > 1) {
        
        if (httpd_req_get_hdr_value_str(req, "Content-Type", boundary_buff, boundary_len) == ESP_OK) {
            ESP_LOGI(REST_TAG, "Found header => Host: %s", boundary_buff);
        }
		boundary_start_p = strstr(boundary_buff, "boundary=") + 9;	
		if (boundary_start_p) {
            snprintf(boundary_buff, sizeof boundary_buff, "\r\n--%s", boundary_start_p);
			boundary_start_p = &boundary_buff;
			boundary_len = strlen(boundary_buff);
			ESP_LOGI(REST_TAG, "Boundary : %s, len = %d", boundary_start_p, boundary_len);	
		} else {
			httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,"Can't find data separator");
			return ESP_FAIL;
		}
    }

    /* Retrieve the pointer to scratch buffer for temporary storage */
    //char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    //int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    //int remaining = req->content_len;

	char ota_buff[1024];
	char * body_start_p;
	int content_length = req->content_len;
	int content_received = 0;
	int recv_len;
	int data_len;
	bool is_req_body_started = false;
	//int search_position = 0;
	//int maches = 0;
	//int cursor = 0;
	int tmp_size = 0;
    char tmp_buf[50];

	// reset boundary state
	calculate_data_lenght("123456", 6, "tt", 2);
	do
		{
			/* Read the data for the request */
			if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0) 
			{
				if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) 
				{
					ESP_LOGI("OTA", "Socket Timeout");
					/* Retry receiving if timeout occurred */
					continue;
				}
				/* In case of unrecoverable error,
				* close and delete the unfinished file*/
				fclose(fd);
				unlink(tmp_filename);
				ESP_LOGE("OTA", "OTA Other Error %d", recv_len);
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
				return ESP_FAIL;
			}

			ESP_LOGD(REST_TAG, "Received %d  bytes", recv_len);
			ESP_LOGD(REST_TAG, "Buffer: %s", ota_buff);

			//printf("OTA RX: %d of %d\r", content_received, content_length);
			
			// Is this the first data we are receiving
			// If so, it will have the information in the header we need. 
			body_start_p = ota_buff;
			data_len = recv_len;

			if (!is_req_body_started)
			{
				is_req_body_started = true;
				char *filename_start_p = strstr(ota_buff, "filename=\"") + 10;	
				char *filename_end_p = strchr(filename_start_p, '"');
				char tmp[20];
				strncpy(tmp, filename_start_p, filename_end_p-filename_start_p);
				tmp[filename_end_p-filename_start_p] = 0;
				ESP_LOGI(REST_TAG, "Receiving file : %s...", tmp);
				snprintf(filename, sizeof filename, "%s/%s", CONFIG_EXAMPLE_WEB_MOUNT_POINT, tmp);
				ESP_LOGI(REST_TAG, "Local file : %s...", filename);			
				
				// Lets find out where the actual data staers after the header info		
				body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;	
				data_len = recv_len - (body_start_p - ota_buff);
				ESP_LOGI(REST_TAG, "File size: %d\r\n", content_length);
				if (data_len == 0) {
					continue;
				}
			}
			ESP_LOGD(REST_TAG, "Data_len: %d", data_len);
   
			int file_len = calculate_data_lenght(body_start_p, data_len, boundary_start_p, boundary_len);
			printf("Result: %d, tmp_size=%d\n", file_len, tmp_size);
			if (file_len == data_len && tmp_size > 0) {
				ESP_LOGD(REST_TAG, "Save %d bytes from prev section", tmp_size);
				/* Write buffer content to file on storage */
				if (tmp_size && (tmp_size != fwrite(tmp_buf, 1, tmp_size, fd))) {
					/* Couldn't write everything to file!
					* Storage may be full? */
					fclose(fd);
					unlink(tmp_filename);

					ESP_LOGE(REST_TAG, "File write failed!");
					/* Respond with 500 Internal Server Error */
					httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
					return ESP_FAIL;
				}
				tmp_size = 0;
			} else if (file_len > 0) {
				ESP_LOGD(REST_TAG, "Save %d bytes to tmp storage", data_len-file_len);
				for (int n = file_len; n< data_len; n++) {
					tmp_buf[tmp_size] = body_start_p[n];
					tmp_size++;
				}
			} else {
				ESP_LOGD(REST_TAG, "File received no extra data nin buffer, break");
				ESP_LOGD(REST_TAG, "Save %d bytes from prev section", tmp_size);
				/* Write buffer content to file on storage */
				if (tmp_size && (tmp_size != fwrite(tmp_buf, 1, tmp_size, fd))) {
					/* Couldn't write everything to file!
					* Storage may be full? */
					fclose(fd);
					unlink(tmp_filename);

					ESP_LOGE(REST_TAG, "File write failed!");
					/* Respond with 500 Internal Server Error */
					httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
					return ESP_FAIL;
				}
				break;
			}
			
			ESP_LOGD("OTA FILE", "Writing %d bytes", file_len);

			/* Write buffer content to file on storage */
			if (file_len && (file_len != fwrite(body_start_p, 1, file_len, fd))) {
				/* Couldn't write everything to file!
				* Storage may be full? */
				fclose(fd);
				unlink(tmp_filename);

				ESP_LOGE(REST_TAG, "File write failed!");
				/* Respond with 500 Internal Server Error */
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
				return ESP_FAIL;
			}
				
			content_received += file_len;
			
	
		} while (recv_len > 0 && content_received < content_length);

    fclose(fd);
	if (stat(filename, &file_stat) == 0) {
        // Delete it if it exists
        unlink(filename);
    }

	//if (stat(tmp_filename, &file_stat) == 0) {
	//	ESP_LOGD(REST_TAG, "File before truncate: %ld", file_stat.st_size);
    //}

	// Rename original file
    ESP_LOGI(REST_TAG, "Renaming file");
    if (rename(tmp_filename, filename) != 0) {
      ESP_LOGE(REST_TAG, "Rename failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to rename received file");
		return ESP_FAIL;
    }


  	//if (stat(tmp_filename, &file_stat) == 0) {
	//	ESP_LOGD(REST_TAG, "File before truncate: %ld", file_stat.st_size);
    //}
	
    //ESP_LOGI(REST_TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

void ota_system_reboot_task(void * parameter)
{

	// Init the event group
	reboot_event_group = xEventGroupCreate();
	
	// Clear the bit
	xEventGroupClearBits(reboot_event_group, REBOOT_BIT);

	
	for (;;)
	{
		// Wait here until the bit gets set for reboot
		EventBits_t staBits = xEventGroupWaitBits(reboot_event_group, REBOOT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
		
		// Did portMAX_DELAY ever timeout, not sure so lets just check to be sure
		if ((staBits & REBOOT_BIT) != 0)
		{
			ESP_LOGI("OTA", "Reboot Command, Restarting");
			vTaskDelay(2000 / portTICK_PERIOD_MS);

			esp_restart();
		}
	}
}
