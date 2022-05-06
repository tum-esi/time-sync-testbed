# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import sympy
import csv
import subprocess
import shlex
import time
import os
import hashlib
import uuid
import re
from . import logger
from .multiutils import GracefulKiller
from .commandmaker import Process
from .parser import parse_yaml


# so = ' '.join(so[i:i+2] for i in range(0, len(so), 2))

def resolve_row_delay(internal_clock, csv_data):
    for row in range(1, len(csv_data)):
        if int(csv_data[row][0]) <= internal_clock < int(csv_data[row][1]):
            return row
    return False


def flood_attack(device, csv_file, clock):
    # 0: Start
    # 1: End
    # 2: Interface
    # 3: Src_mac
    # 4: Dst_mac
    # 5: Src_ip
    # 6: Dst_ip
    # 7: Src_port
    # 8: Dst_port
    # 9: Transport
    # 10: Interval
    # 11: Payload
    # 12: Min_payload
    # 13: Max_payload
    # 14: Threads

    macs_dict = {}
    macs_yaml = 'configs/MACS.yaml'
    if os.path.exists(macs_yaml):
        macs_dict = parse_yaml(macs_yaml)
        
    ips_dict = {}
    ips_yaml = 'configs/IPS.yaml'
    if os.path.exists(ips_yaml):
        ips_dict = parse_yaml(ips_yaml)

    csv_data = []

    with open(csv_file, 'rt') as file:
        data = csv.reader(file)
        for i in data:
            csv_data.append(i)

    internal_clock = None
    prev_internal_clk = None
    with clock.get_lock():
        internal_clock = clock.value

    curr_context = {
        'proc': None,
        'hash': None,
    }

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

        start = csv_data[current_row][0]
        end = csv_data[current_row][1]
        interface = csv_data[current_row][2]
        src_mac = csv_data[current_row][3]
        dst_mac = csv_data[current_row][4]
        src_ip = csv_data[current_row][5]
        dst_ip = csv_data[current_row][6]
        src_port = csv_data[current_row][7]
        dst_port = csv_data[current_row][8]
        transport = csv_data[current_row][9]
        interval = csv_data[current_row][10]
        payload = csv_data[current_row][11]
        min_payload = csv_data[current_row][12]
        max_payload = csv_data[current_row][13]
        threads = csv_data[current_row][14]

        raw_data_str = ''
        for i in range(2, 15):
            raw_data_str += csv_data[current_row][i]

        data_hash_obj = hashlib.md5(raw_data_str.encode())
        data_hash = data_hash_obj.hexdigest()

        # Terminate and clean expired process
        if not curr_context['hash'] or curr_context['hash'] != data_hash:
            if curr_context['proc']:
                curr_context['proc'].terminate()
                curr_context['proc'] = None

            # Split payload HEX stream to byte pairs for compatibility
            payload = ' '.join(payload[i:i+2] for i in range(0, len(payload), 2))

            # Resolve src and/or dst MAC and IP address fields if in the form of <dev>:<interface> as per MACS.yaml
            src_mac_re = re.search('(.+):(.+)', src_mac)
            if src_mac_re:
                dev_id = src_mac_re.group(1)
                if_id = src_mac_re.group(2)
                try:
                    src_mac = macs_dict[dev_id][if_id]
                except Exception as e:
                    logger.error(e)
                    src_mac = "0"
            dst_mac_re = re.search('(.+):(.+)', dst_mac)
            if dst_mac_re:
                dev_id = dst_mac_re.group(1)
                if_id = dst_mac_re.group(2)
                try:
                    dst_mac = macs_dict[dev_id][if_id]
                except Exception as e:
                    logger.error(e)
                    dst_mac = "0"
            src_ip_re = re.search('(.+):(.+)', src_ip)
            if src_ip_re:
                dev_id = src_ip_re.group(1)
                if_id = src_ip_re.group(2)
                try:
                    src_ip = ips_dict[dev_id][if_id]
                except Exception as e:
                    logger.error(e)
                    src_ip = ""
            dst_ip_re = re.search('(.+):(.+)', dst_ip)
            if dst_ip_re:
                dev_id = dst_ip_re.group(1)
                if_id = dst_ip_re.group(2)
                try:
                    dst_ip = ips_dict[dev_id][if_id]
                except Exception as e:
                    logger.error(e)
                    dst_ip = ""

            args = [
                f'-i {interface}',
                f'-d {dst_ip}',
                f'-p {dst_port if dst_port else "0"}',
                f'--sport {src_port if src_port else "0"}',
                f'--interval {interval if interval else "1000000"}',
                f'--threads {threads if threads else "1"}',
                f'--smac {src_mac if src_mac else "0"}',
                f'--dmac {dst_mac if dst_mac else "0"}',
                '--nostats',
            ]

            if src_ip:
                args.append(f'-s {src_ip}')

            if payload:
                args.append(f"--payload '{payload}'")
            else:
                args.append(f'--min {min_payload}')
                args.append(f'--max {max_payload}')

            if transport.upper() == 'TCP':
                args.append('--tcp')
            elif transport.upper() == 'ICMP':
                args.append('--icmp')
                args.append('--icmptype 8')

            pconfig = {
                'command': 'flood',
                'run_root': True,
                'args': args,
            }

            curr_proc = Process(device.name, device.ip, device.username, device.password, pconfig, device.id, False)
            curr_proc.run()

            curr_context['proc'] = curr_proc
            curr_context['hash'] = data_hash

        else:
            pass

        prev_internal_clk = internal_clock
        failsafe = time.time()
        while prev_internal_clk == internal_clock:
            time.sleep(0.7)
            if time.time() >= failsafe + 3:
                break
            with clock.get_lock():
                internal_clock = clock.value

    # Exit functions
    # Restore to 0
    logger.info("Exiting flooding attack")
    curr_context['proc'].terminate()
    curr_context['proc'] = None
