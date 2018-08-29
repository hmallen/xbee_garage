import configparser
import datetime
import logging
import serial
# import sys
import time

import cayenne.client
from pymongo import MongoClient

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

mqtt_loop_interval = 10

config_path = '../config/config.ini'

config = configparser.ConfigParser()
config.read(config_path)

mqtt_username = config['mqtt']['username']
mqtt_password = config['mqtt']['password']
mqtt_client_id = config['mqtt']['client_id']

mqtt_client = cayenne.client.CayenneMQTTClient()

mongo_uri = config['mongodb']['uri']
mongo_db = config['mongodb']['database']

collections = {
    'log': config['mongodb']['collection_log'],
    'state': config['mongodb']['collection_state']
}

db = MongoClient(mongo_uri)[mongo_db]

ser = serial.Serial(
    port='/dev/ttyUSB0',
    baudrate=9600,
    # stimeout=0.5
)


# Trigger actions from received MQTT messages
def trigger_action(target, action=None):
    command_success = True

    if target == 'door':
        logger.debug('Constructing door ' + action + ' command.')

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

            logger.info('Sending door ' + action + ' command.')
            ser.write(door_command)

        else:
            logger.error('Error while constructing command. No command sent.')
    else:
        logger.error('Unknown target variable passed to trigger_action().')


# Function for updating dashboard values via MQTT
def mqtt_update(variable, value):
    update_ready = True

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
    elif variable == 'doorAlarmActive':
        channel = 4
        data_unit = 'd'
    elif variable == 'heartbeatLast':
        update_ready = False

    if update_ready is True:
        logger.debug('Updating "' + variable + '" via MQTT.')
        mqtt_client.virtualWrite(channel, value, data_type, data_unit)
    else:
        logger.debug('Skipping "' + variable + '" update.')


# Callback for messages received from Cayenne
def on_message(msg):
    logger.info('msg [on_message]: ' + str(msg))

    logger.debug('msg.client_id: ' + msg.client_id)
    logger.debug('msg.topic: ' + msg.topic)
    logger.debug('msg.channel: ' + str(msg.channel))
    logger.debug('msg.msg_id: ' + msg.msg_id)
    logger.debug('msg.value: ' + msg.value)

    # If door button channel, trigger door open/close via serial
    if msg.channel == 5:
        logger.info('Received door trigger command via MQTT.')
        trigger_action('door')


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

        elif start_char == '#':
            if msg_decoded == '#TS#':
                dt_current = datetime.datetime.now()
                time_message = dt_current.strftime('#m%md%dy%YH%HM%MS%S#')
                bytes_written = ser.write(time_message.encode('utf-8'))
                logger.debug('bytes_written: ' + str(bytes_written))
            else:
                logger.error('Unrecognized time sync message received from controller.')

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

    garage_state = {
        'doorOpen': None,
        'lastOpened': None,
        'lockStateDoor': None,
        'lockStateButton': None,
        'doorAlarmActive': None
    }

    mqtt_client.on_message = on_message
    mqtt_client.begin(mqtt_username, mqtt_password, mqtt_client_id)

    mqtt_client.loop()
    mqtt_loop_last = time.time()

    new_msg = False

    # Request starting values from controller and update
    logger.info('Requesting full data update from controller.')
    # update_request = '@UF^'
    update_request = '@UA^'
    logger.debug('update_request: ' + str(update_request))
    bytes_written = ser.write(update_request.encode('utf-8'))
    logger.debug('bytes_written: ' + str(bytes_written))

    while (True):
        if ser.in_waiting > 0:
            c = ser.read()
            if c == b'@' or c == b'^' or c == b'&' or c == b'#':
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
                            # time.sleep(0.05)
                    else:
                        logger.error('Error while processing message.')
            elif new_msg is True:
                msg += c
            else:
                logger.warning('Orphaned character(s) in serial buffer. Flushing buffer.')
                while ser.in_waiting > 0:
                    orph_char = ser.read()
                    logger.debug('orph_char: ' + str(orph_char))

        if (time.time() - mqtt_loop_last) > mqtt_loop_interval:
            mqtt_client.loop()
            mqtt_loop_last = time.time()

        time.sleep(0.1)
