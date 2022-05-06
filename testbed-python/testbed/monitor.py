# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import multiprocessing
from .sshtools import ssh_to_file
from .serialttytools import serial_to_file
from .commandmaker import *
from .parser import parse_yaml
from . multiutils import ProcessState


def init_monitor_procs(monitor_yaml_file):
    mon_processes = []
    mon_config = parse_yaml(monitor_yaml_file)

    for k, v in mon_config.items():
        if v['script'] == 'ptp4l-logdump-ssh':
            found = 0
            for dev in Device.instances:
                if dev.id == v['device']:
                    mproc = multiprocessing.Process(target=ssh_to_file,
                                                    args=tuple([dev.username,
                                                                dev.ip,
                                                                v['rfile'],
                                                                v['outfile']]))
                    mproc.daemon = True
                    mon_processes.append(Monitor(v['name'], mproc, v['outfile']))
                    found = 1
                    break
            if not found:
                logger.warning(f"Device {v['device']} does not exist for monitor processes {v['name']}")

        elif v['script'] == 'serial-logdump':
            mproc = multiprocessing.Process(target=serial_to_file,
                                            args=tuple([v['serial'], int(v['baud']), v['outfile']]))
            mproc.daemon = True
            mon_processes.append(Monitor(v['name'], mproc, v['outfile']))

    return mon_processes


class Monitor:
    def __init__(self, name, mproc, lfile):
        self.name = name
        self.mproc = mproc
        self.state = ProcessState.NEW
        self.lfile = lfile

    def run(self):
        if not self.mproc.is_alive():
            self.mproc.start()
            self.state = ProcessState.RUNNING
            logger.info(f'Monitor {self.name} is now RUNNING')
        else:
            logger.warning(f'Monitor {self.name} is already running!')

    def terminate(self):
        if self.mproc.is_alive():
            self.mproc.terminate()
            self.state = ProcessState.EXITED
            logger.info(f'Monitor {self.name} is now EXITED')
        else:
            logger.warning(f'Monitor {self.name} is not running!')

    def print_state(self):
        return f'{self.name + ":":<70} {self.state:>20}'

    def update_state(self):
        if self.state == ProcessState.NEW:
            pass
        elif not self.mproc.is_alive() and self.state == ProcessState.RUNNING:
            self.state = ProcessState.EXITED
            logger.info(f'Monitor {self.name} state updated to EXITED')
        elif self.mproc.is_alive() and self.state == ProcessState.EXITED:
            self.state = ProcessState.RUNNING
            logger.info(f'Monitor {self.name} state updated to RUNNING')









# def resolve_mon_script(script, proc):
#     if script == 'ptp4l-logdump-ssh':
#         script_cmd = f'./ptp4l-logdump-ssh.sh {proc.device.ip} {proc.outfile}'
#         proc.command = script_cmd
#     elif script == 'serial-logdump':
#         script_cmd = f'./serialToFile.sh {proc.serial} {proc.outfile}'
#         proc.command = script_cmd
#     pass
