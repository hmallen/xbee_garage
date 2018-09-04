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


def trigger_action(target, source=None, action=None):
    action_message = ''

    if target == 'door':
        if source is None:
            action_message += '@'
        else:
            action_message += source
        action_message += '>door^'
    else:
        logger.error('Unrecognized target in trigger_action().')

    logger.debug('action_message: ' + action_message)

    if len(action_message) > 0:
        logger.info('Sending action trigger command.')
        bytes_written = ser.write(action_message.encode('utf-8'))
        logger.debug('bytes_written: ' + str(bytes_written))
    else:
        logger.warning('Command not sent due to error.')


def process_message(msg):
    process_success = True

    try:
        msg_content = msg[1:-1]
        logger.debug('msg_content: ' + msg_content)
        msg_type = msg_content[0]
        logger.debug('msg_type: ' + msg_type)

        if msg_type == '#':
            # Request/Response
            msg_purpose = msg_content.split('/')[0][1:]
            logger.debug('msg_purpose: ' + msg_purpose)

            if msg_purpose == 'request':
                msg_request = msg_content.split('/')[1][:-1]
                logger.debug('msg_request: ' + msg_request)

                response_prefix = '@#'
                response_suffix = '#^'
                response_list = []

                # Handle request from controller
                if msg_request == 'ping':
                    # @#ping#^
                    logger.info('Ping requested.')
                    response_list.append(response_prefix + 'ping' + response_suffix)
                elif msg_request == 'settings':
                    # Get settings from MongoDB
                    lock_states = db[collections['state']].find_one({'_id': 'locks'})
                    for lock in lock_states:
                        logger.debug('lock: ' + lock)
                        if lock != '_id':
                            lock_response = '@%' + lock
                            # Value construction
                            if lock_states[lock] == True:
                                lock_response += '$1'
                            else:
                                lock_response += '$0'
                            lock_response += '^'
                            logger.debug('lock_response: ' + lock_response)
                            response_list.append(lock_response)
                elif msg_request == 'time':
                    # Construct datetime message
                    time_message = response_prefix + 'time/'
                    time_message += datetime.datetime.now().strftime('m%md%dy%YH%HM%MS%S')
                    time_message += response_suffix
                    logger.debug('time_message: ' + time_message)
                    response_list.append(time_message)

                if len(response_list) > 0:
                    for response in response_list:
                        logger.info('Broadcasting response: ' + response)
                        bytes_written = ser.write(response.encode('utf-8'))
                        logger.debug('bytes_written: ' + str(bytes_written))
                        time.sleep(0.1)

        elif msg_type == '%':
            pass

    except Exception as e:
        logger.exception(e)
        process_success = False

    finally:
        return process_success


def update_log():
    pass


def flush_buffer():
    logger.info('Flushing serial buffer.')
    if ser.in_waiting > 0:
        while ser.in_waiting > 0:
            c = ser.read()
            time.sleep(0.1)


if __name__ == '__main__':
    # Flush serial receive buffer
    flush_buffer()

    try:
        while (True):
            if ser.in_waiting > 0:
                cmd_raw = ser.readline()
                logger.debug('cmd_raw: ' + str(cmd_raw))

                try:
                    command = cmd_raw.decode().rstrip('\n')
                    logger.debug('command: ' + command)

                    cmd = command.encode('utf-8')
                    logger.debug('cmd: ' + str(cmd))

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
                            process_message(command)
                        elif end_char == '+':
                            # Rebroadcast
                            rebroadcast = True
                        else:
                            logger.error('Invalid end character in command from controller.')

                    # Message from remote
                    elif start_char == '+':
                        if end_char == '@':
                            # Command from remote --> repeater
                            logger.warning('No handling implemented for command from remote --> repeater.')
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

                    elif start_char == '@':
                        if end_char == '^':
                            rebroadcast = True

                    if rebroadcast is True:
                        logger.debug('Rebroadcasting: ' + str(cmd))
                        bytes_written = ser.write(cmd)
                        logger.debug('bytes_written: ' + str(bytes_written))

                except UnicodeDecodeError as e:
                    logger.exception('UnicodeDecodeError: ' + str(e))

                except Exception as e:
                    logger.exception('Exception: ' + str(e))

            time.sleep(0.01)

    except KeyboardInterrupt:
        logger.info('Exit signal received.')

    finally:
        logger.info('Exiting.')
