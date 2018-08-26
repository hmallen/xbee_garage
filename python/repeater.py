import logging
import serial
import sys
import time

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

ser = serial.Serial(
    port='/dev/ttyUSB0',
    baudrate=9600,
    timeout=0.5
)


if __name__ == '__main__':
    while (True):
        if ser.in_waiting > 0:
            # Receive, decode, and strip trailing \r\n from incoming message
            #msg = ser.readline()
            msg = ser.readlines()
            #msg_decoded = msg.decode().rstrip('\r\n')
            #logger.debug('msg_decoded: ' + msg_decoded)
            pprint(msg)

            # Rebroadcast message for remote units (repeater function)
            for line in msg:
                ser.write(line)
            logger.debug('Rebroadcasted message.')

        time.sleep(0.01)
