import configparser
import datetime
# import io
import logging
# from multiprocessing import Process
import serial
# import sys
import time

import cayenne.client

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

config_path = '../../config/config.ini'

config = configparser.ConfigParser()
config.read(config_path)


def trigger_action(target, action=None):
    if target == "door":
        pass
    else:
        logger.error('Unknown target variable passed to trigger_action().')


# Function for updating dashboard values via MQTT
def mqtt_update(variable, value):
    channel = None
    data_type = 'null'
    data_unit = None

    if variable == 'doorOpen':
        channel = 1
        data_unit = 'd'
    elif variable == 'lockStateDoor':
        channel = 2
        data_unit = 'd'
    elif variable == 'lockStateButton':
        channel = 3
        data_unit = 'd'
    elif variable == 'doorAlarm':
        channel = 4
        data_unit = 'd'
    elif variable == 'heartbeatLast':
        pass

    logger.debug('Updating "' + variable + '" via MQTT.')
    mqtt_client.virtualWrite(channel, value, data_type, data_unit)


# Callback for messages received from Cayenne
def on_message(msg):
    logger.debug('msg [on_message]: ' + str(msg))

    logger.debug('msg.client_id: ' + msg.client_id)
    logger.debug('msg.topic: ' + msg.topic)
    logger.debug('msg.channel: ' + str(msg.channel))
    logger.debug('msg.msg_id: ' + msg.msg_id)
    logger.debug('msg.value: ' + msg.value)

    # If door button channel, trigger door open/close via serial
    if msg.channel == 5:
        trigger_action("door")


if __name__ == '__main__':
    mqtt_username = config['mqtt']['username']
    mqtt_password = config['mqtt']['password']
    mqtt_client_id = config['mqtt']['client_id']

    mqtt_client = cayenne.client.CayenneMQTTClient()
    mqtt_client.on_message = on_message
    mqtt_client.begin(mqtt_username, mqtt_password, mqtt_client_id)

    """
    ser = serial.Serial(
        port='/dev/ttyUSB0',
        baudrate=9600,
        # stimeout=0.5
    )
    """

    while (True):
        mqtt_client.loop()
        time.sleep(0.01)
