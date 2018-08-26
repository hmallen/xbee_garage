import logging
import serial
import sys
import time

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

ser = serial.Serial(
    port='/dev/ttyUSB0',
    baudrate=9600#,
    #timeout=1
)


if __name__ == '__main__':
    while (True):
        if ser.in_waiting > 0:
            # Receive, decode, and strip trailing \r\n from incoming message
            msg = ser.readline()
            logger.debug('msg (decoded): ' + msg.decode().rstrip('\r\n'))

            # Rebroadcast message for remote units (repeater function)
            ser.write(msg)
            logger.debug('Rebroadcasted received message.')

        time.sleep(0.01)
