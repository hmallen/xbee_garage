import configparser
from flask import Flask, jsonify, request

from pymongo import MongoClient

app = Flask(__name__)

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

config_path = '../config/config.ini'

config = configparser.configparser()
config.read(config_path)

mongo_uri = config['mongodb']['uri']
mongo_db = config['mongodb']['database']

db = MongoClient(mongo_uri)[mongo_db]

collections = {
    'state': config['mongodb']['collection_state'],
    'log': config['mongodb']['collection_log']
}

"""
sensors = {
    'door': 'open',
    'carbon-monoxide': 25,
    'air-quality': 67
}

locks = {
    'door': False,
    'button': False
}
"""


@app.route('/')
def home():
    return 'All hail Gene Sundeen!'


@app.route('/sensors')
def list_sensors():
    """
    sensors = [
        'door',
        'carbon-monoxide',
        'air-quality'
    ]
    """
    sensors = db[collections['state']].find_one({'_id': 'sensors'})
    return jsonify(sensors)


@app.route('/locks')
def get_locks():
    """
    locks = [
        'door',
        'button'
    ]
    """
    locks = db[collections['state']].find_one({'_id': 'locks'})
    return jsonify(locks)


@app.route('/locks', methods=['POST'])
def set_locks():
    lock_request = request.get_json()
    logger.debug('lock_request: ' + str(lock_request))

    locks = db[collections['state']].find_one({'_id': 'locks'})

    for lock in lock_request:
        logger.debug('Setting ' + lock + ' to ' + lock_request[lock] + '.')
        locks[lock] = lock_request[lock]

    updated_result = db[collections['state']].update_one({'_id': 'locks'}, {'$set': locks})
    logger.debug('updated_result.modified_count: ' + str(updated_result.modified_count))

    return '', 204


@app.route('/action', methods=['POST'])
def trigger_action():
    pass
