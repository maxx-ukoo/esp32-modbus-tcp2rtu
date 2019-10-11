#define PORT 502
#define FLOW_CONTROL_QUEUE_TIMEOUT_MS (100)
#define FLOW_CONTROL_QUEUE_LENGTH (5)

//
// MODBUS MBAP offsets
//
#define MB_TCP_TID 0
#define MB_TCP_PID 2
#define MB_TCP_LEN 4
#define MB_TCP_UID 6
#define MB_TCP_FUNC 7
#define MB_TCP_REGISTER_START 8
#define MB_TCP_REGISTER_NUMBER 10

// Tested functions
#define MB_FUNC_READ_INPUT_REGISTER 4

//#define MB_FC_NONE 0
//#define MB_FC_READ_REGISTERS 3 //implemented
//#define MB_FC_WRITE_REGISTER 6 //implemented
//#define MB_FC_WRITE_MULTIPLE_REGISTERS 16 //implemented
//
// MODBUS Error Codes
//
#define MB_EC_NONE 0
#define MB_EC_ILLEGAL_FUNCTION 1
#define MB_EC_ILLEGAL_DATA_ADDRESS 2
#define MB_EC_ILLEGAL_DATA_VALUE 3
#define MB_EC_SLAVE_DEVICE_FAILURE 4

#define SENSE_MB_CHECK(a, ret_val, str, ...) \
    if (!(a)) { \
        ESP_LOGE(TAG, "%s(%u): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        return (ret_val); \
    }

#define MB_BAUDRATE 9600
#define MB_PORTNUM 2
#define MB_PARITY UART_PARITY_DISABLE

#define CONFIG_MB_UART_RXD 18
#define CONFIG_MB_UART_TXD 13
#define CONFIG_MB_UART_RTS 4

typedef struct {
    char* message;
    uint16_t length;
} flow_control_msg_t;
