# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import time
import sys
import subprocess
import shlex


index = int(sys.argv[1])
step = int(sys.argv[2])
interval = int(sys.argv[3])
limit = int(sys.argv[4])
start1 = int(sys.argv[5])
start2 = int(sys.argv[6])
flip = int(sys.argv[7])
filename = str(sys.argv[8])

dconfig = {
    'password': 'iot',
    'username': 'esi',
    'lfile': filename,
    'ip': '129.187.151.206',
    'rfile': '/home/esi/Desktop/delayTC.txt'
}

def incrementor_script(dconfig, index, step, interval, limit, start1, start2, flip, filename):

    vals = {
        'password': dconfig['password'],
        'username': dconfig['username'],
        'lfile': dconfig['lfile'],
        'ip': dconfig['ip'],
        'rfile': dconfig['rfile']
    }

    scp_cmd = "sshpass -p {password} scp -o ConnectTimeout=1 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no " \
              "{lfile} {username}@{ip}:{rfile}"
    scp_cmd = scp_cmd.format_map(vals)

    c = 0
    while True:
        with open(filename, 'w+') as f:
            if not c:
                c = 1
            else:
                if index == 0:
                    if start1 == 0 and step < 0:
                        if not flip:
                            break
                        index = 1
                        step = step * -1
                    elif start1 >= 0 and start1 < limit:
                            start1 += step
                    else:
                        break

                if index == 1:
                    if start2 == 0 and step < 0:
                        if not flip:
                            break
                        index = 0
                        step = step * -1
                        # Corner case
                        start1 += step
                    elif start2 >= 0 and start2 < limit:
                            start2 += step
                    else:
                        break

            print(f"{start1} {start2}")
            f.write(f"{start1} {start2}")

        subprocess.run(shlex.split(scp_cmd))
        time.sleep(int(interval))


incrementor_script(dconfig, index, step, interval, limit, start1, start2, flip, filename)