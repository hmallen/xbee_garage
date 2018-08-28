import logging

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


if __name__ == '__main__':
    test_var = '&U%doorOpen$1%lockStateDoor$0%lockStateButton$0%doorAlarm$1'

    start_char = test_var[0]

    if start_char == '&':
        updates = [(var.split('$')[0], var.split('$')[1]) for var in test_var[3:].split('%')]
        logger.debug('updates: ' + str(updates))
