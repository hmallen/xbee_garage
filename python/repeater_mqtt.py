import configparser
import datetime
import logging
import sys
import time

import paho.mqtt.client as mqtt
from pymongo import MongoClient
import serial

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

config_path = '../config/config.ini'

config = configparser.ConfigParser()
config.read(config_path)

serial_device = config['serial']['device']
serial_baudrate = int(config['serial']['baudrate'])

mqtt_url = config['mqtt']['url']
logger.debug('mqtt_url: ' + mqtt_url)
mqtt_port = int(config['mqtt']['port'])
logger.debug('mqtt_port: ' + str(mqtt_port))
mqtt_keepalive = int(config['mqtt']['keepalive'])
logger.debug('mqtt_keepalive: ' + str(mqtt_keepalive))
mqtt_username = config['mqtt']['username']
logger.debug('mqtt_username: ' + mqtt_username)
mqtt_password = config['mqtt']['password']
logger.debug('mqtt_password: ' + mqtt_password)
mqtt_client_id = config['mqtt']['client_id']
logger.debug('mqtt_client_id: ' + mqtt_client_id)

# mqtt_topics = ('OpenHab/#', 0)

"""
mqtt_topics = [
    ('OpenHAB/sensors/#', 0),
    ('OpenHAB/locks/#', 0),
    ('OpenHAB/actions/#', 0)
]
"""
"""
mqtt_topics = [
    # ('OpenHAB/sensors/doorState', 0),
    ('OpenHAB/locks/doorLock', 0),
    ('OpenHAB/locks/buttonLock', 0),
    ('OpenHAB/actions/doorTrigger', 0)
]
"""

# Subscribe to actions topic to receive commands from OpenHAB
mqtt_topics = ('OpenHAB/actions/#', 0)

boolean_reference = {
    '0': [0, '0', 'OFF', 'CLOSE', 'CLOSED'],
    '1': [1, '1', 'ON', 'OPEN', 'OPENED']
}

mongo_uri = config['mongodb']['uri']
mongo_db = config['mongodb']['database']

collections = {
    'log': config['mongodb']['collection_log'],
    'state': config['mongodb']['collection_state']
}

garage_state = {
    'doorState': None,
    'lightState': None,
    'doorLock': None,
    'buttonLock': None,
    'doorAlarm': None,
    'alarmTriggered': None
}


## MQTT Functions ##
def on_connect(mqtt_client, userdata, flags, rc):
    logger.debug('Connected with result code: ' + str(rc))
    mqtt_client.subscribe('$sys/#')
    mqtt_client.subscribe(mqtt_topics)
    #for topic in mqtt_topics:
        #mqtt_client.subscribe(topic)


def on_disconnect(mqtt_client, userdata, rc):
    logger.debug('Disconnected with result code: ' + str(rc))


def on_subscribe(mqtt_client, userdata, msg_id, granted_qos):
    logger.debug('Subscribed with msg_id: ' + str(msg_id))


def on_unsubscribe(mqtt_client, userdata, msg_id):
    logger.debug('Unsubscribed with msg_id: ' + str(msg_id))


def on_message(mqtt_client, userdata, msg):
    """
    Topics:
    Actions --> OpenHAB/action/{target}

    ex. Button Lock switch toggled via OpenHAB interface
    DEBUG:__main__:msg.topic: OpenHAB/locks/buttonLock
    DEBUG:__main__:msg.payload: b'ON'
    """
    logger.info('Received MQTT message.')
    logger.debug('msg.topic: ' + msg.topic)
    logger.debug('msg.payload: ' + str(msg.payload))

    target_type = msg.topic.split('/')[1]
    logger.debug('target_type: ' + target_type)
    target_var = msg.topic.split('/')[2]
    logger.debug('target_var: ' + target_var)

    target_action = False
    msg_action = msg.payload.decode()
    logger.debug('msg_action: ' + msg_action)
    if msg_action in boolean_reference['1']:
        target_action = True
    logger.debug('target_action: ' + str(target_action))

    logger.info('Triggering action. (' + target_var + '/' + target_action + ')')
    trigger_action(target_var, target_action)


def on_publish(mqtt_client, userdata, msg_id):
    logger.debug('msg_id: ' + str(msg_id))


def publish_update(update_var, update_val):
    publish_success = True

    topic = 'OpenHAB/'
    if 'Lock' in update_var:
        topic += 'locks/'
    elif 'State' in update_var:
        topic += 'sensors/'
    else:
        logger.error('Unhandled variable type in publish_update().')
        publish_success = False

    if publish_success is True:
        topic += update_var
        logger.debug('topic: ' + topic)

        if update_val == 0:
            if 'sensors' in topic:
                update_str = 'OPEN'
            else:
                update_str = 'ON'
        else:
            if 'sensors' in topic:
                update_str = 'CLOSED'
            else:
                update_str = 'OFF'
        logger.debug('update_str: ' + update_str)

        logger.info('Publishing MQTT update.')

        (rc, msg_id) = mqtt_client.publish(topic, update_str, qos=0)
        logger.debug('rc: ' + str(rc))
        logger.debug('msg_id: ' + str(msg_id))


## Other Functions ##
def trigger_action(target, action=None, source='@'):
    action_message = ''

    if action != None:
        for boolean_key in boolean_reference:
            if action in boolean_reference[boolean_key]:
                action_conv = int(boolean_key)
                break
        else:
            logger.error('Unrecognized action passed to trigger_action().')
            action_conv = -1

    if action_conv >= 0:
        action_message = source + ">" + target + "<" + action_conv + "^"
        logger.debug('action_message: ' + action_message)

    if len(action_message) > 3:
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
            msg_var = msg_content.lstrip('%').split('$')[0]
            logger.debug('msg_var: ' + msg_var)
            msg_val = msg_content.lstrip('%').split('$')[1]
            logger.debug('msg_val: ' + msg_val)

            if garage_state[msg_var] != msg_val:
                logger.info('Variable changed. Updating via MQTT.')
                publish_update(msg_var, msg_val)
                garage_state[msg_var] = msg_val
            else:
                logger.debug('Variable unchanged. Skipping update.')

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
    # Initialize clients
    db = MongoClient(mongo_uri)[mongo_db]

    mqtt_client = mqtt.Client(clean_session=True) # client_id=mqtt_client_id)
    mqtt_client.enable_logger()
    mqtt_client.on_connect = on_connect
    mqtt_client.on_disconnect = on_disconnect
    mqtt_client.on_subscribe = on_subscribe
    mqtt_client.on_unsubscribe = on_unsubscribe
    mqtt_client.on_message = on_message
    mqtt_client.on_publish = on_publish
    mqtt_client.connect(mqtt_url, port=mqtt_port) # , keepalive=mqtt_keepalive)

    ser = serial.Serial(
        port=serial_device,
        baudrate=serial_baudrate,
        timeout=1
    )

    # Flush serial receive buffer
    flush_buffer()

    try:
        # Start threaded MQTT loop to keep incoming/outgoing MQTT updated
        mqtt_client.loop_start()

        while (True):
            if ser.in_waiting > 0:
                cmd_raw = ser.readline()
                # logger.debug('cmd_raw: ' + str(cmd_raw))

                try:
                    command = cmd_raw.decode().rstrip('\n')
                    # logger.debug('command: ' + command)

                    cmd = command.encode('utf-8')
                    # logger.debug('cmd: ' + str(cmd))

                    start_char = command[0]
                    # logger.debug('start_char: ' + start_char)
                    end_char = command[-1]
                    # logger.debug('end_char: ' + end_char)

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

            # time.sleep(0.01)
            time.sleep(1)

    except KeyboardInterrupt:
        logger.info('Exit signal received.')

        logger.info('Stopping threaded MQTT loop.')
        mqtt_client.loop_stop()

        logger.info('Disconnecting from MQTT broker.')
        mqtt_client.disconnect()

    finally:
        logger.info('Exiting.')
