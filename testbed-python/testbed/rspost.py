# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import multiprocessing
from cProfile import label
from . import logger
from .parser import parse_yaml
import requests
import json
import time
import signal
from numpy import genfromtxt
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import os
import re
import math
import uuid
import sys
import copy
from tkinter import filedialog
import shutil
from .multiutils import GracefulKiller
import pprint

# url = "http://129.187.151.100/scpi_response.txt"

url = "http://{ip}/scpi_response.txt"

headers = {'Host': '{ip}',  #
           'User-Agent': 'Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:95.0) Gecko/20100101 Firefox/95.0',
           'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8',
           'Accept-Language': 'en-US,en;q=0.5',
           'Accept-Encoding': 'gzip, deflate',
           'Content-Type': 'application/x-www-form-urlencoded',
           'Content-Length': '{c_len}',
           'Origin': 'http://{ip}',  #
           'Connection': 'keep-alive',
           'Referer': 'http://{ip}/scpictrl.htm',  #
           'Upgrade-Insecure-Requests': '1'}

def save_file_as(title):
    # If there is no file path specified, prompt the user with a dialog which
    # allows him/her to select where they want to save the file
    file_path = filedialog.asksaveasfilename(
        filetypes=(
            ("CSV files", "*.csv"),
            ("All files", "*.*"),
        ),
        title=title,
    )
    return file_path


def oscilloRequest(ip, type, m_id, channel=None, ref=None):
    url_f = url.format_map({'ip': ip})
    headers_f = copy.deepcopy(headers)
    headers_f['Host'] = headers_f['Host'].format_map({'ip': ip})
    headers_f['Origin'] = headers_f['Origin'].format_map({'ip': ip})
    headers_f['Referer'] = headers_f['Referer'].format_map({'ip': ip})

    data = ''

    if type == 'delay':
        delay_cmd = f'password=&request=MEASurement{m_id}%3ARESult%3AACTual%3F+DELay&cmd=Send'
        headers_f['Content-Length'] = headers_f['Content-Length'].format_map({'c_len': len(delay_cmd)})
        data = delay_cmd
    elif type == 'mset':
        mset_cmd = f'password=&request=MEASurement{m_id}%3ASOURce+{channel}%2C{ref}&cmd=Send'
        headers_f['Content-Length'] = headers_f['Content-Length'].format_map({'c_len': len(mset_cmd)})
        data = mset_cmd

    res = requests.post(url_f, data=data, headers=headers_f)
    return res.text.strip()


def start(ip, lines, clock: multiprocessing.Value, data, manager, lock):
    count = 1
    with clock.get_lock():
        clock.value = 1

    oscillo = parse_yaml("configs/OSCILLO.yaml")

    # Set oscilloscope measurement settings
    for line in lines:
        if line['type'] == 'oscillo':
            real_channel = oscillo[line['channel']]
            real_ref = oscillo[line['reference']]
            oscilloRequest(ip, 'mset', line['measurement'], real_channel, real_ref)

    data['x_data'] = manager.list()
    for i in range(len(lines)):
        data[i] = manager.list()

    internal_data = dict()
    internal_data['x_data'] = []
    for i in range(len(lines)):
        internal_data[i] = {
            'data': None,
            'prev': 0
        }

    delay = 0
    lastD = 0
    uid = str(uuid.uuid4())[:8]
    with open(f'/tmp/delays-{uid}.csv', 'w') as f:
        for line in lines:
            f.write(line['name'] + ',')
        f.write('\n')

        killer = GracefulKiller()

        while not killer.kill_now:
            # Time
            timeStartMs = int(time.time() * 1000)

            # Time x-axis
            internal_data['x_data'].append(count)

            # Loop and process each line
            for index, line in enumerate(lines):

                # Measurement line
                if line['type'] == 'oscillo':
                    delay = oscilloRequest(ip, 'delay', int(line['measurement']))
                    if 'E+37' in delay:
                        delay = internal_data[index]['prev']
                    internal_data[index]['prev'] = delay
                    internal_data[index]['data'] = float(delay)
                    logger.debug(f"{line['name']}: {delay}")
                    f.write(str(float(delay)) + ',')

                # PTP4L Offset value
                elif line['type'] == 'log-ptp4l-offset':
                    while 1:
                        with open(line['logfile'], 'rb') as f2:
                            f2.seek(-2, os.SEEK_END)
                            while f2.read(1) != b'\n':
                                f2.seek(-2, os.SEEK_CUR)

                            last_line = f2.readline().decode()
                            line_search = re.search('.+ master offset\s+(-*\d+)\s.+ path delay\s+(-*\d+)',
                                                    last_line.strip())
                            if line_search:
                                calcOffset = float(line_search.group(1)) * math.pow(10, -9)
                                # reportedDelay = float(line_search.group(2)) * 10 ** (-9)
                                internal_data[index]['data'] = calcOffset
                                logger.debug(f"{line['name']}: {calcOffset}")
                                f.write(str(float(calcOffset)) + ',')
                                break

                # PTP4L Delay value
                elif line['type'] == 'log-ptp4l-delay':
                    while 1:
                        with open(line['logfile'], 'rb') as f2:
                            f2.seek(-2, os.SEEK_END)
                            while f2.read(1) != b'\n':
                                f2.seek(-2, os.SEEK_CUR)

                            last_line = f2.readline().decode()
                            line_search = re.search('.+ master offset\s+(-*\d+)\s.+ path delay\s+(-*\d+)',
                                                    last_line.strip())
                            if line_search:
                                # calcOffset = float(line_search.group(1)) * math.pow(10, -9)
                                reportedDelay = float(line_search.group(2)) * 10 ** (-9)
                                internal_data[index]['data'] = reportedDelay
                                logger.debug(f"{line['name']}: {reportedDelay}")
                                f.write(str(float(reportedDelay)) + ',')
                                break

                # Serial Offset value
                elif line['type'] == 'nxp-serial-offset':
                    while 1:
                        with open(line['logfile'], 'rb') as f2:
                            f2.seek(-2, os.SEEK_END)
                            while f2.read(1) != b'\n':
                                f2.seek(-2, os.SEEK_CUR)

                            last_line = f2.readline().decode()
                            line_search = re.search(
                                'offset from master:\s+(-?\d+)s\s+(-?\d+)ns\s+mean path delay:\s+(-?\d+)s\s+(-?\d+)ns',
                                last_line.strip())
                            if line_search:
                                offsetS = float(line_search.group(1))
                                offsetNs = float(line_search.group(2))
                                # delayS = float(line_search.group(3))
                                # delayNs = float(line_search.group(4))
                                calcOffset = offsetS + offsetNs * 10 ** (-9)
                                # reportedDelay = delayS + delayNs * 10 ** (-9)
                                internal_data[index]['data'] = calcOffset
                                logger.debug(f"{line['name']}: {calcOffset}")
                                f.write(str(float(calcOffset)) + ',')
                                break

                # Serial Delay value
                elif line['type'] == 'nxp-serial-delay':
                    while 1:
                        with open(line['logfile'], 'rb') as f2:
                            f2.seek(-2, os.SEEK_END)
                            while f2.read(1) != b'\n':
                                f2.seek(-2, os.SEEK_CUR)

                            last_line = f2.readline().decode()
                            line_search = re.search(
                                'offset from master:\s+(-?\d+)s\s+(-?\d+)ns\s+mean path delay:\s+(-?\d+)s\s+(-?\d+)ns',
                                last_line.strip())
                            if line_search:
                                # offsetS = float(line_search.group(1))
                                # offsetNs = float(line_search.group(2))
                                delayS = float(line_search.group(3))
                                delayNs = float(line_search.group(4))
                                # calcOffset = offsetS + offsetNs * 10 ** (-9)
                                reportedDelay = delayS + delayNs * 10 ** (-9)
                                internal_data[index]['data'] = reportedDelay
                                logger.debug(f"{line['name']}: {reportedDelay}")
                                f.write(str(float(reportedDelay)) + ',')
                                break
            f.write('\n')

            # Update shared memory
            lock.acquire()
            data['x_data'].append(count)
            for i in range(len(lines)):
                data[i].append(internal_data[i]['data'])
            lock.release()

            count += 1

            # Add file writing here

            logger.debug(f'clock: {count}')
            with clock.get_lock():
                clock.value = count
            while (int(time.time() * 1000) < (timeStartMs + 1000)):
                pass

        # WRITE EXIT FUNCTIONS HERE
        logger.debug("Exited gracefully!")
        new_file_path = save_file_as("Measurement data CSV file ...")
        if new_file_path:
            shutil.move(f'/tmp/delays-{uid}.csv', new_file_path)
            logger.debug(f"Saved CSV file as: {new_file_path}")
























    #
    #
    #
    #         # Real Offset
    #         delay = getDelay(ip)
    #         if 'E+37' in delay:
    #             delay = lastD
    #
    #         calcOffset = 0
    #         reportedDelay = 0
    #
    #         if slaveType == 'BBB' or slaveType == 'PTP4L':
    #             # Calculated Offset and Reported Delay (from BBB slave)
    #             while 1:
    #                 with open(logfile, 'rb') as f2:
    #                     f2.seek(-2, os.SEEK_END)
    #                     while f2.read(1) != b'\n':
    #                         f2.seek(-2, os.SEEK_CUR)
    #
    #                     last_line = f2.readline().decode()
    #                     line_search = re.search('.+ master offset\s+(-*\d+)\s.+ path delay\s+(-*\d+)',
    #                                             last_line.strip())
    #                     if line_search:
    #                         calcOffset = float(line_search.group(1)) * math.pow(10, -9)
    #                         reportedDelay = float(line_search.group(2)) * 10 ** (-9)
    #                         break
    #
    #         elif slaveType == 'NXP':
    #             # Calculated Offset and Reported Delay (from NXP slave)
    #             while 1:
    #                 with open(logfile, 'rb') as f2:
    #                     f2.seek(-2, os.SEEK_END)
    #                     while f2.read(1) != b'\n':
    #                         f2.seek(-2, os.SEEK_CUR)
    #
    #                     last_line = f2.readline().decode()
    #                     line_search = re.search(
    #                         'offset from master:\s+(-?\d+)s\s+(-?\d+)ns\s+mean path delay:\s+(-?\d+)s\s+(-?\d+)ns',
    #                         last_line.strip())
    #                     if line_search:
    #                         offsetS = float(line_search.group(1))
    #                         offsetNs = float(line_search.group(2))
    #                         delayS = float(line_search.group(3))
    #                         delayNs = float(line_search.group(4))
    #                         calcOffset = offsetS + offsetNs * 10 ** (-9)
    #                         reportedDelay = delayS + delayNs * 10 ** (-9)
    #                         break
    #
    #         # Append Values to Lists
    #         lock.acquire()
    #         data['x_data'].append(count)
    #         data['y_real_offset'].append(float(delay))
    #         data['y_calc_offset'].append(calcOffset)
    #         data['y_calc_delay'].append(reportedDelay)
    #         lock.release()
    #
    #         # Write to CSV and stdout
    #         f.write(f'{str(count)},{delay},{str(calcOffset)},{str(reportedDelay)},\n')
    #         logger.debug(
    #             f'Second: {count} RealOffset: {delay} CalculatedOffset: {str(calcOffset)} ReportedDelay: {str(reportedDelay)}')
    #         lastD = delay
    #         count += 1
    #         with clock.get_lock():
    #             clock.value = count
    #         while (int(time.time() * 1000) < (timeStartMs + 1000)):
    #             pass
    #
    # # WRITE EXIT FUNCTIONS HERE
    # logger.debug("Exited gracefully!")
    # new_file_path = save_file_as("Measurement data CSV file ...")
    # shutil.move(f'/tmp/delays-{uid}.csv', new_file_path)
    # logger.debug(f"Saved CSV file as: {new_file_path}")

#
# def plot(data, lines, lock, title):
#     fig = plt.figure()
#     ax1 = fig.add_subplot(1, 1, 1)
#
#     ani_run = True
#     lined = {}
#
#
#     def pause_on_p(event):
#         nonlocal ani_run
#         if event.key == 'p':
#             if ani_run:
#                 ani.event_source.stop()
#                 ani_run = False
#             else:
#                 ani.event_source.start()
#                 ani_run = True
#
#     def on_pick(event):
#         # On the pick event, find the original line corresponding to the legend
#         # proxy line, and toggle its visibility.
#         legline = event.artist
#         origline = lined[legline]
#         visible = not origline.get_visible()
#         origline.set_visible(visible)
#         # Change the alpha on the line in the legend so we can see what lines
#         # have been toggled.
#         legline.set_alpha(1.0 if visible else 0.2)
#         fig.canvas.draw()
#
#     def animate(i):
#         nonlocal lined
#         lineslist = []
#         ax1.clear()
#         lock.acquire()
#         xlen = len(data['x_data'])
#
#         for i in range(len(lines)):
#             lin, = ax1.plot(data['x_data'][:xlen], data[i][:xlen], label=lines[i]['name'])
#             lineslist.append(lin)
#         # ax1.plot(data['x_data'][:xlen], data['y_real_offset'][:xlen], label='Actual Offset')
#         # ax1.plot(data['x_data'][:xlen], data['y_calc_offset'][:xlen], label='Reported Offset')
#         # ax1.plot(data['x_data'][:xlen], data['y_calc_delay'][:xlen], label='Reported Delay')
#         lock.release()
#
#         leg = ax1.legend(fancybox=True, shadow=True)
#
#         for legline, origline in zip(leg.get_lines(), lineslist):
#             legline.set_picker(True)  # Enable picking on the legend line.
#             lined[legline] = origline
#
#         plt.xlabel('Time Elapsed (seconds)')
#         plt.ylabel('(seconds)')
#         if title:
#             plt.title(title)
#
#     fig.canvas.mpl_connect('key_press_event', pause_on_p)
#     fig.canvas.mpl_connect('pick_event', on_pick)
#     ani = animation.FuncAnimation(fig, animate, interval=1000)
#     plt.show()




def plot(data, lines, lock, title):
    fig = plt.figure()
    ax1 = fig.add_subplot(1, 1, 1)

    ani_run = True

    def pause_on_p(event):
        nonlocal ani_run
        if event.key == 'p':
            if ani_run:
                ani.event_source.stop()
                ani_run = False
            else:
                ani.event_source.start()
                ani_run = True

    def animate(i):

        ax1.clear()
        lock.acquire()
        xlen = len(data['x_data'])

        for i in range(len(lines)):
            ax1.plot(data['x_data'][:xlen], data[i][:xlen], label=lines[i]['name'])
        # ax1.plot(data['x_data'][:xlen], data['y_real_offset'][:xlen], label='Actual Offset')
        # ax1.plot(data['x_data'][:xlen], data['y_calc_offset'][:xlen], label='Reported Offset')
        # ax1.plot(data['x_data'][:xlen], data['y_calc_delay'][:xlen], label='Reported Delay')
        lock.release()
        plt.xlabel('Time Elapsed (seconds)')
        plt.ylabel('(seconds)')
        if title:
            plt.title(title)
        plt.legend()

    fig.canvas.mpl_connect('key_press_event', pause_on_p)
    ani = animation.FuncAnimation(fig, animate, interval=1000)
    plt.show()