from __future__ import print_function
import os
from subprocess import Popen, PIPE
import sys
import _thread
from threading import Thread
from flask import *
from flask_socketio import SocketIO, emit, disconnect
	
app = Flask(__name__)
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app, async_mode='threading')

#deprecated, use generate_images_array instead
def generate_images_data():
	names = os.listdir(os.path.join(app.static_folder, 'cam_images'))
	names.sort()
	return names[0],names[1],names[2],names[3],names[4],names[5],names[6],names[7]

def generate_images_array():
	if os.path.isdir(os.path.join(app.static_folder, 'cam_images')):
		names = os.listdir(os.path.join(app.static_folder, 'cam_images'))
		names.sort()
		return names
	else:
		return []

def render_images_template(tmpl_name, **kwargs):
	images = generate_images_array()
	if len(images) > 0:
		return render_template(tmpl_name, im1=images[0], im2=images[1], im3=images[2], im4=images[3], im5=images[4], im6=images[5], im7=images[6], im8=images[7], **kwargs)
	else:
		return render_template(tmpl_name, im1="", im2="", im3="", im4="", im5="", im6="", im7="", im8="", **kwargs)

def prepare_download_all(data):
	cmd = ["/home/pi/cam-new/prepare_download.sh"]	
	with Popen(cmd, stdout=PIPE, bufsize=1, universal_newlines=True) as p:
			sys.stdout.flush()
			count = 0
			for line in p.stdout:
				if 'OK\n' == line:
					socketio.emit('download_data',{'data': "OK", 'count': count},namespace='/test')
				else:
					socketio.emit('my response',{'data': line, 'count': count},namespace='/test')
				count += 1
				print(line, end='')
				
def run_capture(image_count):
#def run_capture():
	#cmd = ["/home/pi/cam-new/base_unit"]
	cmd = ["/home/pi/cam-new/base_unit", "-c", image_count]
	with Popen(cmd, stdout=PIPE, bufsize=1, universal_newlines=True) as p:
		sys.stdout.flush()
		count = 0
		for line in p.stdout:
			if 'OK\n' == line:
				images = generate_images_array()
				socketio.emit('image_data',{'data': "|".join(images), 'count': count},namespace='/test')
			else:
				socketio.emit('my response',{'data': line, 'count': count},namespace='/test')
			count += 1
			print(line, end='')

@app.route('/', methods=['GET', 'POST'])
def index():
	return render_images_template('./dash.html')

@app.route('/config', methods=['GET', 'POST'])
def config():
	SITE_ROOT = os.path.realpath(os.path.dirname(__file__))
	json_url = os.path.join(SITE_ROOT, "config.json")
	
	if request.method == 'POST':
		f = open(json_url, 'w')
		print(json.dumps(request.get_json(), indent=4))
		json.dump(request.get_json(), f)
		f.close()
		
	fr = open(json_url)
	data = json.load(fr)
	fr.close();
	return jsonify(data)

@socketio.on('my event', namespace='/test')
def test_message(message):
	session['receive_count'] = session.get('receive_count', 0) + 1
	emit('my response',{'data': message['data'], 'count': session['receive_count']})

@socketio.on('capture_event', namespace='/test')
def capture_images(message):
	session['receive_count'] = session.get('receive_count', 0) + 1
	_thread.start_new_thread(run_capture, (message['data'],))

@socketio.on('download_images', namespace='/test')
def capture_images(message):
	session['receive_count'] = session.get('receive_count', 0) + 1
	_thread.start_new_thread(prepare_download_all, (message['data'],))

@socketio.on('disconnect request', namespace='/test')
def disconnect_request():
	session['receive_count'] = session.get('receive_count', 0) + 1
	emit('my response',{'data': 'Disconnected!', 'count': session['receive_count']})
	disconnect()

@socketio.on('connect', namespace='/test')
def test_connect():
	emit('my response', {'data': 'Connected!!!', 'count': 0})


@socketio.on('disconnect', namespace='/test')
def test_disconnect():
	print('Client disconnected', request.sid)


if __name__ == '__main__':
	socketio.run(app,debug=True, host='0.0.0.0')