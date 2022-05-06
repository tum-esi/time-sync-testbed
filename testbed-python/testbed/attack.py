# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import multiprocessing
from .tcattack import tc_attack
from .delayattack import delay_attack
from .floodattack import flood_attack
from . import logger
from .parser import parse_yaml
from .commandmaker import Device
from .plotter import main_clock


def init_attacks(attack_yaml_file):
    attacks = []
    attack_config = parse_yaml(attack_yaml_file)

    for k, v in attack_config.items():
        found = 0
        for dev in Device.instances:
            if dev.id == v['device']:
                attack = Attack(v['name'], dev, v['type'], v['csv'])
                attacks.append(attack)
                found = 1
                break
        if not found:
            logger.warning(f"Device {v['device']} does not exist for attack {v['name']}")

    return attacks


class Attack:
    def __init__(self, name, device, type, csv):
        self.name = name
        self.type = type
        self.device = device
        self.csv = csv
        self.proc = None
        if self.type == 'tc_corr':
            self.function = tc_attack
        elif self.type == 'delay_attack':
            self.function = delay_attack
        elif self.type == 'packet_flood':
            self.function = flood_attack

    def start(self):
        if not self.proc:
            proc = multiprocessing.Process(target=self.function, args=tuple([self.device, self.csv,
                                                                             main_clock]))
            proc.daemon = True
            proc.start()
            self.proc = proc
            return proc
        else:
            logger.warning(f"Attack {self.name} is already running!")

    def terminate(self):
        if self.proc:
            self.proc.terminate()
            self.proc = None
        else:
            logger.warning(f"Attack {self.name} is already terminated!")
