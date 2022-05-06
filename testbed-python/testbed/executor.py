# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

from . import logger
import subprocess
import shlex
import time


def execute(cmd, block):
    res = subprocess.Popen(shlex.split(cmd),
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)
    logger.debug(f"Running command: {cmd}")
    logger.debug(f"PID: {res.pid}")
    if block:
        res.wait()
    time.sleep(1)
    return res


