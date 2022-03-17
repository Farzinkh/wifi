from tkinter import EXCEPTION
import flask
import logging
import os 

UPLOAD_FOLDER = ''
ALLOWED_EXTENSIONS = {'txt', 'bin'}

app = flask.Flask(__name__)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

logging.basicConfig(level=logging.INFO, format=f'%(asctime)s %(levelname)s %(name)s %(threadName)s : %(message)s')

errors = flask.Blueprint('errors', __name__)
@errors.app_errorhandler(Exception)
def handle_unexpected_error(error):
    app.logger.warning("No update is available!")
    status_code = 500
    success = False
    response = {
        'success': success,
        'error': {
            'type': 'UnexpectedException',
            'message': 'An unexpected error has occurred.'
        }
    }

    return flask.jsonify(response), status_code
    
@app.route("/")
def hello():
    file1 = open('sdkconfig', 'r')
    Lines = file1.readlines()
    for line in Lines:
        if line[0]=='#':
            continue
        if line[0:23]=="CONFIG_APP_PROJECT_VER=":
            version=line[23:].strip()
            print(version)
    response = flask.make_response(version, 200)
    response.mimetype = "text/plain"
    return response        

@app.route("/build/<path:filename>")
def downloader(filename):
    if not flask.request.is_secure:
        app.logger.warning("Not secure!!!")
        url = flask.request.url.replace('http://', 'https://', 1)
        code = 301
        return flask.redirect(url, code=code)
    app.logger.info(flask.request.headers)    
    uploads = os.path.join(flask.current_app.root_path+'/build/', app.config['UPLOAD_FOLDER'])
    return flask.send_from_directory(directory=uploads,path=filename)

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=8070,ssl_context=('server_certs/ca_cert.pem', 'server_certs/ca_key.pem'))