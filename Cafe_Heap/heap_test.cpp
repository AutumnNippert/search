#include "cafe_heap2.hpp"
#include <cstddef>
#include <ctime>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <cmath>
#include <boost/heap/d_ary_heap.hpp>
#include <thread>
#include <stop_token>
 
struct Node{
    int g;
    Node() = default;
    Node(int gValue):g(gValue){}

    inline friend std::ostream& operator<<(std::ostream& stream, const Node& n){
        stream << n.g;
        return stream;
    }
};

struct NodeComp{
    bool operator()(const Node& lhs, const Node& rhs) const{
        return lhs.g < rhs.g;
    }
};

void thread_speculate(std::stop_token token, CafeMinBinaryHeap<Node, NodeComp>& open, WorkerMetadata & mdat, std::size_t slowdown){
        // std::size_t sexp = 0;
        while(!token.stop_requested()){
			// auto start = std::chrono::high_resolution_clock::now();
			HeapNode<Node, NodeComp> *hn = fetch_work(open, mdat);
			if(hn == nullptr){
				continue;
			}
			waste_time(slowdown);
			hn->set_completed((Node *)1, 0);
            // sexp++;
		}
        // std::cerr << "Speculative Expansions: " << sexp << "\n";
	}

using Queue = boost::heap::d_ary_heap<Node, boost::heap::arity<2>, boost::heap::mutable_<true>, boost::heap::compare<NodeComp>>;
typedef typename Queue::handle_type handle_t2;

int main(int argc, char* argv[]){
    if(argc != 3){
        std::cerr << "Usage ...\n"; 
    }
    std::size_t s_i = 0;
    volatile std::size_t * sum_i = &s_i;
    std::size_t slowdown = std::stof(argv[1]);
    int workers = std::stof(argv[2]);
    std::vector<std::jthread> threads;
    std::stop_source stop_source;
    std::vector<WorkerMetadata> wmdat;
    wmdat.resize(workers-1);

    std::size_t test_n_pop = 1000000;
    std::size_t normal_expansions = 0;
    std::size_t spec_exp_used = 0;
    unsigned int ratio_push_to_pop = 5;
    std::srand(std::time(0)); // use current time as seed for random generator
    std::size_t total_pushes = ratio_push_to_pop * test_n_pop;
    std::vector<int> g_values;
    g_values.reserve(total_pushes);
    for (std::size_t i = 0; i < total_pushes; i++){
        g_values.push_back((rand() % (total_pushes/10)) + 10*i);
        //g_values.push_back(i);
    }

    CafeMinBinaryHeap<Node, NodeComp> heap(total_pushes, workers);
    for (int i = 1; i < workers; i++){
        wmdat[i-1].heap_top_fetch.resize(workers);
        wmdat[i-1].recent_push_fetch.resize(workers);
        threads.emplace_back(&thread_speculate, stop_source.get_token(), std::ref(heap), std::ref(wmdat[i-1]), slowdown);
    }
    NodePool<Node, NodeComp> node_pool(total_pushes);
    long sum = 0;

    auto search_start_time = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < test_n_pop; i++){
        auto succ = node_pool.reserve(ratio_push_to_pop);
        heap.batch_recent_push(succ, ratio_push_to_pop);
        for(std::size_t j = 0; j < ratio_push_to_pop; j++){
            auto n = succ + j;
            n->search_node.g = g_values[j + ratio_push_to_pop*i];
            heap.push(n);
            // std::cerr << "push: " << *n << "\n";
            // std::cerr << heap;
            // assert(heap.heap_property());
        }
        // std::cerr << heap; 
        auto& n = heap.top();
        sum += n.search_node.g;
        // std::cerr << heap;
        // std::cerr << "pop: " << heap.top() << "\n";
        heap.pop();
        if(!n.is_completed()){
            *sum_i += waste_time(slowdown);
            normal_expansions++;
        }
        else{
            spec_exp_used++;
        }
        // std::cerr << heap;
        assert(heap.heap_property());
        assert(heap.check_handles());
    }
    std::cerr << "Normal expansions: " << normal_expansions << "\n";
    WorkerMetadata total_mdat;
    total_mdat.heap_top_fetch.resize(workers);
    total_mdat.recent_push_fetch.resize(workers);
    for (int i = 1; i < workers; i++){
        std::cerr << "Worker " << i << ": ";
        std::cerr << wmdat[i-1].total() << "\n";
        total_mdat.add(wmdat[i-1]);
    }
    std::cerr << "Spec exp dist: " << total_mdat;
    std::cerr << "Total spec exp: " << total_mdat.total() << "\n";
    std::cerr << "Spec exp used: " << spec_exp_used << "\n";
    stop_source.request_stop();
    for (auto& thread: threads){
        thread.join();
    }
    auto search_time = std::chrono::high_resolution_clock::now();
    auto search_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(search_time - search_start_time);

    std::cout << "Our heap: " << static_cast<double>(total_pushes)*std::pow(10, 9)/search_duration.count() << " hz\n";

    // Queue heap2;
    // NodePool<Node, NodeComp> node_pool2(total_pushes);

    // auto search2_start_time = std::chrono::high_resolution_clock::now();
    // for (std::size_t i = 0; i < test_n_pop; i++){
    //     for(std::size_t j = 0; j < ratio_push_to_pop; j++){
    //         heap2.push(Node(g_values[j + ratio_push_to_pop*i]));
    //         // std::cerr << "push: " << *n << "\n";
    //         // assert(heap.heap_property());

    //     }
    //     // std::cerr << heap; 
    //     sum += heap2.top().g;
    //     // std::cerr << "pop: " << heap.top() << "\n";
    //     heap2.pop();
    //     waste_time(slowdown);
    //     // std::cerr << heap;
    //     assert(heap.heap_property());
    // }
    // auto search2_time = std::chrono::high_resolution_clock::now();
    // auto search2_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(search2_time - search2_start_time);

    // std::cout << "Boost N-ary heap <n=2>: " << static_cast<double>(total_pushes)*std::pow(10, 9)/search2_duration.count() << " hz\n";

    

}