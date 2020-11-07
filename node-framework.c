#define CONFIG_MQTT_LOG_INFO_ON 
#define CONFIG_MQTT_LOG_WARN_ON 


#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"


#include "node-framework.h"
#include "mqtt_client.h"
#include "freertos/event_groups.h"

#include "wifi_manager.h"
#include "mqtt_manager.h"

#include "sdkconfig.h"
#ifdef CONFIG_IDF_TARGET_ESP32
#define ESP32
#endif


#define TWDT_TIMEOUT_S  10


static const char *TAG = "NODE-FRAMEWORK";

// Event group
EventGroupHandle_t conn_event_group = NULL;

// NVS handle
nvs_handle storage_handle;

/* @brief Array of callback function pointers */
static int (**cb_ptr_arr)(void*) = NULL;


#define MAX_NODE_NAME_LEN 32

static char base_name_with_mac[MAX_NODE_NAME_LEN];
static char node_name[MAX_NODE_NAME_LEN];
static char * mqtt_name;

void mqtt_publish_error(const char * text) {
    char topic[64];
    if (iot_is_connected()) {
        snprintf(topic,64, "/home/node/%s/error", mqtt_name);
        mqtt_manager_publish( topic, text, 0, 0, 0);        
    }
}


void mqtt_publish_ext( const char * device_type, const char * subtopic, const char * data, bool retain) {
    char topic[64];
    if (iot_is_connected()) {
        snprintf(topic, 64, "/home/%s/%s/%s", device_type, mqtt_name, subtopic);
        ESP_LOGI(TAG, "Publish: %s : %s", topic, data);
        mqtt_manager_publish(topic, data, 0, 0, retain);
    }
}

void mqtt_publish( const char * device_type, const char * subtopic, const char * data) {
    mqtt_publish_ext(device_type, subtopic, data, false);
}

void mqtt_publish_int_ext( const char * device_type, const char * subtopic, int data, bool retain) {
    char topic[64];
    char data2[16];
    if (iot_is_connected()) {
        snprintf(topic, 64, "/home/%s/%s/%s", device_type, mqtt_name, subtopic);
        sprintf(data2,"%d",data);
        ESP_LOGI(TAG, "Publish: %s : %s", topic, data2);
        mqtt_manager_publish(topic, data2, 0, 0, retain);
    }
}

void mqtt_publish_int( const char * device_type, const char * subtopic, int data) {
    mqtt_publish_int_ext(device_type, subtopic, data, false);
}

void mqtt_publish_on_off( const char * device_type, const char * subtopic, int data) {
    if (data) {
        mqtt_publish(device_type, subtopic, "ON");
    } else {
        mqtt_publish(device_type, subtopic, "OFF");        
    }
}

void mqtt_publish_ha_cfg( const char * device_type, const char * subtopic, const char * cfg_template, int name_count) {
	size_t len = strlen(cfg_template) + name_count*strlen(mqtt_get_name()) + 1;
	char * tmp = malloc(len);
	if (tmp) {
	    if (name_count==1)
	        snprintf(tmp, len, cfg_template, mqtt_get_name(), mqtt_get_name() );
	    else if (name_count==2)
	        snprintf(tmp, len, cfg_template, mqtt_get_name(), mqtt_get_name() );
	    else if (name_count==3)
	        snprintf(tmp, len, cfg_template, mqtt_get_name(), mqtt_get_name(), mqtt_get_name() );
	    else if (name_count==4)
	        snprintf(tmp, len, cfg_template, mqtt_get_name(), mqtt_get_name(), mqtt_get_name(), mqtt_get_name() );
	    mqtt_publish_ext(device_type, subtopic, tmp, true);
	    free(tmp);
	}
}

const char * mqtt_get_name() {
    return mqtt_name;
}


void iot_set_callback(iot_cb_code_t callback_code, int (*func_ptr)(void*) ) {
    if(cb_ptr_arr && callback_code < IOT_CB_CODE_COUNT){
        cb_ptr_arr[callback_code] = func_ptr;
    }
}


void iot_update_firmware( const char * url ) {
    char temp[128];

    // TODO
    esp_http_client_config_t config = {

        //TODO
        //.cert_pem = (char *)ca_cert_pem_start,
        
        //.event_handler = _http_event_handler,
    };

    if (strlen(url)==0) {
        size_t string_size=128;
        if (nvs_get_str(storage_handle, "update_url", temp, &string_size)==ESP_OK) {
            temp[string_size]=0;
            config.url = temp;
            ESP_LOGI(TAG, "OTA update from saved URL: '%s'", temp);
        } else {
            ESP_LOGE(TAG, "update url not given nor set!");
            mqtt_publish_error("update url not given nor set!");
            return;
        }
    } else {
        config.url = url;
        ESP_LOGI(TAG, "OTA update via MQTT URL: '%s'", url);
    }

    config.skip_cert_common_name_check = true;

    /* Ensure to disable any WiFi power save mode, this allows best throughput
     * and hence timings for overall OTA operation.
     */
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "[OTA] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[OTA] Stack: %d", uxTaskGetStackHighWaterMark(NULL));

    xEventGroupSetBits(conn_event_group, UPDATING_FIRMWARE_BIT);

    /* callback */
    if(cb_ptr_arr[ IOT_HANDLE_OTA ]) (*cb_ptr_arr[ IOT_HANDLE_OTA ])( NULL );

    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        mqtt_publish("node","ota","OTA OK, restarting...");
        vTaskDelay(1000 / portTICK_RATE_MS);
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed...\n");
        mqtt_publish_error("OTA update failed!");
        xEventGroupClearBits(conn_event_group, UPDATING_FIRMWARE_BIT);
        /* callback */
        if(cb_ptr_arr[ IOT_HANDLE_ERROR ]) (*cb_ptr_arr[ IOT_HANDLE_ERROR ])( (void*)IOT_OTA_ERROR );
    }
}


void mqtt_update_node_name(const char * name) {
    ESP_LOGI(TAG,"Updating node name to '%s'",name);
    char topic[64];
    if (strcmp(mqtt_name, base_name_with_mac) != 0) {
        snprintf(topic,64,"/home/+/%s/#",mqtt_name);
        mqtt_manager_unsubscribe(topic);
        ESP_LOGI(TAG,"-- Unsubscribe %s", topic);
    }
    snprintf(topic,64,"/home/+/%s/#",name);
    mqtt_manager_subscribe(topic);
    ESP_LOGI(TAG,"-- Subscribe %s", topic);

    char oldname[MAX_NODE_NAME_LEN];
    strncpy(oldname, node_name,MAX_NODE_NAME_LEN);
    strncpy(node_name, name, MAX_NODE_NAME_LEN);

    /* callback */
    if(cb_ptr_arr[ IOT_HANDLE_NAME_CHANGE ]) (*cb_ptr_arr[ IOT_HANDLE_NAME_CHANGE ])( node_name );

    mqtt_name = node_name;
}


void mqtt_set(const char * variable, const char * data) {
    ESP_LOGI(TAG, "SET: %s -> %s",variable,data);
    if (strcmp(variable,"name")==0) {
        if (strlen(data)==0) {
            ESP_LOGE(TAG,"SET: Name cannot be empty!");
            mqtt_publish_error("Name cannot be empty!");
            return;
        }
        if (strcmp(data,"#")==0) {
            ESP_LOGE(TAG,"SET: Name cannot be '#'!");
            mqtt_publish_error("Name cannot be '#'!");
            return;
        }
        if (strlen(data)>MAX_NODE_NAME_LEN-1) {
            ESP_LOGE(TAG,"SET: name too long! (>31)");
            mqtt_publish_error("name too long! (>31)");
            return;         
        }
        mqtt_update_node_name(data);
    } else {

        /* callback: check if node recognizes this variable */
        bool handled=false;
        if(cb_ptr_arr[ IOT_HANDLE_SET_VARIABLE ]) {
            iot_variable_t var;
            var.name = variable;
            var.data = data;
            if ((*cb_ptr_arr[ IOT_HANDLE_SET_VARIABLE ])( &var )) {
                handled=true;
            }
        }
        if (!handled) {
            ESP_LOGE(TAG,"SET: Unknown variable!");
            mqtt_publish_error("Unknown variable");
            return;
        }
    }
    if (nvs_set_str(storage_handle, variable, data)!=ESP_OK) {
        ESP_LOGE(TAG,"SET: error recording variable to NVS!");
        mqtt_publish_error("error recording variable to NVS!");
    } else
    {
        nvs_commit(storage_handle);
    }
}


void mqtt_get(const char * variable) {
    char temp[128];
    size_t string_size=128;
    if (nvs_get_str(storage_handle, variable, temp, &string_size)==ESP_OK) {
        printf("GET: %s = %s\n",variable,temp);
        mqtt_publish("node",variable,temp);
    } else {
        printf("GET: %s not found!\n",variable);
        mqtt_publish("node", variable,"NOTFOUND");
    }
}

void mqtt_handle_msg(const char * device_type, const char * subtopic, const char * arg, const char * data) {
    if (arg && (strcmp(arg,"config")==0))
        // ignore 'config' JSONs sent by us
        return;
    if (strcmp(subtopic,"config")==0)
        // ignore 'config' JSONs sent by us
        return;
    if (arg)
        printf("HANDLE MSG: %s:%s (%s) : %s\n",device_type, subtopic,arg,data);
    else
        printf("HANDLE MSG: %s:%s : %s\n",device_type, subtopic,data);

    if (strcmp(device_type,"node")==0) {
        if (strcmp(subtopic,"get")==0) {
            if (arg) {
                mqtt_get(arg);
            } else
                mqtt_publish_error("mqtt GET: No variable!");
        } else if (strcmp(subtopic,"set")==0) {
            if (arg && data) {
                mqtt_set(arg,data);
            } else
                mqtt_publish_error("mqtt SET: Insufficient args!");
        } else if (strcmp(subtopic,"update_firmware")==0) {
            iot_update_firmware(data);
        } else if (strcmp(subtopic,"start_ap")==0) {
            ESP_LOGI(TAG,"Starting AP and disabling auto shutdown..");
            wifi_manager_set_auto_ap_shutdown(false);
            wifi_manager_send_message(WM_ORDER_START_AP, NULL);
        } else if (strcmp(subtopic,"stop_ap")==0) {
            ESP_LOGI(TAG,"Stopping AP..");
            wifi_manager_send_message(WM_ORDER_STOP_AP, NULL);
        } else if (strcmp(subtopic,"restart")==0) {
            esp_restart();
        } else {
            // ignore others, maybe msgs sent by us
        }
    } else {
        iot_mqtt_msg_t msg;
        msg.device_type = device_type;
        msg.subtopic = subtopic;
        msg.arg = arg;
        msg.data = data;
        if(cb_ptr_arr[ IOT_HANDLE_MQTT_MSG ]) (*cb_ptr_arr[ IOT_HANDLE_MQTT_MSG ])( &msg );
    }
}


/* 
    Parse MQTT topic. It is formatted as follows:
    /home/{device_type}/{node_name}/{sub_topic}[/{argument}]

    Examples:  
    /home/switch/my_switch_123/set/my_custom_parameter  (data: "foobar")
    /home/light/kitchen_light/switch  (data: "ON")
*/
void mqtt_handle_data(esp_mqtt_event_handle_t event)
{
    int n;
    char temp[128];
    char data[128];
    char device_type[32];

    n = event->topic_len < 128 ? event->topic_len : 127;
    memcpy(temp, event->topic, n);
    temp[n]=0;

    n = event->data_len < 128 ? event->data_len : 127;
    memcpy(data, event->data, n);
    data[n]=0;

    char* rest = temp;
    // ignore 'home'
    char * token = strtok_r(temp,"/",&rest);
    if (!token)
        return;

    token = strtok_r(NULL, "/",&rest);
    // the device type is 'node', 'switch' etc..
    strncpy(device_type, token,32);

    // ignore node name, because we have already subscribed only for those that match our node name
    token = strtok_r(NULL, "/",&rest);
    if (!token)
        return;
    
    token = strtok_r(NULL, "/",&rest);
    if (token) {        
        char subtopic[32];
        strncpy(subtopic,token,32);
        token = strtok_r(NULL, "/",&rest);  // this third MQTT parameter (argument) is optional and can be NULL
        mqtt_handle_msg(device_type, subtopic, token, data);
    }
}


void mqtt_event_handler_cb(void * arg)
{
    esp_mqtt_event_handle_t event = arg;

    switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			ESP_LOGW(TAG," Heap: %d", esp_get_free_heap_size());
			xEventGroupSetBits(conn_event_group, MQTT_CONNECTED_BIT);

			mqtt_manager_subscribe("/home/node/all/#");

			char temp[64];
			snprintf(temp,64,"/home/+/%s/#",base_name_with_mac);
			mqtt_manager_subscribe(temp);
			if (mqtt_name != base_name_with_mac) {
				snprintf(temp,64,"/home/+/%s/#",mqtt_name);
				mqtt_manager_subscribe(temp);
			}

			mqtt_publish_ext("node", "announce", "awoke", true);
			mqtt_publish_ext("node", "framework_version", NODE_FRAMEWORK_BUILD_VERSION, true);

            if(cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ]) (*cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ])( (void*)IOT_MQTT_CONNECTED );
			break;

		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			xEventGroupClearBits(conn_event_group, MQTT_CONNECTED_BIT);
            if(cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ]) (*cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ])( (void*)IOT_MQTT_DISCONNECTED );
			break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            mqtt_handle_data(event);
            break;
        default:
        	// Other events are handled by MQTT manager
            break;
    }
    ESP_LOGD(TAG, "[EVENT] Stack: %d heap: %d", uxTaskGetStackHighWaterMark(NULL), esp_get_free_heap_size());

    }


void mqtt_connecting_cb(void * arg) {
    if(cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ]) (*cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ])( (void*)IOT_MQTT_CONNECTING );
}


bool iot_get_nvs_uint32(const char * variable, uint32_t * value) {
    char temp[32];
    size_t string_size=32;
    if (nvs_get_str(storage_handle, variable, temp, &string_size)==ESP_OK) {
        *value = atoi(temp);
        return true;
    }
    return false;
}

bool iot_get_nvs_float(const char * variable, float * value) {
    char temp[32];
    size_t string_size=32;
    if (nvs_get_str(storage_handle, variable, temp, &string_size)==ESP_OK) {
        *value = atof(temp);
        return true;
    }
    return false;
}


bool iot_get_nvs_bool(const char * variable, bool * value) {
    char temp[32];
    size_t string_size=32;
    if (nvs_get_str(storage_handle, variable, temp, &string_size)==ESP_OK) {
        if (temp[0]=='0') {
            *value = false;
            return true;
        }
        else if (temp[0]=='1') {
            *value = true;
            return true;
        }
        else
            return false;
    }
    return false;
}


void wifi_connected_cb( void * param ) {

    // defer getting MAC address to  WM_EVENT_STA_GOT_IP because earlier events might have been missed    
	if (strcmp(base_name_with_mac,"")==0) {
		// get MAC address if not yet fetched
		struct wifi_settings_t * settings = wifi_manager_get_wifi_settings();
		//strncpy(base_name_with_mac, (char*)wifi_settings.ap_ssid, MAX_NODE_NAME_LEN );
		strncpy(base_name_with_mac, (char*)settings->ap_ssid, MAX_NODE_NAME_LEN );
		size_t string_size = MAX_NODE_NAME_LEN;
		if (nvs_get_str(storage_handle, "name", node_name, &string_size)==ESP_OK) {
			ESP_LOGI(TAG, "Publishing as %s (responding also to %s)", node_name, base_name_with_mac);
			mqtt_name = node_name;
		} else {
			ESP_LOGI(TAG, "Publishing as %s",base_name_with_mac);
			mqtt_name = base_name_with_mac;
		}
	}
    if(cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ]) (*cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ])( (void*) IOT_WIFI_CONNECTED );
}


void wifi_connecting_cb( void * param ) {
    if(cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ]) (*cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ])( (void*) IOT_WIFI_CONNECTING );
}


void wifi_disconnected_cb( void * param ) {
    if(cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ]) (*cb_ptr_arr[ IOT_HANDLE_CONN_STATUS ])( (void*) IOT_WIFI_DISCONNECTED );
    ESP_LOGD(TAG, "wifi_disconnected_cb - stack: %d", uxTaskGetStackHighWaterMark(NULL));
}


void iot_factory_reset() {
    ESP_LOGW(TAG,"----FACTORY RESET!----");
    // disconnect from WiFi station, reset any saved WiFi settings and restart of the Access Point
    wifi_manager_send_message(WM_ORDER_DISCONNECT_STA, NULL);
    wifi_manager_erase_config();
    wifi_manager_send_message(WM_ORDER_START_AP, NULL);

    // reset MQTT config. If connected to MQTT server, it is automatically disconnected when WiFi disconnects
    mqtt_manager_set_uri("");
    mqtt_manager_set_username("");
    mqtt_manager_set_password("");
    mqtt_manager_save_config();

    if(cb_ptr_arr[ IOT_HANDLE_FACTORY_RESET ]) (*cb_ptr_arr[ IOT_HANDLE_FACTORY_RESET ])(NULL);
}


bool iot_is_connected() {
    if (conn_event_group == NULL)
        return false;
    return (xEventGroupGetBits(conn_event_group) & MQTT_CONNECTED_BIT);
}


void iot_init(const char * base_name) {

    // initialize task watchdog
#ifdef ESP32
    esp_task_wdt_init(TWDT_TIMEOUT_S, true);
#else
    esp_task_wdt_init();
#endif

    cb_ptr_arr = malloc(sizeof(void (*)(void*)) * IOT_CB_CODE_COUNT);
    for(int i=0; i<IOT_CB_CODE_COUNT; i++){
        cb_ptr_arr[i] = NULL;
    }

    conn_event_group = xEventGroupCreate();

    ESP_LOGI(TAG,"Running on core #%d", xPortGetCoreID());

    // Init and open NVS partition in RW mode
    esp_err_t ret = nvs_flash_init();
#ifdef ESP32
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
#else
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
#endif
    	ESP_ERROR_CHECK(nvs_flash_erase());
    	ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_open("storage", NVS_READWRITE, &storage_handle);
    if (ret != ESP_OK) {        
        ESP_LOGE(TAG,"FATAL ERROR: Unable to open NVS");
        while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG,"NVS open OK");


    iot_led_init();

    base_name_with_mac[0] = 0;

    wifi_manager_start(base_name, true);
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, wifi_connected_cb);
    wifi_manager_set_callback(WM_ORDER_CONNECT_STA, wifi_connecting_cb);
    wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, wifi_disconnected_cb);
    mqtt_manager_start();
    mqtt_manager_set_callback( MM_EVENT_MQTT_EVENT, mqtt_event_handler_cb);
    mqtt_manager_set_callback( MM_ORDER_CONNECT, mqtt_connecting_cb);

}


void iot_logging(void) {
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);

    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);

    /*
    esp_log_level_set("TRANS_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANS_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANS", ESP_LOG_VERBOSE);
    */
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    // disable buffering on stdout
    setvbuf(stdout, NULL, _IONBF, 0);	
}


uint32_t iot_timestamp() {
#ifdef ESP32
    return esp_log_timestamp(); // this is not found on ESP8266
#else
    return esp_log_early_timestamp(); // .. but then again, this does not return same value between cores on ESP32 (??)
#endif
}



