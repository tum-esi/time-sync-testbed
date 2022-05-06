from sshtail import SSHTailer
import time
import os
import signal

class GracefulKiller:
    kill_now = False

    def __init__(self):
        signal.signal(signal.SIGINT, self.exit_gracefully)
        signal.signal(signal.SIGTERM, self.exit_gracefully)

    def exit_gracefully(self, *args):
        self.kill_now = True


def ssh_to_file(username, ip, rfile, lfile):
    killer = GracefulKiller()
    tailer = SSHTailer(f'{username}@{ip}', rfile)

    while not killer.kill_now:
        with open(lfile, 'a+') as f:
            for line in tailer.tail():
                print(f"SSH RX: {line}")
                f.write(line + '\n')

        time.sleep(0.7)

    # EXIT TASKS HERE
    tailer.disconnect()
    os.remove(lfile)


ssh_to_file('debian', '192.168.8.2', '/var/log/messages','/home/testbed-controller/Desktop/testbed-python/BBBslave.log')

