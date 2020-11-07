# Add IOT_FIRMWARE_SERVER_LOCAL_URI and IOT_FIRMWARE_SERVER_URI to .bash_profile
# For example:
# export IOT_FIRMWARE_SERVER_WWW_ROOT_URI=web-user@firmware-server.local:/var/www
# export IOT_FIRMWARE_SERVER_URI=http://firmware-server.local:80

# --- OTA update can be done with project name (if there's only node running with the code and it's subscribe to /home/node/PROJECT_NAME/+) :
# make mqtt-ota 
# .. or alternatively with a specific node name (it is subscribed to subscribed to /home/node/NODE_NAME/+) :
# make mqtt-node-ota NODE_NAME="fyrtur-e975c1"

.PHONY: upload
.PHONY: app2
.PHONY: flash2
.PHONY: ota

upload:
	sftp ${IOT_FIRMWARE_SERVER_WWW_ROOT_URI} <<< $$'put build/${PROJECT_NAME}.bin'

app2: app upload

flash2: flash upload

mqtt-ota: app upload
	mqtt pub -t /home/node/${PROJECT_NAME}/update_firmware -m ${IOT_FIRMWARE_SERVER_URI}/${PROJECT_NAME}.bin

mqtt-node-ota: app upload
	mqtt pub -t /home/node/${NODE_NAME}/update_firmware -m ${IOT_FIRMWARE_SERVER_URI}/${PROJECT_NAME}.bin
