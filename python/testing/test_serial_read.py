import logging
import serial
import sys

ser = serial.Serial(
    port='/dev/ttyUSB0',
    baudrate=9600#,
    #timeout=1
)


if __name__ == '__main__':
    while (True):
        if ser.in_waiting > 0:
            msg = ser.read()
