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

def figure_gen(df, x, y, title, xlabel, ylabel) -> plt.figure:
    plt.figure()
    plt.plot(df[x], df[y])
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.title(title)
    return plt

def speedup_plot(df, algo, x, file) -> plt.figure:
    df = df[df['algorithm'] == algo]
    df = df[df['file'] == file]
    return figure_gen(df, x, 'Speedup', f'{algo} Speedup vs {x}', x, 'Speedup')

def help():
    print("Default variables:")
    print("df: pandas dataframe")
    print()
    print("Functions:")
    print("load_data(glob_str) -> pd.DataFrame")
    print("figure_gen(df, x, y, title, xlabel, ylabel) -> plt.figure")
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

    plt.figure()
    plt.scatter(df['threads'], df['Speedup'])
    plt.xlabel('Threads')
    plt.ylabel('Speedup')
    plt.title('Speedup vs Threads')
    #save the plot
    plt.savefig('speedup.png')
    # print(df)

    print(f"Data loaded from {len(df)} files")
    print("------------------------------")

    import code
    console = code.InteractiveConsole(locals=globals())
    console.interact(banner="Interactive Plotter Console\nType 'help()' for plotter help")