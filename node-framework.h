#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_task.h"
#include "iot_helper.h"

#define NODE_FRAMEWORK_BUILD_VERSION __DATE__ "-" __TIME__

#define WIFI_STARTED_BIT 	BIT0
#define WIFI_CONNECTED_BIT  BIT1
#define WIFI_IP_CHANGED_BIT  BIT2
#define MQTT_CONNECTED_BIT  BIT3
#define UPDATING_FIRMWARE_BIT BIT4

#define MQTT_TASK_CORE 0

void iot_logging(void);
void iot_wifi_loop(); 
void iot_init(const char * base_name);

void iot_handle_ota();
void iot_handle_ota_failed();

bool iot_get_nvs_bool(const char * variable, bool * value);
bool iot_get_nvs_uint32(const char * variable, uint32_t * value);
bool iot_get_nvs_float(const char * variable, float * value);

bool iot_is_connected();

void iot_factory_reset();

void iot_mqtt_init();
void mqtt_task_wdt(bool enable);

const char * mqtt_get_name();
void mqtt_publish_error(const char * text);
void mqtt_publish( const char * device_type, const char * subtopic, const char * data);
void mqtt_publish_ext( const char * device_type, const char * subtopic, const char * data, bool retain);
void mqtt_publish_int( const char * device_type, const char * subtopic, int data);
void mqtt_publish_int_ext( const char * device_type, const char * subtopic, int data, bool retain);
void mqtt_publish_on_off( const char * device_type, const char * subtopic, int data);


/*
 Example Templates for configuration topic to be used with Home Assistant's MQTT discovery.
 %s are replace with node name

#define SENSOR_TEMPERATURE_CFG "{\
    \"name\": \"%s temperature\", \
    \"state_topic\": \"/home/sensor/%s/temperature\" \
    }"

#define SWITCH_POWER_CFG "{\
    \"name\": \"%s hood power\", \
    \"state_topic\": \"/home/switch/%s/state\", \
    \"command_topic\": \"/home/switch/%s/power\" \
    }"

	// configuration topic will be "/home/sensor/[node_name]/temperature/config"
    mqtt_publish_ha_cfg("sensor", "temperature/config", SENSOR_TEMPERATURE_CFG, 2);

	// configuration topic will be "/home/switch/[node_name]/power_switch/config"
    mqtt_publish_ha_cfg("switch", "power_switch/config", SWITCH_POWER_CFG, 3);
*/
void mqtt_publish_ha_cfg( const char * device_type, const char * subtopic, const char * cfg_template, int name_count);

typedef enum iot_error_code_t {
    IOT_NO_ERROR = 0,
    IOT_OTA_ERROR = 1
} iot_error_code_t;

typedef enum iot_conn_status_t {
    IOT_WIFI_DISCONNECTED = 0,
    IOT_WIFI_CONNECTING = 1,
    IOT_WIFI_CONNECTED = 2,
    IOT_MQTT_CONNECTING = 3,
    IOT_MQTT_CONNECTED = 4,
    IOT_MQTT_DISCONNECTED = 5,
} iot_conn_status_t;

/**
 * Each of these codes can trigger a callback function and each callback function is stored
 * in a function pointer array for convenience. Because of this behavior, it is extremely important
 * to maintain a strict sequence and the top level special element 'MESSAGE_CODE_COUNT'
 *
 * @see iot_set_callback
 */
typedef enum iot_callback_code_t {
    IOT_NONE = 0,
    IOT_HANDLE_MQTT_MSG = 1,
    IOT_HANDLE_SET_VARIABLE = 2,
    IOT_HANDLE_NAME_CHANGE = 3,
    IOT_HANDLE_OTA = 4,
    IOT_HANDLE_ERROR = 5,
    IOT_HANDLE_CONN_STATUS = 6,
    IOT_HANDLE_FACTORY_RESET = 7,
    IOT_CB_CODE_COUNT = 8 /* important for the callback array */
} iot_cb_code_t;

void iot_set_callback(iot_cb_code_t callback_code, int (*func_ptr)(void*) );

/**
 * @brief Structure used to pass parsed MQTT msg to node
 */
typedef struct{
    const char * device_type;
    const char * subtopic;
    const char * arg;
    const char * data;
} iot_mqtt_msg_t;

/**
 * @brief Structure used to pass variable to node
 */
typedef struct{
    const char * name;
    const char * data;
} iot_variable_t;
