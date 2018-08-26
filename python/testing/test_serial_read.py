import logging
import serial
import sys
import time

ser = serial.Serial(
    port='/dev/ttyUSB0',
    baudrate=9600#,
    #timeout=1
)


if __name__ == '__main__':
    while (True):
        if ser.in_waiting > 0:
            msg = ser.readline()
            print(msg)
            message = msg.encode('utf-8')

        time.sleep(0.01)
