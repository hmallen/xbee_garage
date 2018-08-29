import configparser
import datetime
import cayenne.client

config = configparser.ConfigParser()
config.read('resources/config.ini')

mqtt_username = config['mqtt']['username']
mqtt_password = config['mqtt']['password']
mqtt_client_id = config['mqtt']['client_id']


# Function for updating dashboard values via MQTT
def trigger_door(variable, value):
    update_ready = True

    channel = None
    data_type = 'null'
    data_unit = None

    logger.debug('Updating "' + variable + '" via MQTT.')
    mqtt_client.virtualWrite(channel, value, data_type, data_unit)


if __name__ == '__main__':
    mqtt_client = cayenne.client.CayenneMQTTClient()
    mqtt_client.on_message = on_message
    mqtt_client.begin(mqtt_username, mqtt_password, mqtt_client_id)

    mqtt_client.loop()
