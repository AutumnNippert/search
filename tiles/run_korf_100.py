# take input arg of name of algorithm

import sys
import subprocess
from time import time

algo = sys.argv[1]
# run the command 15md_solver <algorithm> < /home/aifs2/group/data/tiles_instances/korf/4/4/1-100
for i in range(1, 101):
    print("Korf 4/4/" + str(i))
    command = "./15md_solver " + algo + " < /home/aifs2/group/data/tiles_instances/korf/4/4/" + str(i)
    print(command)
    algo_start_time = time()
    output = result = subprocess.check_output(command, shell=True)
    algo_end_time = time()
    output = output.decode("utf-8")
    print(output)