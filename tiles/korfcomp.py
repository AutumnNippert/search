#!/usr/bin/env python
# take input arg of name of algorithm

import sys
import subprocess

algo1 = sys.argv[1]
algo2 = sys.argv[2]
folder = sys.argv[3]


def parse_cpu_time(output):
    output = output.split("\n")
    for line in output:
        if 'total raw cpu time' in line:
            arr = line.split("\t")
            ret = arr[-1]
            ret = float(ret[1:-2])
            return ret
    return None

algo1_cum_time = 0.0
algo2_cum_time = 0.0

for i in range(1, 101): # non inclusive
    print("Korf 4/4/" + str(i) + " --------------------")

    command = "./15md_solver " + algo1 + " < " + folder + str(i) + "> tmp"
    output = result = subprocess.check_output(command, shell=True)
    f = open("tmp", "r")
    output = f.read() 
    cpu_time = parse_cpu_time(output)
    print(f"{algo1}: {cpu_time}")
    algo1_cum_time += cpu_time

    command = "./15md_solver " + algo2 + " < " + folder + str(i) + "> tmp"
    output = result = subprocess.check_output(command, shell=True)
    f = open("tmp", "r")
    output = f.read() 
    cpu_time = parse_cpu_time(output)
    print(f"{algo2}: {cpu_time}")
    algo2_cum_time += cpu_time

print(f"====================\nFinal Stats:\n{algo1} cumulative time: {algo1_cum_time}\n{algo2} cumulative time: {algo2_cum_time}")
