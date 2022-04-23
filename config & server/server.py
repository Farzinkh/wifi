import flask
import logging
import os
import sqlite3
from python_arptable import get_arp_table

UPLOAD_FOLDER = ''
ALLOWED_EXTENSIONS = {'txt', 'bin'}

app = flask.Flask(__name__)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
database = sqlite3.connect('Devices.db',check_same_thread=False)
c = database.cursor()
c.execute("CREATE TABLE IF NOT EXISTS Devices (MAC TEXT,IP TEXT, Running_version TEXT,Last_version TEXT)")
database.commit()

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
    
def save_intodb(pv,data):
    if(pv is None):
        c.execute("INSERT INTO Devices VALUES(?,?,?,?)",[data['MAC'],data['IP'],data['Running_version'],data['Last_version']])
    else:    
        c.execute('UPDATE Devices SET Running_version=? where MAC =?',(data['Running_version'],data['MAC']))
        c.execute('UPDATE Devices SET Last_version=? where MAC =?',(data['Last_version'],data['MAC']))
    database.commit()
    
def load_fromdb(mac):  
    c.execute("SELECT * FROM Devices WHERE MAC=?", [mac])
    result = c.fetchall()
    if not result:
        return None
    return result[0][2]
         
def mac_from_ip(ip):    
    for l in get_arp_table():
        if l['IP address']==ip:
            return l['HW address']
    return None        
    
@app.route("/", methods=['GET', 'POST'])
def hello():
    rverion=flask.request.data
    rverion=rverion.decode('UTF-8')[2:].replace('\x00','').replace(' ', '')
    ip=flask.request.remote_addr
    mac=mac_from_ip(ip)
    pversion=load_fromdb(mac)
    file1 = open('sdkconfig', 'r')
    Lines = file1.readlines()
    for line in Lines:
        if line[0]=='#':
            continue
        if line[0:23]=="CONFIG_APP_PROJECT_VER=":
            version=line[23:].replace('"', '').rstrip()
    print("Device:\n\tIP:",ip,"\n\tMAC:",mac,"\n\tRunning version:",rverion,"\n\tLast version:",version,"\n\tPrevious version:",pversion) 
    data = {'MAC':mac,'IP': ip, 'Running_version': rverion, 'Last_version': version}
    save_intodb(pversion,data)
    response = flask.make_response(version, 200)
    response.mimetype = "text/plain"
    return response        

@app.route("/build/<path:filename>", methods=['GET', 'POST'])
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
