import configparser
import logging
# import sys
import time

import cayenne.client

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

config_path = '../../config/config.ini'

updateInterval = 10


# The callback for when a message is received from Cayenne.
def on_message(message):
    print('message received: ' + str(message))


if __name__ == '__main__':
    config = configparser.ConfigParser()
    config.read(config_path)

    mqtt_username = config['mqtt']['username']
    mqtt_password = config['mqtt']['password']
    mqtt_client_id = config['mqtt']['client_id']

    client = cayenne.client.CayenneMQTTClient()
    client.on_message = on_message
    client.begin(mqtt_username, mqtt_password, mqtt_client_id)

    updateLast = 0

    i = 1

    loopStart = time.time()
    while ((time.time() - loopStart) < 60):
        client.loop()

        if ((time.time() - updateLast) > updateInterval):
            client.virtualWrite(1, i, 'null', 'd')
            # client.virtualWrite(2, i)
            # client.virtualWrite(3, i)
            # client.virtualWrite(4, i)

            if i == 1:
                i = 0
            else:
                i = 1

            updateLast = time.time()

        time.sleep(0.1)
