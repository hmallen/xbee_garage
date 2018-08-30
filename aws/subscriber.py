import configparser
import logging
import os
import socket
import ssl

import paho.mqtt.client as paho

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

config_path = '../config/config_aws.ini'

connflag = False


def on_connect(client, userdata, flags, rc):
    global connflag
    connflag = True
    print("Connection returned result: " + str(rc))


def on_message(client, userdata, msg):
    print(msg.topic + " " + str(msg.payload))


# def on_log(client, userdata, level, buf):
    # print(msg.topic + " " + str(msg.payload))


if __name__ == '__main__':
    config = configparser.ConfigParser()
    config.read(config_path)

    awshost = config['awsiot']['awshost']
    awsport = config['awsiot']['awsport']
    clientId = config['awsiot']['clientId']
    thingName = config['awsiot']['thingName']
    caPath = config['certificates']['caPath']
    certPath = config['certificates']['certPath']
    keyPath = config['certificates']['keyPath']

    mqttc = paho.Client()
    mqttc.on_connect = on_connect
    mqttc.on_message = on_message
    # mqttc.on_log = on_log

    mqttc.tls_set(
        caPath,
        certfile=certPath,
        keyfile=keyPath,
        cert_reqs=ssl.CERT_REQUIRED,
        tls_version=ssl.PROTOCOL_TLSv1_2,
        ciphers=None
    )

    mqttc.connect(awshost, awsport, keepalive=60)

    mqttc.loop_forever()
