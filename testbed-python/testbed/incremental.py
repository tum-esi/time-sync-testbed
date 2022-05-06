# Copyright (c) Thomas Wakim, 2022
#     t7wakim@gmail.com

import time
import sys

index = int(sys.argv[1])
step = int(sys.argv[2])
interval = int(sys.argv[3])
limit = int(sys.argv[4])
start1 = int(sys.argv[5])
start2 = int(sys.argv[6])

c = 0
while True:
    with open('delayTC.txt', 'w+') as f:
        if start1 < 0 or start2 < 0:
            break
        print(f"{start1} {start2}")
        f.write(f"{start1} {start2}")
        if not c:
            c = 1
            time.sleep(int(interval))

        if index == 0:
            if start1 > -1 and start1 < limit:
                start1 += step
            else:
                break
        elif index == 1:
            if start2 > -1 and start2 < limit:
                start2 += step
            else:
                break
    time.sleep(int(interval))
