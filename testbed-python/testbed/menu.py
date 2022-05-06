# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

from consolemenu import *
from consolemenu.items import *
from consolemenu.menu_component import Dimension
from consolemenu.screen import Screen
from . import logger
from .parser import parse_yaml
from .commandmaker import ProcessState
from .tcattack import tc_attack
from .monitor import *
from .plotter import *
from .attack import *
from .executor import execute
import copy
import multiprocessing
import time

configuration = {}
processes = []
mon_processes = []
plotters = []
attacks = []


def spawn_menu():

    # duplicated here only for early error detection
    parse_yaml("configs/OSCILLO.yaml")

    global configuration
    configuration = parse_yaml("configs/startup.yaml")

    global processes
    processes = process_creator(configuration)

    global mon_processes
    mon_processes = init_monitor_procs('configs/monitor.yaml')

    global plotters
    plotters = init_plotters('configs/plotter.yaml')

    global attacks
    attacks = init_attacks('configs/attack.yaml')

    format_dim = MenuFormatBuilder(max_dimension=Dimension(width=120, height=60))

    menu = ConsoleMenu("Main menu", "Welcome to the PTP testbed!", clear_screen=False, formatter=format_dim)
    setup_submenu = ConsoleMenu("This is the Setup submenu", clear_screen=False, formatter=format_dim)
    status_submenu = ConsoleMenu("This is the Status submenu", clear_screen=False, formatter=format_dim)
    terminate_submenu = ConsoleMenu("This is the Terminate submenu", clear_screen=False, formatter=format_dim)
    monitor_submenu = ConsoleMenu("This is the Monitor submenu", clear_screen=False, formatter=format_dim)
    plotter_submenu = ConsoleMenu("This is the Plotter submenu", clear_screen=False, formatter=format_dim)
    attack_submenu = ConsoleMenu("This is the Attack submenu", clear_screen=False, formatter=format_dim)

    setup_submenu_item = SubmenuItem("Setup", setup_submenu, menu=menu)
    menu.append_item(setup_submenu_item)
    parse_config_item = FunctionItem("Parse config file", parse_add_setup_items, args=[setup_submenu], menu=setup_submenu)
    setup_submenu.append_item(parse_config_item)

    status_submenu_item = SubmenuItem("Status", status_submenu, menu=menu)
    menu.append_item(status_submenu_item)
    get_status_item = FunctionItem("Get status", populate_status_submenu, args=[status_submenu], menu=status_submenu)
    status_submenu.append_item(get_status_item)
    # get_status_item = FunctionItem("Clear status", clean_status_submenu, args=[status_submenu], menu=status_submenu)
    # status_submenu.append_item(get_status_item)

    terminate_submenu_item = SubmenuItem("Terminate", terminate_submenu, menu=menu)
    menu.append_item(terminate_submenu_item)
    get_running_item = FunctionItem("Get running processes", populate_terminate_submenu, args=[terminate_submenu], menu=terminate_submenu)
    terminate_submenu.append_item(get_running_item)

    monitor_submenu_item = SubmenuItem("Monitor", monitor_submenu, menu=menu)
    populate_monitor_submenu(monitor_submenu)
    menu.append_item(monitor_submenu_item)

    plotter_submenu_item = SubmenuItem("Plotter", plotter_submenu, menu=menu)
    populate_plotter_submenu(plotter_submenu)
    menu.append_item(plotter_submenu_item)

    attack_submenu_item = SubmenuItem("Attack", attack_submenu, menu=menu)
    populate_attack_submenu(attack_submenu)
    menu.append_item(attack_submenu_item)

    menu.show()


def populate_terminate_submenu(submenu):
    refresh_all_states()
    clean_terminate_submenu(submenu)

    terminate_item = FunctionItem("Terminate all", terminate_all_processes, menu=submenu)
    submenu.append_item(terminate_item)

    for proc in processes:
        if proc.state == ProcessState.RUNNING:
            terminate_item = FunctionItem(proc.print_state(), terminate_process, args=[proc],
                                          menu=submenu)
            submenu.append_item(terminate_item)
    for proc in mon_processes:
        if proc.state == ProcessState.RUNNING:
            status_item = FunctionItem(proc.print_state(), terminate_process, args=[proc], menu=submenu)
            submenu.append_item(status_item)


def terminate_process(proc):
    proc.terminate()


def populate_attack_submenu(submenu):
    for attack in attacks:
        attack_item = FunctionItem(f'{"Start":<20} {attack.name:>20}', start_attack, args=[attack], menu=submenu)
        attack_item_stop = FunctionItem(f'{"Terminate":<20} {attack.name:>20}', terminate_attack, args=[attack],
                                        menu=submenu)
        submenu.append_item(attack_item)
        submenu.append_item(attack_item_stop)
    pass


def start_attack(attack):
    attack.start()


def terminate_attack(attack):
    attack.terminate()


def populate_plotter_submenu(submenu):
    for plotter in plotters:
        plotter_item = FunctionItem(f'{"Start":<20} {plotter.name:>20}', start_plotter, args=[plotter], menu=submenu)
        plotter_item_stop = FunctionItem(f'{"Terminate":<20} {plotter.name:>20}', terminate_plotter, args=[plotter], menu=submenu)
        submenu.append_item(plotter_item)
        submenu.append_item(plotter_item_stop)
    pass


def start_plotter(plotter):
    plotter.start()
    # while(1):
    #     logger.info(main_clock.value)
    #     time.sleep(1)


def terminate_plotter(plotter):
    plotter.terminate()


def populate_monitor_submenu(submenu):
    for mproc in mon_processes:
        monitor_item = FunctionItem(mproc.name, start_mon_process, args=[mproc], menu=submenu)
        submenu.append_item(monitor_item)
    follow_monitor_item = FunctionItem('Follow raw logs (requires `klogg`)', follow_mon_process, menu=submenu)
    submenu.append_item(follow_monitor_item)
    pass


def follow_mon_process():
    log_files = []
    for mproc in mon_processes:
        if mproc.state == ProcessState.RUNNING:
            log_files.append(mproc.lfile)
    try:
        execute(f'klogg -f {" ".join(log_files)}', block=False)
    except Exception as e:
        logger.error(e)


def start_mon_process(proc):
    if proc.state == ProcessState.NEW or proc.state == ProcessState.EXITED:
        proc.run()
    else:
        logger.error(f'Process {proc.name} is already running!')


def terminate_all_processes():
    for proc in mon_processes:
        proc.terminate()
    for proc in processes:
        proc.terminate()


def clean_status_submenu(submenu):
    items = copy.copy(submenu.items)
    for item in items:
        if type(item) == MenuItem:
            submenu.remove_item(item)


def clean_terminate_submenu(submenu):
    items = copy.copy(submenu.items)
    first = True
    for item in items:
        if first:
            first = False
            continue
        if type(item) == FunctionItem:
            submenu.remove_item(item)


def populate_status_submenu(submenu):
    refresh_all_states()
    clean_status_submenu(submenu)
    for proc in processes:
        status_item = MenuItem(proc.print_state(), menu=submenu)
        submenu.append_item(status_item)
    for proc in mon_processes:
        status_item = MenuItem(proc.print_state(), menu=submenu)
        submenu.append_item(status_item)


def parse_add_setup_items(submenu):
    global processes
    setup_item = FunctionItem("Setup all", start_processes_for_dev, args=[True], menu=submenu)
    submenu.append_item(setup_item)
    devs = configuration.keys()
    for dev in devs:
        dev_item = FunctionItem(dev, start_processes_for_dev, args=[False, dev], menu=submenu)
        submenu.append_item(dev_item)
    return


def start_processes_for_dev(all, dev='all'):
    logger.info(f"Starting processes for {dev}")
    # Screen.println(f"Starting processes for {dev}")
    if not all:
        for devs in Device.instances:
            if devs.name == dev:
                for proc in devs.processes:
                    if proc.outfile == None:
                        proc.run()
    else:
        for devs in Device.instances:
            for proc in devs.processes:
                if proc.outfile == None:
                    proc.run()

    pass
