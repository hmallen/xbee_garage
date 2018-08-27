import configparser
import logging
import sys
import time

import cayenne.client

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

config_path = '../../config/config.ini'


if __name__ == '__main__':
    config = configparser.ConfigParser()
    config.read(config_path)

    mqtt_username = config['mqtt']['username']
    mqtt_password = config['mqtt']['password']
    mqtt_client_id = config['mqtt']['client_id']

    client = cayenne.client.CayenneMQTTClient()
    client.begin(mqtt_username, mqtt_password, mqtt_client_id)

    updateLast = 0
    i = 0

    while (True):
        client.loop()

        if ((time.time() - updateLast) > updateInterval):
            client.celsiusWrite(1, i)
            client.luxWrite(2, (i * 10))
            client.hectoPascalWrite(3, (i + 800))
            
            updateLast = time.time()
            i += 1
