# Add IOT_FIRMWARE_SERVER_LOCAL_URI and IOT_FIRMWARE_SERVER_URI to .bash_profile
# For example:
# export IOT_FIRMWARE_SERVER_WWW_ROOT_URI=web-user@firmware-server.local:/var/www
# export IOT_FIRMWARE_SERVER_URI=http://firmware-server.local:80
#
# Please clean and rebuild the project after changing the variables (Ninja will not detect changes and build.ninja contains stale values)
# These OTA commands use the preconfigured command line HiveMQ MQTT client (https://www.hivemq.com/blog/mqtt-cli/) but feel free to modify the
# commands to suit your favourite MQTT tool (mosquitto_pub for example)

# --- OTA update can be done with project name (this updates all nodes with the same project name that are subscribed to /home/control/PROJECT_NAME/#) :
# idf.py mqtt-ota 
# .. or alternatively with a specific node name (it is subscribed to subscribed to /home/control/NODE_NAME/#) :
# NODE_NAME="examplenode-e975c1" idf.py mqtt-node-ota 

add_custom_target(upload DEPENDS app
	COMMAND sftp $ENV{IOT_FIRMWARE_SERVER_WWW_ROOT_URI} <<< $$'put ${PROJECT_NAME}.bin'
	)

add_custom_target(mqtt-ota DEPENDS upload
	COMMAND mqtt pub -t /home/control/${PROJECT_NAME}/update_firmware -m $ENV{IOT_FIRMWARE_SERVER_URI}/${PROJECT_NAME}.bin
	)

add_custom_target(mqtt-node-ota DEPENDS upload
	COMMAND mqtt pub -t /home/control/$ENV{NODE_NAME}/update_firmware -m $ENV{IOT_FIRMWARE_SERVER_URI}/${PROJECT_NAME}.bin
	)

add_custom_target(mqtt-restart
	COMMAND mqtt pub -t /home/control/$ENV{NODE_NAME}/restart -m 0
	)
