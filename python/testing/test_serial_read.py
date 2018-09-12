import logging
import serial
# import sys
import time

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

ser = serial.Serial(
    port='/dev/ttyUSB0',
    baudrate=19200,
    timeout=1
)


if __name__ == '__main__':
    while (True):
        if ser.in_waiting > 0:
            # Receive, decode, and strip trailing \r\n from incoming message
            msg = ser.readline()
            logger.debug('msg: ' + str(msg))

            msg_stripped = msg.rstrip(b'\n')
            logger.debug('msg_stripped: ' + str(msg_stripped))

            msg_decoded = msg_stripped.decode()
            logger.debug('msg_decoded: ' + msg_decoded)

        time.sleep(0.01)
