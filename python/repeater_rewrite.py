import configparser
import datetime
import logging
import serial
import sys
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

"""
garage_state = {
    'doorState': None,
    'lastOpened': None,
    'doorLock': None,
    'buttonLock': None,
    'doorAlarm': None
}
"""


# Trigger actions from received MQTT messages
def trigger_action(target, action=None):
    pass


def process_message(msg):
    process_success = True

    try:
        pass

    except Exception as e:
        logger.exception(e)
        process_success = False

    finally:
        return process_return


def update_log():
    pass


if __name__ == '__main__':
    # Flush serial receive buffer
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

            if '\n' in command:
                logger.error('NEWLINE FOUND IN STRIPPED COMMAND! Exiting.')
                sys.exit(1)

            rebroadcast = False

            # Message from controller
            if start_char == '^':
                if end_char == '@':
                    # Message from controller --> repeater
                    process_message(cmd)
                elif end_char == '+':
                    # Rebroadcast
                    rebroadcast = True
                else:
                    logger.error('Invalid end character in command from controller.')

            # Message from remote
            elif start_char == '+':
                if end_char == '@':
                    # Command from remote --> repeater
                    pass
                elif end_char == '^':
                    # Rebroadcast
                    rebroadcast = True
                else:
                    logger.error('Invalid end character in command from remote.')

            # Message from API
            elif start_char == '*':
                if end_char == '@':
                    # Command from API --> repeater
                    pass
                elif end_char == '^':
                    # Command from API --> controller
                    pass
                elif end_char == '+':
                    # Command from API --> remote
                    pass

            if rebroadcast is True:
                logger.debug('Rebroadcasting: ' + str(cmd))
                ser.write(cmd)

        time.sleep(0.01)
