# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import sympy
import csv
import subprocess
import shlex
import time
import uuid
import hashlib
from . import logger
from .multiutils import GracefulKiller

tmp_file_path = '/tmp/delay-{uid}.txt'

scp_cmd = "scp -o ConnectTimeout=1 {tmpfile} {username}@{ip}:/home/delay.txt"


def resolve_row_delay(internal_clock, csv_data):
    for row in range(1, len(csv_data)):
        if int(csv_data[row][0]) <= internal_clock < int(csv_data[row][1]):
            return row
    return False


def delay_attack(device, csv_file, clock):
    # 0: Start
    # 1: End
    # 2: Mode (func || fixed || incr)
    # 3: Arg1 (func || val || step)
    # 4: Arg2 (s_m || m_s)
    # 5: Arg3 (start value for inc)
    # 6: Arg4 (a || s || p) for ESI-1 enp3s0->enp2s0 direction (all, sync, peer messages only relevant for L2 PTP)
    # 7: Arg5 (a || s || p) for ESI-1 enp2s0->enp3s0 direction (all, sync, peer messages only relevant for L2 PTP)

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

    prev_hash = 0

    s_m = 0
    m_s = 0

    killer = GracefulKiller()

    while not killer.kill_now:
        current_row = resolve_row_delay(internal_clock, csv_data)
        if not current_row:
            break

        # Put this in separate function when finished testing
        csv_data = []
        with open(csv_file, 'rt') as file:
            data = csv.reader(file)
            for i in data:
                csv_data.append(i)

        s_m_selector = 'a'
        m_s_selector = 'a'

        start = csv_data[current_row][0]
        end = csv_data[current_row][1]
        mode = csv_data[current_row][2]
        arg1 = csv_data[current_row][3]
        arg2 = csv_data[current_row][4]
        arg3 = csv_data[current_row][5]
        arg4 = csv_data[current_row][6]
        arg5 = csv_data[current_row][7]

        if str(mode) == 'func':
            x = sympy.symbols('x')
            expr_value = sympy.sympify(str(arg1)).evalf(subs={x: internal_clock})
            if str(arg2) == 's_m':
                s_m = expr_value
            elif str(arg2) == 'm_s':
                m_s = expr_value
            else:
                logger.error(f"Invalid value: {arg2}")
            prev_incr_context['incr_mode'] = False
            pass

        elif mode == 'fixed':
            if str(arg2) == 's_m':
                s_m = int(arg1)
            elif str(arg2) == 'm_s':
                m_s = int(arg1)
            else:
                logger.error(f"Invalid value: {arg2}")
            prev_incr_context['incr_mode'] = False
            pass

        elif mode == 'incr':
            logger.info('in incr mode')
            if not prev_incr_context['incr_mode']:
                logger.info('new context')
                if str(arg2) == 's_m':
                    s_m = int(arg3)
                elif str(arg2) == 'm_s':
                    m_s = int(arg3)
                else:
                    logger.error(f"Invalid value: {arg2}")
                prev_incr_context['incr_mode'] = True
            else:
                logger.info('existing context')
                if str(arg2) == 's_m':
                    s_m += float(arg1)
                elif str(arg2) == 'm_s':
                    m_s += float(arg1)
                else:
                    logger.error(f"Invalid value: {arg2}")
                prev_incr_context['incr_mode'] = True
            logger.info(f'{s_m} {m_s}')
            pass

        else:
            logger.error(f"Invalid mode: {mode}")

        if arg4 == 'p' or arg4 == 's' or arg4 == 'a':
            s_m_selector = arg4
        else:
            s_m_selector = 'a'

        if arg5 == 'p' or arg5 == 's' or arg5 == 'a':
            m_s_selector = arg5
        else:
            m_s_selector = 'a'

        logger.info(f"Delay1: {s_m} {m_s}")
        raw_data_str = f"{int(s_m)} {int(m_s)} {s_m_selector} {m_s_selector}"
        data_hash_obj = hashlib.md5(raw_data_str.encode())
        data_hash = data_hash_obj.hexdigest()
        logger.info(data_hash)

        if data_hash != prev_hash:
            prev_hash = data_hash
            with open(tmp_file_path_f, 'w+') as scratch_file:
                scratch_file.write(f"{int(s_m)} {int(m_s)} {s_m_selector} {m_s_selector}")
                logger.debug(f"Delay: {s_m} {m_s}")
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
        scratch_file.write(f"{0} {0} a a")
    subprocess.run(shlex.split(scp_cmd_f), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)