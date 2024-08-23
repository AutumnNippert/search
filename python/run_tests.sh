#!/bin/bash
# Description: Run the cafe experiment with the input list of thread counts and exp values
read -p "Enter the algorithms to run (separated by ' '): " ALGORITHMS
read -p "Enter the thread counts (separated by ' '): " THREADS
read -p "Enter the exp values (separated by ' '): " EXPS

curr_time=$(date +"%Y-%m-%d_%H-%M-%S")
echo "Start time: $curr_time"

for alg in $ALGORITHMS; do
    for exp in $EXPS; do
        for thread in $THREADS; do
            echo "Running $alg with $thread threads and exp $exp"
            python3 algocomp.py ../tiles/15md_solver ../korftest/ $alg -threads $thread -exp $exp
        done
    done
done

end_time=$(date +"%Y-%m-%d_%H-%M-%S")
echo "Start time: $curr_time"
echo "End time: $end_time"