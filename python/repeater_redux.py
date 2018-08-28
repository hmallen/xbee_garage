import configparser
import logging
# from pprint import pprint
import serial
# import sys
import time

import cayenne.client

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

config_path = '../config/config.ini'


# Function for updating dashboard values via MQTT
def update_value(variable, value):
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


# Callback for messages received from Cayenne
def on_message(message):
    print('message received: ' + str(message))


if __name__ == '__main__':
    ser = serial.Serial(
        port='/dev/ttyUSB0',
        baudrate=9600,
        # timeout=1
    )

    config = configparser.ConfigParser()
    config.read(config_path)

    mqtt_username = config['mqtt']['username']
    mqtt_password = config['mqtt']['password']
    mqtt_client_id = config['mqtt']['client_id']

    client = cayenne.client.CayenneMQTTClient()
    client.on_message = on_message
    client.begin(mqtt_username, mqtt_password, mqtt_client_id)

    while (True):
        if ser.in_waiting > 0:
            # msg = b''
            while ser.in_waiting > 0:
                msg = ser.read()
                # msg += c
                # logger.debug('msg: ' + str(msg))

            msg_decoded = msg.decode()
            logger.debug('msg_decoded: ' + msg_decoded)

            start_char = msg_decoded[0]
            logger.debug('start_char: ' + start_char)
            end_char = msg_decoded[-1]
            logger.debug('end_char: ' + end_char)

            rebroadcast_msg = False

            if start_char == '&':
                if len(msg_decoded) > 1 and end_char == '&':
                    # Update Cayenne dashboard via MQTT client
                    updates = [(var.split('$')[0], var.split('$')[1]) for var in msg_decoded[3:].split('%')]
                    logger.debug('updates: ' + str(updates))

                    [update_value(update[0], update[1]) for update in updates]

            elif start_char == '@':
                logger.debug('Source: Remote / Target: Controller')
                if end_char == '^':
                    rebroadcast_msg = True
                else:
                    logger.error('Unrecognized end character in command from remote --> controller.')

            elif start_char == '^':
                logger.debug('Source: Controller / Target: Remote')
                if end_char == '@':
                    rebroadcast_msg = True
                else:
                    logger.error('Unrecognized end character in command from controller --> remote.')

            else:
                logger.error('Unrecognized start character in received message.')

            # Rebroadcast message for remote units, if necessary (repeater function)
            if rebroadcast_msg == True:
                pass

        print(msg)

        time.sleep(0.01)
