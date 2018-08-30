import time
import os
import motor_runner
#import net_check
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient

def run_motor(self, params, packet):
    motor_runner.pulse(2)
    myMQTTClient.publish('home/motorRunStatus', packet.payload, 0)

myMQTTClient = AWSIoTMQTTClient("XBeeGarage_RPi3") #random key, if another connection using the same key is opened the previous one is auto closed by AWS IOT

myMQTTClient.configureEndpoint("YOUR AWS IOT ENDPOINT HERE", 8883)

certRootPath = '/home/pi/xbee_garage/resources/aws/certs/XBeeGarage_RPi3/'
myMQTTClient.configureCredentials(
    "{}root-ca.pem".format(certRootPath),
    "{}XBeeGarage_RPi3.pem.key".format(certRootPath),
    "{}cloud.pem.crt".format(certRootPath))
)

myMQTTClient.configureOfflinePublishQueueing(-1) # Infinite offline Publish queueing
myMQTTClient.configureDrainingFrequency(2) # Draining: 2 Hz
myMQTTClient.configureConnectDisconnectTimeout(10) # 10 sec
myMQTTClient.configureMQTTOperationTimeout(5) # 5 sec

myMQTTClient.connect()
myMQTTClient.subscribe("home/runMotor", 1, run_motor)

def looper():
    while True:
        time.sleep(5) #sleep for 5 seconds and then sleep again
        #check_internet()

looper()

def function_handler(event, context):
    return
