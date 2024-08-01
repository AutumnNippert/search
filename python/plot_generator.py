# import numpy, matplotlib, pandas, and json
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import json
import sys
import os

"""
Example imput:
{"cafe--threads-1": [{"file": "1", "wall_start_date": "Tue Jul 30 14:37:19 2024", "wall_start_time": "1.72236e+09", "machine_id": "mica.cs.unh.edu-Linux-6.8.9-100.fc38.x86_64-x86_64", "cost": "unit", "initial_heuristic": "41.000000", "initial_distance": "41.000000", "algorithm": "cafe", "final_sol_cost": "57.000000", "state_size": "76", "packed_state_size": "8", "final_sol_length": "58", "total_raw_cpu_time": "15.516084", "total_wall_time": "15.583273", "total_nodes_expanded": "14000490", "total_nodes_generated": "27587724", "total_nodes_duplicated": "2492978", "total_nodes_reopened": "0", "closed_list_type": null, "closed_fill": null, "closed_collisions": null, "closed_resizes": null, "closed_buckets": null, "closed_max_bucket_fill": null, "open_list_type": "closed", "node_size": "40", "wall_finish_time": "1.72236e+09", "wall_finish_date": "Tue Jul 30 14:37:37 2024", "max_virtual_kilobytes": "19543172", "threads": 1}], "cafe--threads-2": [{"file": "1", "wall_start_date": "Tue Jul 30 14:37:37 2024", "wall_start_time": "1.72236e+09", "machine_id": "mica.cs.unh.edu-Linux-6.8.9-100.fc38.x86_64-x86_64", "cost": "unit", "initial_heuristic": "41.000000", "initial_distance": "41.000000", "algorithm": "cafe", "final_sol_cost": "57.000000", "state_size": "76", "packed_state_size": "8", "final_sol_length": "58", "total_raw_cpu_time": "44.076482", "total_wall_time": "22.097168", "total_nodes_expanded": "14000490", "total_nodes_generated": "27587724", "total_nodes_duplicated": "2492978", "total_nodes_reopened": "0", "closed_list_type": null, "closed_fill": null, "closed_collisions": null, "closed_resizes": null, "closed_buckets": null, "closed_max_bucket_fill": null, "open_list_type": "closed", "node_size": "40", "wall_finish_time": "1.72236e+09", "wall_finish_date": "Tue Jul 30 14:38:02 2024", "max_virtual_kilobytes": "33613872", "threads": 2}], "cafe--threads-4": [{"file": "1", "wall_start_date": "Tue Jul 30 14:38:02 2024", "wall_start_time": "1.72236e+09", "machine_id": "mica.cs.unh.edu-Linux-6.8.9-100.fc38.x86_64-x86_64", "cost": "unit", "initial_heuristic": "41.000000", "initial_distance": "41.000000", "algorithm": "cafe", "final_sol_cost": "57.000000", "state_size": "76", "packed_state_size": "8", "final_sol_length": "58", "total_raw_cpu_time": "163.389863", "total_wall_time": "40.955830", "total_nodes_expanded": "14000490", "total_nodes_generated": "27587724", "total_nodes_duplicated": "2492978", "total_nodes_reopened": "0", "closed_list_type": null, "closed_fill": null, "closed_collisions": null, "closed_resizes": null, "closed_buckets": null, "closed_max_bucket_fill": null, "open_list_type": "closed", "node_size": "40", "wall_finish_time": "1.72236e+09", "wall_finish_date": "Tue Jul 30 14:38:45 2024", "max_virtual_kilobytes": "61820804", "threads": 4}], "cafe--threads-8": [{"file": "1", "wall_start_date": "Tue Jul 30 14:38:45 2024", "wall_start_time": "1.72236e+09", "machine_id": "mica.cs.unh.edu-Linux-6.8.9-100.fc38.x86_64-x86_64", "cost": "unit", "initial_heuristic": "41.000000", "initial_distance": "41.000000", "algorithm": "cafe", "final_sol_cost": "57.000000", "state_size": "76", "packed_state_size": "8", "final_sol_length": "58", "total_raw_cpu_time": "301.861354", "total_wall_time": "37.825911", "total_nodes_expanded": "14000490", "total_nodes_generated": "27587724", "total_nodes_duplicated": "2492978", "total_nodes_reopened": "0", "closed_list_type": null, "closed_fill": null, "closed_collisions": null, "closed_resizes": null, "closed_buckets": null, "closed_max_bucket_fill": null, "open_list_type": "closed", "node_size": "40", "wall_finish_time": "1.72236e+09", "wall_finish_date": "Tue Jul 30 14:39:25 2024", "max_virtual_kilobytes": "118431288", "threads": 8}], "cafe--threads-16": [{"file": "1", "wall_start_date": "Tue Jul 30 14:39:25 2024", "wall_start_time": "1.72236e+09", "machine_id": "mica.cs.unh.edu-Linux-6.8.9-100.fc38.x86_64-x86_64", "cost": "unit", "initial_heuristic": "41.000000", "initial_distance": "41.000000", "algorithm": "cafe", "final_sol_cost": "57.000000", "state_size": "76", "packed_state_size": "8", "final_sol_length": "58", "total_raw_cpu_time": "598.252153", "total_wall_time": "37.488906", "total_nodes_expanded": "14000490", "total_nodes_generated": "27587724", "total_nodes_duplicated": "2492978", "total_nodes_reopened": "0", "closed_list_type": null, "closed_fill": null, "closed_collisions": null, "closed_resizes": null, "closed_buckets": null, "closed_max_bucket_fill": null, "open_list_type": "closed", "node_size": "40", "wall_finish_time": "1.72236e+09", "wall_finish_date": "Tue Jul 30 14:40:04 2024", "max_virtual_kilobytes": "230734748", "threads": 16}]}
"""

HELP = """
plot_generator.py
    -f <file> | -F <folder> : file or folder location of data
    --plot <xvar> <yvar> : plot xvar yvar
    --logx : log scale x axis
    --logy : log scale y axis
    --output / -o : output folder
    --override / -O : be prompted for x,y,title overrides
    -----------------------------------------------------------
    Example:
    python3 plot_generator.py -F algocomp_res/ --plot threads total_wall_time -O -o ./plots/
"""


def plot_xvar_over_yvar(data, xvar_name, yvar_name, plotname, xlog=False, ylog=False, x_var_override=None, y_var_override=None, title_override=None, output=None, yminmax=None):
    xvar = []
    yvar = []
    for key in data:
        for entry in data[key]:
            xvar.append(entry[xvar_name])
            yvar.append(entry[yvar_name])
    
    df = pd.DataFrame({xvar_name: xvar, yvar_name: yvar})
    print(df)   
    # plot the data
    # Apply a style for better aesthetics
    # plt.style.use('seaborn-pastel')
    plt.figure(figsize=(10, 6))
    plt.plot(df[xvar_name], df[yvar_name], marker='o', linestyle='-', color='b', label=yvar_name)

    # Adding labels and title
    plt.xlabel(xvar_name, fontsize=14)
    plt.ylabel(yvar_name, fontsize=14)
    if x_var_override:
        plt.xlabel(x_var_override, fontsize=14)
    if y_var_override:
        plt.ylabel(y_var_override, fontsize=14)
    if title_override:
        plt.title(title_override, fontsize=16)
    plt.title(f'{xvar_name} Over {yvar_name}', fontsize=16)

    if xlog:
        plt.xscale('log')
    if ylog:
        plt.yscale('log')

    if yminmax:
        plt.ylim(yminmax[0], yminmax[1])
    # plt.grid(True)
    plt.legend()
    # plt.tight_layout()
    if output:
        plt.savefig(output + '/' + plotname + '.png', dpi=300)
    else:
        plt.savefig(plotname + '.png', dpi=300)

def get_json_from_file(file_location):
    with open(file_location, 'r') as f:
        return json.load(f)

if __name__ == '__main__':
    file = None
    folder = None
    plot = None
    logx = False
    logy = False
    output = None

    # parsing time
    args = sys.argv
    if len(args) < 2:
        print('Please provide a folder location')
        sys.exit(1)
    
    if '--help' in args:
        print(HELP)
        sys.exit(0)

    if '-f' in args:
        file = args[args.index('-f') + 1]

    if '-F' in args:
        folder = args[args.index('-F') + 1]

    if '--plot' in args:
        xvar = args[args.index('--plot') + 1]
        yvar = args[args.index('--plot') + 2]
        plot = (xvar, yvar)
    
    if '--logx' in args:
        logx = True

    if '--logy' in args:
        logy = True

    if '--output' in args:
        output = args[args.index('--output') + 1]
        if not os.path.exists(output):
            os.makedirs(output)
    elif '-o' in args:
        output = args[args.index('-o') + 1]
        # create output dir if not exist
        if not os.path.exists(output):
            os.makedirs(output)

    if file and folder:
        print('Please provide either a file or a folder')
        sys.exit(1)

    #prompt user for updated x and y axis labels
    x_var_override = None
    y_var_override = None
    title_override = None
    if '--override' in args or '-O' in args:
        x_var_override = input('Enter x axis label: ')
        y_var_override = input('Enter y axis label: ')
        title_override = input('Enter title: ')
    
    try:
        if file and plot:
            data = get_json_from_file(file)
            plotname = file.split('.')[0]
            plot_xvar_over_yvar(data, plot[0], plot[1], plotname, logx, logy, x_var_override, y_var_override, title_override, output, (0,36))
        elif folder and plot:
            files = os.listdir(folder)
            for file in files:
                data = get_json_from_file(folder + '/' + file)
                plotname = file.split('.')[0]
                plot_xvar_over_yvar(data, plot[0], plot[1], plotname, logx, logy, x_var_override, y_var_override, title_override, output, (0,36))
        else:
            print('Please provide a file/folder and plot')
            sys.exit(1)
    except Exception as e:
        print(e)
        sys.exit(1)
