import configparser
import datetime
import logging
import serial
# import sys
import time

from pymongo import MongoClient

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

config_path = '../config/config.ini'

config = configparser.ConfigParser()
config.read(config_path)

mongo_uri = config['mongodb']['uri']
mongo_db = config['mongodb']['database']

collections = {
    'log': config['mongodb']['collection_log'],
    'state': config['mongodb']['collection_state']
}

db = MongoClient(mongo_uri)[mongo_db]

ser = serial.Serial(
    port='/dev/ttyUSB0',
    baudrate=19200,
    timeout=1
)

garage_state = {
    'doorState': None,
    'lastOpened': None,
    'doorLock': None,
    'buttonLock': None,
    'doorAlarm': None
}


# Trigger actions from received MQTT messages
def trigger_action(target, action=None):
    command_success = True

    if target == 'door':
        logger.debug('Constructing door ' + str(action) + ' command.')

        door_command = '@D'
        if action == 'open':
            door_command += 'O'
        elif action == 'close':
            door_command += 'C'
        elif action == None:
            door_command += 'F'
        else:
            logger.error('Unrecognized action variable passed to trigger_action().')
            command_success = False

        if command_success is True:
            door_command += '^'
            door_command = door_command.encode('utf-8')
            logger.debug('door_command: ' + str(door_command))

            logger.info('Sending door ' + str(action) + ' command.')
            ser.write(door_command)

        else:
            logger.error('Error while constructing command. No command sent.')
    else:
        logger.error('Unknown target variable passed to trigger_action().')


def process_message(msg):
    process_return = {'success': True}

    try:
        pass

    except Exception as e:
        logger.exception(e)
        process_return['success'] = False

    finally:
        return process_return


def update_state(key, value):
    garage_state[key] = value


def update_log():
    pass


if __name__ == '__main__':
    # Flush serial receive buffer to start fresh
    if ser.in_waiting > 0:
        logger.info('Flushing serial buffer.')

        while ser.in_waiting > 0:
            c = ser.read()
            time.sleep(0.1)

    while (True):
        if ser.in_waiting > 0:
            cmd = ser.readline().rstrip(b'\n')
            logger.debug('cmd: ' + str(cmd))
            command = cmd.decode()
            logger.debug('command: ' + command)

            start_char = command[0]
            logger.debug('start_char: ' + start_char)
            end_char = command[-1]
            logger.debug('end_char: ' + end_char)

            if start_char == '^':
                if end_char == '@':
                    pass
                elif end_char == '+':
                    # Rebroadcast
                    pass
                else:
                    logger.error('Invalid end character in command from controller.')
            elif start_char == '+':
                if end_char == '@':
                    pass
                elif end_char == '^':
                    # Rebroadcast
                    pass
                else:
                    logger.error('Invalid end character in command from remote.')

        time.sleep(0.01)
