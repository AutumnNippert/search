# take in an input folder
# read in the data
# put into a pandas dataframe where the first column is experiment number (arbitrarily based on the order of the files loaded), and all other columns based on the data in the files
# plot the data

"""
Data looks as such in the input files:
{
    "wall start date": "Tue Aug  6 14:38:39 2024",
    "wall start time": 1722970000.0,
    "machine id": "mica.cs.unh.edu-Linux-6.8.9-100.fc38.x86_64-x86_64",
    "cost": "unit",
    "initial heuristic": 43.0,
    "initial distance": 43.0,
    "algorithm": "spastar",
    "final sol cost": 55.0,
    "state size": 76,
    "packed state size": 8,
    "final sol length": 56,
    "total raw cpu time": 145.874965,
    "total wall time": 51.227946,
    "total nodes expanded": 4194609,
    "total nodes generated": 8300685,
    "total nodes duplicated": 703802,
    "total nodes reopened": 0,
    "open list type": "binary heap",
    "node size": 40,
    "wall finish time": 1722970000.0,
    "wall finish date": "Tue Aug  6 14:39:30 2024",
    "max virtual kilobytes": 1022964,
    "file": "2",
    "algo": "spastar -threads 8",
    "slowdown": 0,
    "threads": "8",
    "astar wall time": 4.269387
}
"""

import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import glob

def load_data(glob_str) -> pd.DataFrame:
    data = []
    files = glob.glob(glob_str)
    for file in files:
        with open(file) as f:
            data.append(eval(f.read()))
    return pd.DataFrame(data)

def figure_gen_plot(df, x, y, title, xlabel, ylabel) -> plt.figure:
    plt.figure()
    plt.plot(df[x], df[y])
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.title(title)
    return plt

def figure_gen(df, x, y, title, xlabel, ylabel, type=plt.plot) -> plt.figure:
    plt.figure()
    type(df[x], df[y])
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.title(title)
    return plt

def thread_speedup_plot(df, algo, exp, file) -> plt.figure:
    df = df[df['algorithm'] == algo]
    df = df[df['file'] == file]
    df = df[df['slowdown'] == exp]
    return figure_gen(df, 'threads', 'Speedup', f'{algo} Speedup over Thread Count at Slowdown of {exp}', 'Threads', 'Speedup')

def expansion_rate_plot(df, algo, thread_count, file) -> plt.figure:
    df = df[df['algorithm'] == algo]
    df = df[df['file'] == file]
    df = df[df['threads'] == thread_count]
    return figure_gen(df, 'Astar Expansion Rate', 'Speedup', f'{algo} Speedup at A* Expansion Rate with {thread_count} threads', 'A* Expansion Rate', 'Speedup')

def export_all_expansion_rate_plots(df, algo, thread_count) -> None:
    for file in df['file'].unique():
        fig = expansion_rate_plot(df, algo, thread_count, file)
        fig.savefig(f'{algo}_{thread_count}_expansion_rate_{file}.png')
        
def export_all_speedup_plots(df, algo, exp) -> None:
    for file in df['file'].unique():
        fig = thread_speedup_plot(df, algo, exp, file)
        fig.savefig(f'{algo}_{exp}_speedup_{file}.png')

def export_all(df, algo) -> None:
    # export all speedup plots from all exp rates and vice versa for all files
    for exp in df['slowdown'].unique():
        export_all_speedup_plots(df, algo, exp)
    for thread_count in df['threads'].unique():
        export_all_expansion_rate_plots(df, algo, thread_count)
        
def export_astar_speedup_over_slowdown(df) -> None:
    # export all speedup scatter plot for astar over all slowdowns
    exprate = df.loc[:,['file', 'slowdown', 'Astar Expansion Rate']]
    grouped = exprate.groupby('slowdown')['Astar Expansion Rate']
    mean_expansion_rate = grouped.mean()
    std_expansion_rate = grouped.std()

    # Scatter plot with error bars
    plt.figure()
    plt.errorbar(mean_expansion_rate.index, mean_expansion_rate, yerr=std_expansion_rate, fmt='o', color='blue', ecolor='lightblue', elinewidth=1, capsize=4, capthick=1, markersize=5)
    
    plt.xscale('log')
    plt.yscale('log')
    
    xticks = [10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000] # very cool
    plt.xticks(xticks, xticks)
        
    plt.xlabel('Slowdown')
    plt.ylabel('A* Expansion Rate')
    plt.title('A* Expansion Rate over Slowdown')
    plt.savefig('astar_expansion_rate_over_slowdown.png')

def help():
    print("Default variables:")
    print("df: pandas dataframe")
    print()
    print("Functions:")
    print("load_data(glob_str) -> pd.DataFrame")
    print("figure_gen(df, x, y, title, xlabel, ylabel) -> plt.figure")
    print("thread_speedup_plot(df, algo, exp, file) -> plt.figure")
    print("expansion_rate_plot(df, algo, thread_count, file) -> plt.figure")
    print("export_all_expansion_rate_plots(df, algo, thread_count) -> None")
    print("export_all_speedup_plots(df, algo, exp) -> None")
    print()
    print("Imported modules:")
    print("pd: pandas")
    print("plt: matplotlib.pyplot")
    print("np: numpy")
    print("glob: glob")
    print("os: os")
    print("sys: sys")
    print()

if __name__ == "__main__":
    # get input glob string
    import sys
    glob_str = sys.argv[1] # must be in quotes
    df = load_data(glob_str)
    df['Speedup'] = df['astar wall time'] / df['total wall time']
    df['Expansion Rate'] = df['total nodes expanded'] / df['total wall time']
    df['Astar Expansion Rate'] = df['total nodes expanded'] / df['astar wall time']

    df = df.sort_values(by=['algorithm', 'file', 'threads', 'slowdown'])
    
    export_astar_speedup_over_slowdown(df)

    print(f"Data loaded from {len(df)} files")
    print("------------------------------")

    import code
    console = code.InteractiveConsole(locals=globals())
    console.interact(banner="Interactive Plotter Console\nType 'help()' for plotter help")