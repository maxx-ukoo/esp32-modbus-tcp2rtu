
#ifndef UI_STEPPER_HANDLER_H
#define UI_STEPPER_HANDLER_H

#include <esp_https_server.h>
//#include "esp_system.h"
//#include "esp_vfs.h"

esp_err_t stepper_calibrate_handler(httpd_req_t *req);
esp_err_t stepper_steps_handler(httpd_req_t *req);
esp_err_t stepper_move_handler(httpd_req_t *req);

#endif /* UI_STEPPER_HANDLER_H */


