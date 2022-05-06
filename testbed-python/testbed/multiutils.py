# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import signal
from enum import Enum


class ProcessState(Enum):
    NEW = 0
    RUNNING = 1
    EXITED = 2

    def __str__(self):
        return str(self.name)


class GracefulKiller:
    kill_now = False

    def __init__(self):
        signal.signal(signal.SIGINT, self.exit_gracefully)
        signal.signal(signal.SIGTERM, self.exit_gracefully)

    def exit_gracefully(self, *args):
        self.kill_now = True
