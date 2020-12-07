#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include "cJSON.h"


class IOTConfig {
    private:
        IOTConfig(const IOTConfig&);
        IOTConfig& operator =(const IOTConfig&);
    public:
        IOTConfig();
        static cJSON* readConfig();
        static cJSON* createModbusConfig(bool e, int s);
        static cJSON* createDefaultConfig();
        static void writeModbusConfig(bool enable, int speed);
        static void writeGpioConfig();
        static void write_mqtt_config(cJSON *gpio);
        static void writeConfig(cJSON *config);
        

    ~IOTConfig(void);
};

#endif /* CONFIG_H */


