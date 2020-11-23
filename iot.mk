# Add IOT_FIRMWARE_SERVER_LOCAL_URI and IOT_FIRMWARE_SERVER_URI to .bash_profile
# For example:
# export IOT_FIRMWARE_SERVER_WWW_ROOT_URI=web-user@firmware-server.local:/var/www
# export IOT_FIRMWARE_SERVER_URI=http://firmware-server.local:80


# --- OTA update can be done with project name (this updates all nodes with the same project name that are subscribed to /home/control/PROJECT_NAME/#) :
# make mqtt-ota 
# .. or alternatively with a specific node name (it is subscribed to subscribed to /home/control/NODE_NAME/#) :
# NODE_NAME="examplenode-e975c1" make mqtt-node-ota 

.PHONY: upload
.PHONY: app2
.PHONY: flash2
.PHONY: ota

upload:
	sftp ${IOT_FIRMWARE_SERVER_WWW_ROOT_URI} <<< $$'put build/${PROJECT_NAME}.bin'

app2: app upload

flash2: flash upload

mqtt-ota: app upload
	mqtt pub -t /home/control/${PROJECT_NAME}/update_firmware -m ${IOT_FIRMWARE_SERVER_URI}/${PROJECT_NAME}.bin

mqtt-node-ota: app upload
	mqtt pub -t /home/control/${NODE_NAME}/update_firmware -m ${IOT_FIRMWARE_SERVER_URI}/${PROJECT_NAME}.bin
