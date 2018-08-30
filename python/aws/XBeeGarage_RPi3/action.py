import RPi.GPIO as GPIO
import time
import platform
import logging


def pulse(secs):
    if platform.system().lower() != "windows":
        GPIO.setmode(GPIO.BOARD)

    Motor1A = 12
    Motor1B = 36

    GPIO.setup(Motor1A,GPIO.OUT)
    GPIO.setup(Motor1B,GPIO.OUT)

    logging.info("Turning motor on")
    GPIO.output(Motor1A,GPIO.HIGH)
    GPIO.output(Motor1B,GPIO.LOW)

    time.sleep(secs)

    logging.info("Stopping motor")
    GPIO.output(Motor1A,GPIO.LOW)

    GPIO.cleanup()
