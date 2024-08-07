#!/usr/bin/env python3
import sys
import subprocess
import os
import json
from datetime import datetime

# Get the current date and time
now = datetime.now()

def get_json_from_output(output: str) -> dict:
    result = {}
    for line in output.split("\n"):
        if line.startswith("#pair"):
            parts = line.split("\t")
            key = parts[0][8:-1].strip()
            value = parts[1].strip()
            value = value[1:-1]
            if value.isdigit():
                value = int(value)
            else:
                try:
                    value = float(value)
                except ValueError:
                    pass
                
            result[key] = value
    return result

def parse_output_text(match, output):
    for line in output.split("\n"):
        if match in line:
            return float(line.split("\t")[-1][1:-2])
    return None

if len(sys.argv) <= 3:
    print(json.dumps({"error": "Usage: python3 algocomp.py <path to executable> <path to folder> [algo1 -args:algo2 -args...]"}))
    exit(1)

runner = sys.argv[1]
folder = sys.argv[2]
algo_list = sys.argv[3:]

SKIP_EXISTING = True

if not os.path.exists(folder):
    print(json.dumps({"error": f"{folder} does not exist."}))
    exit(1)

if not os.path.isfile(runner):
    print(json.dumps({"error": f"{runner} is not a file."}))
    exit(1)

if not os.path.exists("./algocomp_res"):
    os.makedirs("./algocomp_res")

# Prepare algorithms list
algo_list = [algo.strip() for algo in " ".join(algo_list).split(":")]

# Collect list of files in folder
lst = [f for f in os.listdir(folder) if f.isdigit()]
lst.sort(key=int)

results = {
    "date": now.strftime('%m/%d/%Y %H:%M:%S'),
    "folder": folder,
    "number_files": len(lst),
    "files": lst,
    "algorithms": algo_list,
    "failed_runs": []
}

# Process each file
for file in lst:
    print(f"Processing file {file}...", file=sys.stderr)
    file_results = {}
    for algo in algo_list:
        algokey = '_'.join(algo.split()).replace(" ", "") + f"-{file}"
        algoname = algo.split()[0]
        args = algo.split()[1:]
        args_str = " ".join(args)
        command = f"{runner} {algo} < {folder}/{file} > tmp"
        astar_command = f"{runner} astar-basic {args_str} < {folder}/{file} > tmp"
        try:
            if "-exp" in args_str:
                # get number after -exp in command
                exp = args_str.split("-exp")[1].split()[0]
                exp = int(exp)
            else:
                exp = 0

            #same with threads
            if "-threads" in args_str:
                threads = args_str.split("-threads")[1].split()[0]
                threads = int(threads)
            else:
                threads = 1
            
            if(SKIP_EXISTING):
                print(f"Checking if ./algocomp_res/{algoname}-{threads}-{exp}-{file}.json exists", file=sys.stderr)
                if(os.path.exists(f"./algocomp_res/{algoname}-{threads}-{exp}-{file}.json")):
                    print(f"Skipping {algokey} as it already exists", file=sys.stderr)
                    continue
            
            subprocess.check_output(command, shell=True)
            with open("tmp", "r") as f:
                output = f.read()
            os.remove("tmp")

            subprocess.check_output(astar_command, shell=True)
            with open("tmp", "r") as f:
                astar_output = f.read()
            os.remove("tmp")

            result = get_json_from_output(output)
            astar_result = get_json_from_output(astar_output)

            result["file"] = file
            result["threads"] = threads
            result["slowdown"] = exp
            result["astar wall time"] = astar_result["total wall time"]
            result["astar expansion rate"] = astar_result["total nodes expanded"] / astar_result["total wall time"]

            # print all results to stderr
            print(f"{algokey}", file=sys.stderr)
            #print result in a nice looking json format
            print(json.dumps(result, indent=4))

            # out to json with algokey and filename
            with open(f"./algocomp_res/{result['algorithm']}-{result['threads']}-{result['slowdown']}-{file}.json", "w") as f:
                f.write(json.dumps(result))

        except Exception as e:
            results["failed_runs"].append({"command": command, "error": str(e)})
            print(f"Error running {command}: {e}", file=sys.stderr)
    
    print(f"Runtime")

print(results["failed_runs"])