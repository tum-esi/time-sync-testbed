# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import logging

from .parser import parse_yaml
from .commandmaker import *
from .menu import *
from . import logger
import pprint
import time



spawn_menu()




pass



# prepared = device_splitter(config)
#
# running_procs = []
#
# running_pids = []
#
# for k,v in prepared.items():
#     procs = execute_list(v)
#     running_procs.extend([v[0], procs])
#     for x in procs:
#         running_pids.append([v[0], str(x.pid)])
#
# logger.info(f'PIDs: {" ".join(running_pids)}')
#
# while(True):
#     try:
#         pass
#     except KeyboardInterrupt:
#         break
#
# logger.info("Killing ...")
#
# for proc in running_procs:
#     proc[1].terminate()
#





