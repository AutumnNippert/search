#include "cafe_heap.hpp"
#include <ctime>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <chrono>
#include <cmath>
#include <boost/heap/d_ary_heap.hpp>

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

using Queue = boost::heap::d_ary_heap<Node, boost::heap::arity<2>, boost::heap::mutable_<true>, boost::heap::compare<NodeComp>>;
typedef typename Queue::handle_type handle_t2;

int main(){
    std::size_t test_n_pop = 1000000;
    unsigned int ratio_push_to_pop = 5;
    std::srand(std::time(0)); // use current time as seed for random generator
    std::size_t total_pushes = ratio_push_to_pop * test_n_pop;
    std::vector<int> g_values;
    g_values.reserve(total_pushes);
    for (std::size_t i = 0; i < total_pushes; i++){
        g_values.push_back(rand());
    }

    CafeMinBinaryHeap<Node, NodeComp> heap(total_pushes);
    NodePool<Node, NodeComp> node_pool(total_pushes);
    long sum = 0;

    auto search_start_time = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < test_n_pop; i++){
        for(std::size_t j = 0; j < ratio_push_to_pop; j++){
            auto n = node_pool.reserve(1);
            n->search_node.g = g_values[j + ratio_push_to_pop*i];
            heap.push(n);
            // std::cerr << "push: " << *n << "\n";
            // assert(heap.heap_property());

        }
        // std::cerr << heap; 
        sum += heap.top().search_node.g;
        // std::cerr << "pop: " << heap.top() << "\n";
        heap.pop();
        // std::cerr << heap;
        assert(heap.heap_property());
        assert(heap.check_handles());
    }
    auto search_time = std::chrono::high_resolution_clock::now();
    auto search_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(search_time - search_start_time);

    std::cout << "Our heap: " << static_cast<double>(total_pushes)*std::pow(10, 9)/search_duration.count() << " hz\n";

    Queue heap2;
    NodePool<Node, NodeComp> node_pool2(total_pushes);

    auto search2_start_time = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < test_n_pop; i++){
        for(std::size_t j = 0; j < ratio_push_to_pop; j++){
            heap2.push(Node(g_values[j + ratio_push_to_pop*i]));
            // std::cerr << "push: " << *n << "\n";
            // assert(heap.heap_property());

        }
        // std::cerr << heap; 
        sum += heap2.top().g;
        // std::cerr << "pop: " << heap.top() << "\n";
        heap2.pop();
        // std::cerr << heap;
        assert(heap.heap_property());
    }
    auto search2_time = std::chrono::high_resolution_clock::now();
    auto search2_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(search2_time - search2_start_time);

    std::cout << "Boost N-ary heap <n=2>: " << static_cast<double>(total_pushes)*std::pow(10, 9)/search2_duration.count() << " hz\n";


    //std::cout << heap;
    //std::cout << "sum: " << sum << "\n";

}