# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import time

from .rspost import start, plot
from .parser import parse_yaml
from . import logger
import multiprocessing


main_clock = multiprocessing.Value('i', 0)


def init_plotters(plotter_yaml):
    plotter_config = parse_yaml(plotter_yaml)
    plotters = []
    for k, v in plotter_config.items():
        plotter = Plotter(v['name'], v['type'], v)
        plotters.append(plotter)
    return plotters


class Plotter:
    def __init__(self, name, type, dict):
        self.name = name
        self.type = type
        self.dproc = None
        self.pproc = None
        if self.type == 'rsplot':
            self.ip = dict['ip']
            self.title = dict['title'] if 'title' in dict else None
            self.lines = dict['lines']
            self.dplotfunc = start
            self.pplotfunc = plot
            self.manager = multiprocessing.Manager()
            self.data = self.manager.dict()
            self.lock = multiprocessing.Lock()

    def start(self):
        if self.type == 'rsplot':
            if not self.dproc:
                proc = multiprocessing.Process(target=self.dplotfunc, args=tuple([self.ip, self.lines, main_clock, self.data,
                                                                                  self.manager, self.lock]))
                proc.daemon = True
                proc.start()
                self.dproc = proc
                time.sleep(5)
                proc = multiprocessing.Process(target=self.pplotfunc, args=tuple([self.data, self.lines, self.lock, self.title]))
                proc.daemon = True
                proc.start()
                self.pproc = proc

            else:
                logger.warning(f"Plotter {self.name} is already running!")

    def terminate(self):
        if self.dproc:
            self.dproc.terminate()
            self.dproc = None
            self.pproc.terminate()
            self.pproc = None
        else:
            logger.warning(f"Plotter {self.name} is already terminated!")




