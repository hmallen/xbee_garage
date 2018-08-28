import configparser
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

config_path = '../config/config.ini'

config = configparser.ConfigParser()
config.read(config_path)


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

    logger.debug('Updating "' + variable + '" via MQTT.')
    mqtt_client.virtualWrite(channel, value, data_type, data_unit)


# Callback for messages received from Cayenne
def on_message(msg):
    logger.info('msg [on_message]: ' + str(msg))


def process_message(msg):
    process_return = {'success': True, 'rebroadcast': False, 'message': None}

    try:
        msg_decoded = msg.decode()
        logger.debug('msg_decoded: ' + msg_decoded)

        start_char = msg_decoded[0]
        logger.debug('start_char: ' + start_char)
        end_char = msg_decoded[-1]
        logger.debug('end_char: ' + end_char)

        if start_char == '@':
            if end_char == '^':
                process_return['message'] = msg
                process_return['rebroadcast'] = True
            else:
                logger.error('Unrecognized end character in command from remote --> controller.')
        elif start_char == '^':
            if end_char == '@':
                process_return['message'] = msg
                process_return['rebroadcast'] = True
            else:
                logger.error('Unrecognized end character in command from controller --> remote')
        elif start_char == '&':
            updates = [(var.split('$')[0], var.split('$')[1]) for var in msg_decoded[3:-1].split('%')]
            logger.debug('updates: ' + str(updates))

            [mqtt_update(update[0], update[1]) for update in updates]

    except Exception as e:
        logger.exception(e)
        process_return['success'] = False

    finally:
        return process_return


if __name__ == '__main__':
    mqtt_username = config['mqtt']['username']
    mqtt_password = config['mqtt']['password']
    mqtt_client_id = config['mqtt']['client_id']

    mqtt_client = cayenne.client.CayenneMQTTClient()
    mqtt_client.on_message = on_message
    mqtt_client.begin(mqtt_username, mqtt_password, mqtt_client_id)

    ser = serial.Serial(
        port='/dev/ttyUSB0',
        baudrate=9600,
        # stimeout=0.5
    )

    new_msg = False

    while (True):
        if ser.in_waiting > 0:
            c = ser.read()
            if c == b'@' or c == b'^' or c == b'&':
                if new_msg is False:
                    new_msg = True
                    msg = c
                else:
                    msg += c
                    process_result = process_message(msg)
                    new_msg = False

                    if process_result['success'] is True:
                        if process_result['rebroadcast'] is True:
                            bytes_written = ser.write(process_result['message'])
                            logger.debug('bytes_written: ' + str(bytes_written))
                            time.sleep(0.05)
                    else:
                        logger.error('Error while processing message.')
            elif new_msg is True:
                msg += c
            else:
                logger.warning('Orphaned character(s) in serial buffer. Flushing buffer.')
                while ser.in_waiting > 0:
                    orph = ser.read()
                    logger.debug('orph: ' + str(orph))

        time.sleep(0.01)
