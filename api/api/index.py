from flask import Flask, jsonify, request
app = Flask(__name__)


@app.route('/')
def home():
    return 'All hail Gene Sundeen!'

@app.route('/sensors')
def list_sensors():
    sensors = [
        'door',
        'carbon-monoxide',
        'air-quality'
    ]
    return jsonify(sensors)

@app.route('/locks')
def list_locks():
    locks = [
        'door',
        'button'
    ]
    return jsonify(locks)
