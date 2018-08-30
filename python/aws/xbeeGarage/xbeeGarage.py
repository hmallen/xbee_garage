from __future__ import print_function
import configparser
import datetime
import time

import cayenne.client


def on_message(msg):
    print(msg)


config = configparser.ConfigParser()
config.read('config/config.ini')

mqtt_username = config['mqtt']['username']
mqtt_password = config['mqtt']['password']
mqtt_client_id = config['mqtt']['client_id']

mqtt_client = cayenne.client.CayenneMQTTClient()
mqtt_client.on_message = on_message
mqtt_client.begin(mqtt_username, mqtt_password, mqtt_client_id)

time.sleep(1)
mqtt_client.virtualWrite(6, 1, 'digital_actuator', 'null')
time.sleep(1)
mqtt_client.loop()
time.sleep(1)
mqtt_client.virtualWrite(6, 0, 'digital_actuator', 'null')
time.sleep(1)
mqtt_client.loop()
