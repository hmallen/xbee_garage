import logging
# from pprint import pprint
import serial
# import sys
import time

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

ser = serial.Serial(
    port='/dev/ttyUSB0',
    baudrate=9600
)


if __name__ == '__main__':
    while (True):
        if ser.in_waiting > 0:
            msg = ser.read()

            """
            while ser.in_waiting > 0:
                # Receive and parse message
                pass
            """

            # Rebroadcast message for remote units (repeater function)
            #

        print(msg)

        time.sleep(0.01)
