#!/usr/bin/env python3
import sys
import subprocess
import os

import json

# get date time
from datetime import datetime
now = datetime.now()

# How I want the program to work

def get_json_from_output(output) -> list:
    #sample output:
    #pair  "final sol length"       "168"
    #pair  "total raw cpu time"     "0.005418"
    #pair  "total wall time"        "0.005421"

    # from each pair, get the key and value and store it in a dictionary
    # return the dictionary

    output = output.split("\n")
    result = {}
    for line in output:
        arr = line.split("\t")
        if arr[0][:5] != "#pair":
            continue
        
        key = arr[0][8:-1]
        value = arr[1]
        result[key] = value

    return result

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
algo_list = algo_list.split(":")

def parse_output_text(match, output):
    output = output.split("\n")
    for line in output:
        if match in line:
            arr = line.split("\t")
            ret = arr[-1]
            ret = float(ret[1:-2])
            return ret
    return None

lst = os.listdir(folder)
number_files = len(lst)

# remove files that can't be cast to numbers
for file in lst:
    try:
        int(file)
    except:
        lst.remove(file)

lst.sort(key=lambda x: int(x))

failed_runs = []

print('folder: ', folder, file=sys.stderr)
print('number_files: ', number_files, file=sys.stderr)
print('files: ', lst, file=sys.stderr)
print('algorithms: ', algo_list, file=sys.stderr)

try:
    print("{")
    print(f"\"date\": \"{now.strftime('%m/%d/%Y %H:%M:%S')}\",")
    print(f"\"folder\": \"{folder}\",")
    print(f"\"number_files\": {number_files},")
    print(f"\"files\": {lst.__str__().replace("'", "\"")},")
    print(f"\"algorithms\": {algo_list.__str__().replace("'", "\"")},")
    print("\"results\": {")
    for file in lst: # non inclusive
        print(f"{folder}{file} --------------------", file=sys.stderr)
        
        print("\"" + str(file) + "\": {")

        for algo in algo_list:
            try:
                algokey = '-'.join(algo.split(" "))
                algokey = algokey.replace(" ", "")
                algokey += f"-{file}"
                command = f"{runner} {algo} < {folder}/{file} > tmp"
                output = result = subprocess.check_output(command, shell=True)
                f = open("tmp", "r")
                output = f.read()

                result = get_json_from_output(output)
                result["file"] = file
                print(result, file=sys.stderr)

                f.close()
                os.remove("tmp")

                print(f"\"{algokey}\": {json.dumps(result)},")

                cpu_time = result["total wall time"]
                if cpu_time is None:
                    print(f"Unable to parse the wall time of {algo}", file=sys.stderr)
                else:
                    print(f"{algo}: {cpu_time}", file=sys.stderr)

            except Exception as e:
                print(f"Error: {e}", file=sys.stderr)
                failed_runs.append(f"{command}")
                
        print("},")    
        
    print("},")
    print("\"failed_runs\": ")
    print(failed_runs)
    print("}")


except KeyboardInterrupt:
    print("Interrupted. Exiting...", file=sys.stderr)
    print("},")
    print(f'\"failed_runs\": {failed_runs.__str__().replace("'", "\"")}')
    print("}")
    exit(1)