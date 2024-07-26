# Web service to serve the current time as "dd/mm/yyyy hh:mm:ss"

from flask import Flask
from flask_cors import CORS
from datetime import datetime

app = Flask(__name__)
CORS(app)

@app.route('/get_time', methods=['GET'])
def time():
    try:
        now = datetime.now()
        return now.strftime("%d/%m/%Y %H:%M:%S"), 200
    except Exception as e:
        return str(e), 500

if __name__ == '__main__':
    app.run(host='localhost', port=5000)
