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
            msg = ser.read()
            print(msg)

        time.sleep(0.01)
