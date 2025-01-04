#pragma once
#include <cassert>
#include <cstddef>
#include <memory>
#include <atomic>
#include <vector>
#include <algorithm>
#include <iostream>

using handle_t = std::size_t;
enum WorkerState {heapTop, recentPush, both, neither};

struct ExpansionEntry{
    WorkerState state;
    unsigned int queue_depth;

    ExpansionEntry() = default;
    ExpansionEntry(WorkerState ws, std::size_t i):state(ws),queue_depth(static_cast<unsigned int>(i)){}
};

struct WorkerMetadata{
    std::vector<std::size_t> heap_top_fetch;
    std::vector<std::size_t> recent_push_fetch;

    WorkerMetadata() = default;
    
    inline void add(const WorkerMetadata& other){
        for(std::size_t i = 0; i < heap_top_fetch.size(); i++){
            heap_top_fetch[i] += other.heap_top_fetch[i];
        }
         for(std::size_t i = 0; i < recent_push_fetch.size(); i++){
            recent_push_fetch[i] += other.recent_push_fetch[i];
        }
    }

    inline void add(const ExpansionEntry& ee){
        if(ee.state == WorkerState::heapTop){
            heap_top_fetch[ee.queue_depth]++;
        }
        else if(ee.state == WorkerState::recentPush){
            recent_push_fetch[ee.queue_depth]++;
        }
    }

    inline std::size_t total() const{
        std::size_t retval = 0;
        for(std::size_t i = 0; i < heap_top_fetch.size(); i++){
            retval += heap_top_fetch[i];
        }
        for(std::size_t i = 0; i < recent_push_fetch.size(); i++){
            retval += recent_push_fetch[i];
        }
        return retval;
    }

    inline friend std::ostream& operator<<(std::ostream& stream, const WorkerMetadata& mdat){
        stream << "heap_top: [";
        for(std::size_t i = 0; i < mdat.heap_top_fetch.size()-1; i++){
            stream << mdat.heap_top_fetch[i] << ", ";
        }
        if(mdat.heap_top_fetch.size() > 0){
            stream << mdat.heap_top_fetch[mdat.heap_top_fetch.size()-1];
        }
        stream << "]\n";

        stream << "heap_recent: [";
        for(std::size_t i = 0; i < mdat.recent_push_fetch.size()-1; i++){
            stream << mdat.recent_push_fetch[i] << ", ";
        }
        if(mdat.recent_push_fetch.size() > 0){
            stream << mdat.recent_push_fetch[mdat.recent_push_fetch.size()-1];
        }
        stream << "]\n";
        return stream;
    }
};



template <typename Node_t, typename Compare>
struct HeapNode{
    private: 
        std::atomic<HeapNode<Node_t, Compare> *> precomputed_successors;  // array of precomputed successors
        std::size_t n_precomputed_successors;
        std::atomic<bool> reserved;
        // std::atomic<bool> spoiled;
    public:
        Node_t search_node;  // the usual search node: {state, g, f, parent...}
        handle_t handle;
        ExpansionEntry ee;
        
        constexpr HeapNode(){};

        inline void zero(){  // Main thread only!
            n_precomputed_successors = 0;
            precomputed_successors.store(nullptr, std::memory_order_relaxed);
            reserved.store(false, std::memory_order_relaxed);  // something else should release
        }

        inline bool reserve(){ // Worker thread only
            bool expected = false;
            if(reserved.load(std::memory_order_relaxed)){
                return false;
            }
            return reserved.compare_exchange_strong(expected, true, std::memory_order_relaxed);
        }

        // inline void spoil(){
        //     if(reserved.load(std::memory_order_relaxed)){  // has been reserved
        //         if(is_completed()){  //has completed
        //             zero();
        //         }
        //         else{  // maybe has not completed
        //             spoiled.store(true, std::memory_order_relaxed);
        //         }
        //     }

        // }

        inline void set_completed(HeapNode<Node_t, Compare> * pre_array, std::size_t n){ // Worker thread only
            n_precomputed_successors = n;
            precomputed_successors.store(pre_array, std::memory_order_release);
            // std::cerr << "Set complete: " << *this << "\n";
            // if(spoiled.load(std::memory_order_relaxed)){
            //     n_precomputed_successors = 0;
            //     precomputed_successors.store(nullptr, std::memory_order_release);
            // }
        }

         inline void set_completed(void * pre_array, std::size_t n){ // dummy, do not use
            n_precomputed_successors = n;
            precomputed_successors.store((HeapNode<Node_t, Compare> *)pre_array, std::memory_order_release);
        }

        inline bool is_completed() const{ // For main thread only
            return precomputed_successors.load(std::memory_order_acquire) != nullptr;
        }

        inline std::pair<HeapNode<Node_t, Compare> *, std::size_t> get_successors() const{ // main thread only, check is_completed first
            assert(is_completed());
            return std::make_pair(precomputed_successors.load(std::memory_order_acquire), n_precomputed_successors);
        }

        // inline void mark_fresh(){
        //     spoiled = false;
        //     spoiled.store(false, std::memory_order_relaxed);
        // }

        inline HeapNode(const Node_t& node, handle_t h):search_node(node),handle(h){
            // mark_fresh();
            zero();
        }

        inline friend std::ostream& operator<<(std::ostream& stream, const HeapNode<Node_t, Compare>& heap_node){
            stream << "<" << heap_node.search_node ;
            stream << " " << heap_node.reserved ;
            stream <<" "<< heap_node.is_completed() << " ";
            stream << heap_node.handle << ">";
            return stream;
        }


};

template <typename Node_t, typename Compare>
class NodePool{ // NOT THREAD SAFE ONE PER THREAD
    private:
        HeapNode<Node_t, Compare> * _nodes;
        const std::size_t _capacity;
        std::size_t _size;
    public:
        NodePool(std::size_t capacity):_capacity(capacity),_size(0){
            _nodes = reinterpret_cast<HeapNode<Node_t, Compare> *>(std::malloc(capacity * sizeof(HeapNode<Node_t, Compare>)));
            if(_nodes == nullptr){
                std::cerr << "Error bad malloc\n";
                exit(-1);
            }
        }

        ~NodePool(){
            free(_nodes);
        }

        HeapNode<Node_t, Compare> * reserve(std::size_t n){
            assert(_size + n <= _capacity);
            HeapNode<Node_t, Compare> * retval = _nodes + _size;
            _size += n;
            return retval;
        } 

        /* TODO: Create Destruct Function */
};

constexpr handle_t parent(handle_t i){
    if (i == 0){
        return 0;
    }
    return (i - 1)/2;
}

constexpr handle_t left_child(handle_t i){
    return 2*i + 1;
}

constexpr handle_t right_child(handle_t i){
    return 2*i + 2;
}

template <typename Node_t, typename Compare>
class CafeMinBinaryHeap{
    private:
        std::size_t _capacity;  // size of array
        std::size_t size;       // size of heap 
        const unsigned int recent_queue_size; // size of helper queues
        const unsigned int top_queue_size; // size of helper queues
        HeapNode<Node_t, Compare>* * _data; // of atomic pointers to nodes
        HeapNode<Node_t, Compare>* * _heap_top;
        HeapNode<Node_t, Compare>* * _recent_push;
        unsigned int _recent_push_index;

        inline unsigned int rnv(unsigned int i, unsigned int queue_size) const{
            return i % queue_size;
            //return i & (2*queue_size - 1);  // circular index 
        }

        inline unsigned int recent_next_index(){
            unsigned int retval = rnv(_recent_push_index+1, recent_queue_size);
            _recent_push_index = retval;
            return retval;
        }

        inline void swap_helper(handle_t i, handle_t j, HeapNode<Node_t, Compare> * xi, HeapNode<Node_t, Compare> * xj){
            // std::cerr << "swap_helper: " << i << " " << j << "\n";
            std::swap(xi->handle, xj->handle);                  // workers don't get to touch handles
            _data[i] = xj;    
            _data[j] = xi;
        }

        inline void swap(handle_t i, handle_t j){
            assert(i < size);
            assert(j < size);
            // std::cerr << "swap: " << i << " " << j << "\n";
            HeapNode<Node_t, Compare> * xi = _data[i];
            HeapNode<Node_t, Compare> * xj = _data[j];
            swap_helper(i, j, xi, xj);
        }

        inline void pull_up(handle_t i){
            handle_t j = parent(i); 
            if (i == 0){
                return;
            }
            HeapNode<Node_t, Compare> * xi = _data[i];
            HeapNode<Node_t, Compare> * xj = _data[j];
            if(Compare()(xi->search_node, xj->search_node)){
                swap_helper(i, j, xi, xj);
                pull_up(j);
            }
        }

        inline void push_down(handle_t i, const std::size_t s){
            handle_t l = left_child(i);
            handle_t r = right_child(i); 
            HeapNode<Node_t, Compare> * xi = _data[i];
            handle_t smallest_i = i;
            HeapNode<Node_t, Compare> * smallest = xi;
            if(l < s){
                HeapNode<Node_t, Compare> * xl = _data[l];
                if(Compare()(xl->search_node, smallest->search_node)){
                    smallest = xl;
                    smallest_i = l;
                }
            }
            if(r < s){
                HeapNode<Node_t, Compare> * xr = _data[r];
                if(Compare()(xr->search_node, smallest->search_node)){
                    smallest = xr;
                    smallest_i = r;
                }
            }
            if(smallest_i != i){
                swap_helper(i, smallest_i, xi, smallest);
                push_down(smallest_i, s);
            }
        }

        inline void setWorkerTop(){
            workerState.store(WorkerState::heapTop, std::memory_order_release);
        }

        inline void setWorkerRecent(){
            workerState.store(WorkerState::recentPush, std::memory_order_release);
        }

        inline void setWorkerBoth(){
            workerState.store(WorkerState::both, std::memory_order_release);
        }

        inline void push_recent(HeapNode<Node_t, Compare> * n){
            assert(workerState == WorkerState::heapTop || workerState == WorkerState::neither);
            assert(n != nullptr);
            _recent_push[recent_next_index()] = n;
        }

        inline void copyTop(){
            assert(workerState == WorkerState::recentPush || workerState == WorkerState::neither);
            for(std::size_t i = 1; i <= top_queue_size; i++){
                if(i >= size){
                    // if(_heap_top[i] == nullptr){
                    //     return;
                    // }
                    _heap_top[i-1] = nullptr;
                }
                else{
                    _heap_top[i-1] = _data[i];
                }
            }
        }


    public:  
        std::atomic<WorkerState> workerState;     
        inline CafeMinBinaryHeap(std::size_t capacity, std::size_t recent_queue_capacity, std::size_t top_queue_capacity):_capacity(capacity),recent_queue_size(recent_queue_capacity),top_queue_size(top_queue_capacity),_recent_push_index(0),workerState(WorkerState::neither){
            // setup heap before workers!
            _data = reinterpret_cast<HeapNode<Node_t, Compare>* *>(std::malloc(capacity * sizeof(HeapNode<Node_t, Compare> *)));
            if(_data == nullptr){
                std::cerr << "Error bad malloc of heap\n";
                exit(-1);
            }
            size = 0;

            _heap_top = new HeapNode<Node_t, Compare>*[top_queue_capacity];
            _recent_push = new HeapNode<Node_t, Compare>*[recent_queue_capacity];
            for(std::size_t i = 0; i < top_queue_capacity; i++){
                _heap_top[i] = nullptr;
            }
            for(std::size_t i = 0; i < recent_queue_capacity; i++){
                _recent_push[i] = nullptr;
            }
        }

        inline ~CafeMinBinaryHeap(){
            free(_data);
        }

        inline HeapNode<Node_t, Compare> * get(handle_t i) const{
            assert(size > i);
            return _data[0];
        }

        inline const HeapNode<Node_t, Compare>& top() const{
            return *get(0);
        }

       

        inline void pop(){
            setWorkerRecent();
            copyTop();
            setWorkerBoth();
            std::size_t s = size;
            swap(--s, 0);
            push_down(0, s);
            size = s; 
        }

        inline void batch_recent_push(HeapNode<Node_t, Compare> * nodes, const unsigned int nodes_size){
            setWorkerTop();
            for(unsigned int i = 0; i < std::min(nodes_size, recent_queue_size); i++){
                push_recent(&nodes[i]);
                nodes[i].zero();
            }
            setWorkerBoth();
        }

        inline void push(HeapNode<Node_t, Compare> * node){
            std::size_t s = size;
            node->handle = s;
            _data[s] = node;
            pull_up(s);
            ++size; 
        }

        inline void decrease_key(handle_t i, HeapNode<Node_t, Compare> * node){
            node->handle = i;
            _data[i] = node;
            pull_up(i);
        }

        inline bool empty() const{ // MAIN THREAD ONLY
            return size == 0;
        }

        inline friend HeapNode<Node_t, Compare> * fetch_work(const CafeMinBinaryHeap<Node_t, Compare>& heap, WorkerMetadata& mdat){  // for worker
            unsigned int i = 0;
            while (i < heap.top_queue_size+heap.recent_queue_size){
                unsigned int q_ind;
                HeapNode<Node_t, Compare>* * both_q;
                HeapNode<Node_t, Compare>* n;
                if(i < heap.top_queue_size){
                    both_q = heap._heap_top;
                    q_ind = heap.rnv(i, heap.top_queue_size);
                }
                else{
                    q_ind = heap.rnv(i, heap.recent_queue_size);
                    both_q = heap._recent_push;
                }
                auto rpi = heap._recent_push_index;
                n = both_q[q_ind];
                // switch(heap.getWorkerState()){
                //     case WorkerState::both:
                //         n = both_q[q_ind];
                //         break;
                //     case WorkerState::heapTop:
                //         n = heap._heap_top[q_ind];
                //         break;
                //     case WorkerState::recentPush:
                //         n = heap._recent_push[q_ind];
                //         break;
                //     default:
                //         n = nullptr;
                // }
                if(n != nullptr && n->reserve()){
                    if(i < heap.top_queue_size){
                        mdat.heap_top_fetch[q_ind]++;
                        n->ee = ExpansionEntry(WorkerState::heapTop, q_ind);
                    }
                    else{
                        std::size_t q_i = (heap.recent_queue_size + (q_ind - rpi)) % heap.recent_queue_size;
                        mdat.recent_push_fetch[q_i]++;
                        n->ee = ExpansionEntry(WorkerState::recentPush, q_i);
                    }
                    return n;
                }
                ++i;
            } 
            return nullptr;
        }

        inline bool heap_property_helper(handle_t i) const{
            if (i >= size){
                return true;
            }
            auto lhs = left_child(i);
            auto rhs = right_child(i);
            bool retval = true;
            if (lhs < size){
                retval &= (!Compare()(_data[lhs]->search_node, _data[i]->search_node));
            }
            if (rhs < size){
                retval &= (!Compare()(_data[rhs]->search_node, _data[i]->search_node));
            }
            if(retval == false){
                std::cerr << *_data[lhs] << " " << *_data[i] << " " << *_data[rhs] << "\n";
            }
            return retval && heap_property_helper(lhs) && heap_property_helper(rhs);
        }

        inline bool heap_property() const{
            return heap_property_helper(0);
        }

        inline bool check_handles() const{
            std::size_t s = size;
            for (std::size_t i = 0; i < s; i++){
                if(_data[i]->handle != i){
                    return false;
                }
            }
            return true;
        }

        inline WorkerState getWorkerState() const{
            return workerState.load(std::memory_order_acquire);
        }

        inline std::size_t get_size() const{
            return size;
        }

        inline friend void dump(std::ostream& stream, const CafeMinBinaryHeap<Node_t, Compare>& heap, std::size_t i, int depth){
            if (i >= heap.size){
                return;
            }
            for(int j = 0; j < depth; j++){
                stream << " ";
            }
            stream << heap._data[i]->handle << ": " << heap._data[i]->search_node << "\n";
            dump(stream, heap, left_child(i), depth + 1);
            dump(stream, heap, right_child(i), depth + 1);
        }

        inline friend std::ostream& operator<<(std::ostream& stream, const CafeMinBinaryHeap<Node_t, Compare>& heap){
            dump(stream, heap, 0, 0);
            // std::size_t s = heap.size.load();
            // for (std::size_t i = 0; i < s; i++){
            //     stream << *heap._data[i] << " ";
            // }
            return stream;
        }
};