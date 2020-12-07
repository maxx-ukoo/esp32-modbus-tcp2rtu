idf_component_register(SRCS "iot_module_main.cpp"
                            "ui/ui_server.cpp"
                            "ui/ui_server_handler.cpp"
                            "ui/ui_stepper_handler.cpp"
                            "ui/ota_handler.cpp"
                            "config/config.cpp"
                            "modbus/modbus.cpp"
                            "gpio/gpio.cpp"
                            "mqtt/mqtt.cpp"
                            "time_utils.cpp"
                            "curtains/curtains.cpp"
                        INCLUDE_DIRS ".")

if(CONFIG_EXAMPLE_WEB_DEPLOY_SF)
    set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../front")
    if(EXISTS ${WEB_SRC_DIR}/dist)
        spiffs_create_partition_image(www ${WEB_SRC_DIR}/dist FLASH_IN_PROJECT)
    else()
        message(FATAL_ERROR "${WEB_SRC_DIR}/dist doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
    endif()
endif()