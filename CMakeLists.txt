idf_component_register(SRCS "node-framework.c" "led_task.c" 
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_netif nvs_flash esp-tls esp_http_client esp_https_ota mqtt esp-wifi-manager
                    )

