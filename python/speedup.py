# Goal: Create a speedup plot for the data of the algorithm with --threads-x and the algorithm without threads
# Speedup = algo1[2threads][total_wall_time] / algo2[total_wall_time]

import json
jsonfile = "out.json"
with open(jsonfile) as f:
    data = json.load(f)
    
algo1 = "cafe"
algo2 = "astar-basic"

for key in data["results"]:
    algo1_thread_results = {} #key is thread count, value is total_wall_time
    algo2_thread_results = {}
    # key is filename, value is the results for that file from all algos
    for algo in data["results"][key]:
        if algo1 in algo:
            if "threads" in algo:
                # parse for threads-2-file, threads-4-file, threads-12-file, etc
                # get num of threads
                thread_count = algo.split("-")
                for i in range(len(thread_count)):
                    if "threads" in thread_count[i]:
                        thread_count = thread_count[i+1]
                        break
                algo1_thread_results[thread_count] = data["results"][key][algo]["total wall time"]
            else:
                algo1_thread_results[1] = data["results"][key][algo]["total wall time"]
        if algo2 in algo:
            if "threads" in algo:
                thread_count = algo.split("-")
                for i in range(len(thread_count)):
                    if "threads" in thread_count[i]:
                        thread_count = thread_count[i+1]
                        break
                algo2_thread_results[thread_count] = data["results"][key][algo]["total wall time"]
            else:
                algo2_thread_results[1] = data["results"][key][algo]["total wall time"]
        
    # calculate speedup
    speedup = {}
    for thread_count in algo1_thread_results:
        # if it doesn't exist in an algo, use the 1 thread time
        if thread_count not in algo2_thread_results:
            algo2_thread_results[thread_count] = algo2_thread_results[1]
        if thread_count not in algo1_thread_results:
            algo1_thread_results[thread_count] = algo1_thread_results[1]
            
        #get rid of the quotes
        algo1_thread_results[thread_count] = algo1_thread_results[thread_count].replace("\"", "")
        algo2_thread_results[thread_count] = algo2_thread_results[thread_count].replace("\"", "")
            
        # cast them to floats
        algo1_thread_results[thread_count] = float(algo1_thread_results[thread_count])
        algo2_thread_results[thread_count] = float(algo2_thread_results[thread_count])
            
        speedup[thread_count] = algo2_thread_results[thread_count] / algo1_thread_results[thread_count]
        
    print(speedup)
        
    # create plot using matplotlib
    from matplotlib import pyplot as plt
    x = list(speedup.keys())
    # x as ints
    x = [int(i) for i in x]
    y = list(speedup.values())
    fig = plt.figure()
    ax=fig.add_subplot(111)
    
    ax.plot(x, y)
    ax.set_aspect('auto')
    ax.set_xlabel("Number of threads")
    ax.set_ylabel("Speedup")
    ax.set_title(f"{algo1} vs {algo2} Speedup for file={key}")
    
    ax.grid(True, linestyle='--')
    ax.xaxis.set_major_locator(plt.MaxNLocator(integer=True))
    
    # set bounds
    ax.set_xbound(1, int(max(x)))
    ax.set_ybound(-3, 3)
    
    plt.style.use('ggplot')
    plt.savefig(f"{algo1}_vs_{algo2}_speedup_file={key}.png", dpi=300)
    plt.clf()