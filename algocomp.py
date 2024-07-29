#!/usr/bin/env python
# take input arg of name of algorithm

import sys
import subprocess
import os

import json

# get date time
from datetime import datetime
now = datetime.now()

class AlgoResult:
    def __init__(self, file, wall_start_date, wall_start_time, machine_id, cost, initial_heuristic, initial_distance, algorithm, final_sol_cost, state_size, packed_state_size, final_sol_length, total_raw_cpu_time, total_wall_time, total_nodes_expanded, total_nodes_generated, total_nodes_duplicated, total_nodes_reopened, closed_list_type, closed_fill, closed_collisions, closed_resizes, closed_buckets, closed_max_bucket_fill, open_list_type, node_size, wall_finish_time, wall_finish_date, max_virtual_kilobytes, threads = 0):
        self.file = file
        self.wall_start_date = wall_start_date
        self.wall_start_time = wall_start_time
        self.machine_id = machine_id
        self.cost = cost
        self.initial_heuristic = initial_heuristic
        self.initial_distance = initial_distance
        self.algorithm = algorithm
        self.final_sol_cost = final_sol_cost
        self.state_size = state_size
        self.packed_state_size = packed_state_size
        self.final_sol_length = final_sol_length
        self.total_raw_cpu_time = total_raw_cpu_time
        self.total_wall_time = total_wall_time
        self.total_nodes_expanded = total_nodes_expanded
        self.total_nodes_generated = total_nodes_generated
        self.total_nodes_duplicated = total_nodes_duplicated
        self.total_nodes_reopened = total_nodes_reopened
        self.closed_list_type = closed_list_type
        self.closed_fill = closed_fill
        self.closed_collisions = closed_collisions
        self.closed_resizes = closed_resizes
        self.closed_buckets = closed_buckets
        self.closed_max_bucket_fill = closed_max_bucket_fill
        self.open_list_type = open_list_type
        self.node_size = node_size
        self.wall_finish_time = wall_finish_time
        self.wall_finish_date = wall_finish_date
        self.max_virtual_kilobytes = max_virtual_kilobytes
        self.threads = threads

    def __str__(self):
        return f"file: {self.file}\nwall_start_date: {self.wall_start_date}\nwall_start_time: {self.wall_start_time}\nmachine_id: {self.machine_id}\ncost: {self.cost}\ninitial_heuristic: {self.initial_heuristic}\ninitial_distance: {self.initial_distance}\nalgorithm: {self.algorithm}\nfinal_sol_cost: {self.final_sol_cost}\nstate_size: {self.state_size}\npacked_state_size: {self.packed_state_size}\nfinal_sol_length: {self.final_sol_length}\ntotal_raw_cpu_time: {self.total_raw_cpu_time}\ntotal_wall_time: {self.total_wall_time}\ntotal_nodes_expanded: {self.total_nodes_expanded}\ntotal_nodes_generated: {self.total_nodes_generated}\ntotal_nodes_duplicated: {self.total_nodes_duplicated}\ntotal_nodes_reopened: {self.total_nodes_reopened}\nclosed_list_type: {self.closed_list_type}\nclosed_fill: {self.closed_fill}\nclosed_collisions: {self.closed_collisions}\nclosed_resizes: {self.closed_resizes}\nclosed_buckets: {self.closed_buckets}\nclosed_max_bucket_fill: {self.closed_max_bucket_fill}\nopen_list_type: {self.open_list_type}\nnode_size: {self.node_size}\nwall_finish_time: {self.wall_finish_time}\nwall_finish_date: {self.wall_finish_date}\nmax_virtual_kilobytes: {self.max_virtual_kilobytes}"
    
    def __repr__(self):
        return self.__str__()
    
    def to_json(self):
        return json.dumps(self.__dict__)
    
    def from_output(file, output):
        """
        All output is in order
        output excerpt example:

        #pair  "final sol length"       "168"
        #pair  "total raw cpu time"     "0.005418"
        #pair  "total wall time"        "0.005421"
        """
        output = output.split("\n")
        output = output[1:-2]
        wall_start_date = wall_start_time = machine_id = cost = initial_heuristic = initial_distance = algorithm = final_sol_cost = state_size = packed_state_size = final_sol_length = total_raw_cpu_time = total_wall_time = total_nodes_expanded = total_nodes_generated = total_nodes_duplicated = total_nodes_reopened = closed_list_type = closed_fill = closed_collisions = closed_resizes = closed_buckets = closed_max_bucket_fill = open_list_type = node_size = wall_finish_time = wall_finish_date = max_virtual_kilobytes = threads = None
        for line in output:
            arr = line.split("\t")
            key = arr[0][8:-1]
            val = arr[1][1:-1]
            if key == "file":
                file = val
            elif key == "wall start date":
                wall_start_date = val
            elif key == "wall start time":
                wall_start_time = val
            elif key == "machine id":
                machine_id = val
            elif key == "cost":
                cost = val
            elif key == "initial heuristic":
                initial_heuristic = val
            elif key == "initial distance":
                initial_distance = val
            elif key == "algorithm":
                algorithm = val
            elif key == "final sol cost":
                final_sol_cost = val
            elif key == "state size":
                state_size = val
            elif key == "packed state size":
                packed_state_size = val
            elif key == "final sol length":
                final_sol_length = val
            elif key == "total raw cpu time":
                total_raw_cpu_time = val
            elif key == "total wall time":
                total_wall_time = val
            elif key == "total nodes expanded":
                total_nodes_expanded = val
            elif key == "total nodes generated":
                total_nodes_generated = val
            elif key == "total nodes duplicated":
                total_nodes_duplicated = val
            elif key == "total nodes reopened":
                total_nodes_reopened = val
            elif key == "closed list type":
                closed_list_type = val
            elif key == "closed fill":
                closed_fill = val
            elif key == "closed collisions":
                closed_collisions = val
            elif key == "closed resizes":
                closed_resizes = val
            elif key == "closed buckets":
                closed_buckets = val
            elif key == "closed max bucket fill":
                closed_max_bucket_fill = val
            elif key == "open list type":
                open_list_type = val
            elif key == "node size":
                node_size = val
            elif key == "wall finish time":
                wall_finish_time = val
            elif key == "wall finish date":
                wall_finish_date = val
            elif key == "max virtual kilobytes":
                max_virtual_kilobytes = val
            elif key == "threads":
                threads = val
        if threads == None:
            threads = 0
        return AlgoResult(file, wall_start_date, wall_start_time, machine_id, cost, initial_heuristic, initial_distance, algorithm, final_sol_cost, state_size, packed_state_size, final_sol_length, total_raw_cpu_time, total_wall_time, total_nodes_expanded, total_nodes_generated, total_nodes_duplicated, total_nodes_reopened, closed_list_type, closed_fill, closed_collisions, closed_resizes, closed_buckets, closed_max_bucket_fill, open_list_type, node_size, wall_finish_time, wall_finish_date, max_virtual_kilobytes, threads)

if len(sys.argv) <= 3:
    print("Usage: python3 algocomp.py <path to executable> <path to folder> [algo1 -args:algo2 -args...]")
    exit(1)

runner = sys.argv[1]
folder = sys.argv[2]
algo_list = sys.argv[3:]

if not os.path.exists(folder):
    print(f"Error: {folder} does not exist.")
    exit(1)

if not os.path.isfile(runner):
    print(f"Error: {runner} is not a file.")
    exit(1)

if not os.path.exists("./algocomp_res"):
    os.makedirs("./algocomp_res")

# put algo_list into a string
algo_list = " ".join(algo_list)
# parse by ':'
algo_list = algo_list.split(":")

print(f"Running: {runner}")
print(f"Folder: {folder}")
print(f"Algorithms: {algo_list}")

def parse_output_text(match, output):
    output = output.split("\n")
    for line in output:
        if match in line:
            arr = line.split("\t")
            ret = arr[-1]
            ret = float(ret[1:-2])
            return ret
    return None

def parse_cpu_time(output):
    return parse_output_text("total raw cpu time", output)

cumulative_times = {}
algo_json_results = {} # key: algo, value: list of AlgoResult

lst = os.listdir(folder)
number_files = len(lst)

# remove files that can't be cast to numbers
for file in lst:
    try:
        int(file)
    except:
        lst.remove(file)

lst.sort(key=lambda x: int(x))


for file in lst: # non inclusive
    print(f"{folder}{file} --------------------")

    for algo in algo_list:
        algokey = '-'.join(algo.split(" "))
        algokey = algokey.replace(" ", "")
        command = f"{runner} {algo} < {folder}/{file} > tmp"
        output = result = subprocess.check_output(command, shell=True)
        f = open("tmp", "r")
        output = f.read()
        cpu_time = parse_cpu_time(output)
        if cpu_time is None:
            print(f"Error: The program was unable to parse the cpu time of {algo}")
            continue
        print(f"{algo}: {cpu_time}")
        if algo not in cumulative_times:
            cumulative_times[algokey] = 0
        cumulative_times[algokey] += cpu_time
        if algo not in algo_json_results:
            algo_json_results[algokey] = []
        algo_json_results[algokey].append(AlgoResult.from_output(file, output)) # this * thing is kinda cool
        # if -threads k is found, set algo's threads to k
        if "-threads" in algo:
            algo_json_results[algokey][-1].threads = int(algo.split(" ")[-1])
        f.close()
    
    
    filename = f'./algocomp_res/{file}-{now}.json'
    filename = filename.replace(":", "-") # Clean up for windows

    # append to json file for each file
    with open(f"{filename}", "w") as f:
        f.write(json.dumps([x.__dict__ for x in algo_json_results[algo]]))
        f.close()

print("==========Final Stats==========")
for algo in algo_list:
    print(f"{algo: <10}: {cumulative_times[algo]}")


# for algo in algo_json_results:
#     # write to json file
#     with open(f"./algocomp_res/{algo}-{now}.json", "w") as f:
#         f.write(json.dumps([x.__dict__ for x in algo_json_results[algo]]))
#         f.close()