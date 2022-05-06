# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

from . import logger
import signal
from collections import defaultdict
import copy
from .executor import execute
from .multiutils import ProcessState

COMMAND_SSH_SKELETON = 'ssh -t -t -o "ConnectTimeout=1" {username}@{ip} "echo {password} | sudo -S {command}"'

COMMAND_SSH_SKELETON_NROOT = 'ssh -t -t -o "ConnectTimeout=1" {username}@{ip} "{command}"'


def process_creator(config):
    ps = []

    for k, v in config.items():
        for d in v["programs"]:
            p = Process(k, v["ip_addr"], v["user"], v["pass"], d, v['id'])
            ps.append(p)
    return ps


def refresh_all_states():
    for dev in Device.instances:
        for proc in dev.processes:
            proc.update_state()


class Device:
    instances = []

    def __new__(cls, *args, **kwargs):
        for i in cls.instances:
            if i.name == args[0]:
                return i
            elif 'name' in kwargs and kwargs['name'] == i.name:
                return i
        return super(Device, cls).__new__(cls)

    def __init__(self, name, ip, username, password, id):
        self.name = name
        self.ip = ip
        self.username = username
        self.password = password
        self.id = id
        if not hasattr(self, 'processes'):
            self.processes = []
        if self not in self.__class__.instances:
            self.__class__.instances.append(self)


class Process:
    def __init__(self, device_name, ip, username, password, pconfig, dev_id, add_list=True):
        self.pconfig = pconfig
        self.name = pconfig["name"] if "name" in pconfig else "N/A"
        self.outfile = pconfig["outfile"] if "outfile" in pconfig else None
        self.serial = pconfig["serial"] if "serial" in pconfig else None
        self.command = None
        self.state = ProcessState.NEW
        self.subprocess = None
        self.block = bool(pconfig["block"]) if "block" in pconfig else None
        if device_name:
            self.device = Device(device_name, ip, username, password, dev_id)
            if add_list:
                self.device.processes.append(self)

    def command_builder(self):
        current_subcmd = f'{self.pconfig["command"]} {" ".join(self.pconfig["args"])}'
        params_up = {'password': self.device.password, 'username': self.device.username, 'ip': self.device.ip,
                     'command': current_subcmd}
        if self.pconfig["run_root"]:
            current_cmd = COMMAND_SSH_SKELETON.format_map(params_up)
        else:
            current_cmd = COMMAND_SSH_SKELETON_NROOT.format_map(params_up)
        return current_cmd

    def run(self):
        if not self.command:
            self.command = self.command_builder()
        logger.debug(f'Running subprocess "{self.device.name}-{self.name}"')
        proc = execute(self.command, self.block)
        self.subprocess = proc
        self.state = ProcessState.RUNNING

    def terminate(self):
        if self.subprocess:
            logger.info(f'Terminating "{self.device.name}-{self.name}"')
            # self.subprocess.send_signal(signal.SIGINT)
            self.subprocess.terminate()

    def update_state(self):
        if not self.subprocess:
            # logger.warning(f'Subprocess "{self.device.name}-{self.name}" not instantiated yet')
            return
        if self.subprocess.poll() is not None:
            logger.debug(f'Subprocess "{self.device.name}-{self.name}" state updated to EXITED')
            logger.debug(f"Return Code {self.subprocess.returncode}")
            self.state = ProcessState.EXITED

    def print_state(self):
        return f'{self.device.name + "-" + self.name + ":":<70} {self.state:>20}'

#
#
#
# def device_splitter(config_yaml):
#     prepared = dict()
#     for k,v in config_yaml.items():
#         logger.debug(f"Preparing commands for device {k}")
#         commands = command_maker(v)
#         prepared[k] = commands
#     return prepared
#
#
# def command_maker(device_config):
#     params = {
#         'password': device_config['pass'],
#         'username': device_config['user'],
#         'ip': device_config['ip_addr']
#     }
#     commands = []
#     for d in device_config['programs']:
#         current_subcmd = f'{d["command"]} {" ".join(d["args"])}'
#         params_up = copy.deepcopy(params)
#         params_up['command'] = current_subcmd
#         if d["run_root"] == True:
#             current_cmd = COMMAND_SSH_SKELETON.format_map(params_up)
#         else:
#             current_cmd = COMMAND_SSH_SKELETON_NROOT.format_map(params_up)
#         logger.debug(f"Built command: {current_cmd}")
#         if "name" in d:
#             name = d["name"]
#         else:
#             name = "N/A"
#         commands.append([name, current_cmd])
#     return commands
