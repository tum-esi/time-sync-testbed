# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

from sshtail import SSHTailer
import time
import os
from .multiutils import GracefulKiller
from . import logger


def ssh_to_file(username, ip, rfile, lfile):
    killer = GracefulKiller()
    tailer = SSHTailer(f'{username}@{ip}', rfile)

    while not killer.kill_now:
        with open(lfile, 'a+') as f:
            for line in tailer.tail():
                logger.debug(f"SSH RX: {line}")
                f.write(line + '\n')

        time.sleep(0.7)

    # EXIT TASKS HERE
    logger.info(f"Killing SSH logger for IP: {ip} File: {rfile}")
    tailer.disconnect()
    os.remove(lfile)
