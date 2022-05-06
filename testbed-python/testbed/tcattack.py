# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import sympy
import csv
import subprocess
import shlex
import time
import uuid
from . import logger
from .multiutils import GracefulKiller

tmp_file_path = '/tmp/delayTC-{uid}.txt'

scp_cmd = "scp -o ConnectTimeout=1 {tmpfile} {username}@{ip}:/home/delayTC.txt"


def resolve_row(internal_clock, csv_data):
    for row in range(1, len(csv_data)):
        if int(csv_data[row][0]) <= internal_clock < int(csv_data[row][1]):
            return row
    return False


def tc_attack(device, csv_file, clock):
    # 0: Start
    # 1: End
    # 2: Mode (func || fixed || incr)
    # 3: Arg1 (func || val || step)
    # 4: Arg2 (f_up || delay_resp)
    # 5: Arg3 (start value for inc)
    # 6: Arg4

    uid = str(uuid.uuid4())[:8]
    tmp_file_path_f = tmp_file_path.format_map({'uid': uid})

    vals = {
        'password': device.password,
        'username': device.username,
        'ip': device.ip,
        'tmpfile': tmp_file_path_f
    }

    scp_cmd_f = scp_cmd.format_map(vals)

    csv_data = []

    with open(csv_file, 'rt') as file:
        data = csv.reader(file)
        for i in data:
            csv_data.append(i)

    internal_clock = None
    prev_internal_clk = None
    with clock.get_lock():
        internal_clock = clock.value

    prev_incr_context = {
        'incr_mode': False,
        # 'prev_val': 0,
    }

    f_up = 0
    delay_resp = 0

    killer = GracefulKiller()

    while not killer.kill_now:
        current_row = resolve_row(internal_clock, csv_data)
        if not current_row:
            break

        # Put this in separate function when finished testing
        csv_data = []
        with open(csv_file, 'rt') as file:
            data = csv.reader(file)
            for i in data:
                csv_data.append(i)

        start = csv_data[current_row][0]
        end = csv_data[current_row][1]
        mode = csv_data[current_row][2]
        arg1 = csv_data[current_row][3]
        arg2 = csv_data[current_row][4]
        arg3 = csv_data[current_row][5]

        if str(mode) == 'func':
            x = sympy.symbols('x')
            expr_value = sympy.sympify(str(arg1)).evalf(subs={x: internal_clock})
            if str(arg2) == 'f_up':
                f_up = expr_value
            elif str(arg2) == 'delay_resp':
                delay_resp = expr_value
            else:
                logger.error(f"Invalid value: {arg2}")
            prev_incr_context['incr_mode'] = False
            pass

        elif mode == 'fixed':
            if str(arg2) == 'f_up':
                f_up = int(arg1)
            elif str(arg2) == 'delay_resp':
                delay_resp = int(arg1)
            else:
                logger.error(f"Invalid value: {arg2}")
            prev_incr_context['incr_mode'] = False
            pass

        elif mode == 'incr':
            if not prev_incr_context['incr_mode']:
                if str(arg2) == 'f_up':
                    f_up = int(arg3)
                elif str(arg2) == 'delay_resp':
                    delay_resp = int(arg3)
                else:
                    logger.error(f"Invalid value: {arg2}")
                prev_incr_context['incr_mode'] = True
            else:
                if str(arg2) == 'f_up':
                    f_up += int(arg1)
                elif str(arg2) == 'delay_resp':
                    delay_resp += int(arg1)
                else:
                    logger.error(f"Invalid value: {arg2}")
                prev_incr_context['incr_mode'] = True
            pass

        else:
            logger.error(f"Invalid mode: {mode}")

        with open(tmp_file_path_f, 'w+') as scratch_file:
            scratch_file.write(f"{int(f_up)} {int(delay_resp)}")
            logger.debug(f"Delay TC: {f_up} {delay_resp}")

        subprocess.run(shlex.split(scp_cmd_f), stdout=subprocess.DEVNULL)
        prev_internal_clk = internal_clock
        failsafe = time.time()
        while prev_internal_clk == internal_clock:
            time.sleep(0.7)
            if time.time() >= failsafe + 3:
                break
            with clock.get_lock():
                internal_clock = clock.value

    # Restore to 0
    with open(tmp_file_path_f, 'w+') as scratch_file:
        scratch_file.write(f"{0} {0}")
    subprocess.run(shlex.split(scp_cmd_f), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)