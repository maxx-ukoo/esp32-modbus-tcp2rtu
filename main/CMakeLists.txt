idf_component_register(SRCS "iot_module_main.c"
                            "ui/ui_server.c"
                            "ui/ui_server_handler.c"
                            "ui/ota_handler.c"
                            "config/config.c"
                            "modbus/modbus.c"
                            "gpio/gpio.c"
                            "mqtt/mqtt.c"
                            "time_utils.c"
                        INCLUDE_DIRS ".")

if(CONFIG_EXAMPLE_WEB_DEPLOY_SF)
    set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../front")
    if(EXISTS ${WEB_SRC_DIR}/dist)
        spiffs_create_partition_image(www ${WEB_SRC_DIR}/dist FLASH_IN_PROJECT)
    else()
        message(FATAL_ERROR "${WEB_SRC_DIR}/dist doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
    endif()
endif()