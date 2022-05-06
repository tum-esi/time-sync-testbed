# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import serial
import time
import os
from .multiutils import GracefulKiller
from . import logger


def serial_to_file(serial_port, baud, file):
    killer = GracefulKiller()
    serial_obj = serial.Serial(serial_port, baud, timeout=5)
    while not killer.kill_now:
        with open(file, 'ab+') as f:
            received_string = serial_obj.readline()
            logger.debug(f"Serial RX: {received_string}")
            f.write(received_string)

        time.sleep(0.7)

    # EXIT TASKS HERE
    logger.info(f"Killing serial logger on port {serial_port}")
    os.remove(file)
    serial_obj.flushInput()
    serial_obj.flushOutput()
    serial_obj.close()
