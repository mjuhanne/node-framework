# Add IOT_FIRMWARE_SERVER_LOCAL_URI and IOT_FIRMWARE_SERVER_URI to .bash_profile
# For example:
# export IOT_FIRMWARE_SERVER_WWW_ROOT_URI=web-user@firmware-server.local:/var/www
# export IOT_FIRMWARE_SERVER_URI=http://firmware-server.local:80

# --- OTA update can be done with project name (if there's only node running with the code and it's subscribe to /home/node/PROJECT_NAME/+) :
# make mqtt-ota 
# .. or alternatively with a specific node name (it is subscribed to subscribed to /home/node/NODE_NAME/+) :
# make mqtt-node-ota NODE_NAME="examplenode-e975c1"

add_custom_target(upload DEPENDS app
	COMMAND sftp ${IOT_FIRMWARE_SERVER_WWW_ROOT_URI} <<< $$'put ${PROJECT_NAME}.bin')

add_custom_target(mqtt-ota DEPENDS upload
	COMMAND mqtt pub -t /home/node/${PROJECT_NAME}/update_firmware -m ${IOT_FIRMWARE_SERVER_URI}/${PROJECT_NAME}.bin
	)

add_custom_target(mqtt-node-ota DEPENDS upload
	COMMAND mqtt pub -t /home/node/${NODE_NAME}/update_firmware -m ${IOT_FIRMWARE_SERVER_URI}/${PROJECT_NAME}.bin
	)
