#pragma once
#include <cassert>
#include <memory>
#include <atomic>
#include <vector>
#include <iostream>

using handle_t = std::size_t;

template <typename Node_t, typename Compare>
struct HeapNode{
    private: 
        std::atomic<HeapNode<Node_t, Compare> *> precomputed_successors;  // array of precomputed successors
        std::size_t n_precomputed_successors;
        std::atomic<bool> reserved;
    public:
        Node_t search_node;  // the usual search node: {state, g, f, parent...}
        handle_t handle;
        
        constexpr HeapNode(){};

        inline void zero(){  // Main thread only!
            n_precomputed_successors = 0;
            precomputed_successors.store(nullptr, std::memory_order_relaxed);
            reserved.store(false, std::memory_order_relaxed);  // something else should release
        }

        inline int reserve(){ // Worker thread only
            bool expected = false;
            bool reserved_cp = reserved.load(std::memory_order_relaxed);
            auto res = reserved.compare_exchange_strong(expected, true, std::memory_order_relaxed);
            // return 0 if reserved, 1 if not reserved because it was already reserved by someone else, and 2 if reserved between the load and the compare_exchange
            if (res){
                return 0;
            }
            if (reserved_cp){
                return 1;
            }
            return 2;
        }

        inline void set_completed(HeapNode<Node_t, Compare> * pre_array, std::size_t n){ // Worker thread only
            n_precomputed_successors = n;
            precomputed_successors.store(pre_array, std::memory_order_release);
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

        inline HeapNode(const Node_t& node, handle_t h):search_node(node),handle(h){
            zero();
        }

        inline friend std::ostream& operator<<(std::ostream& stream, const HeapNode<Node_t, Compare>& heap_node){
            stream << "<" << heap_node.search_node << " " << heap_node.reserved <<" "<< heap_node.is_completed() << " " << heap_node.handle << ">";
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
        std::atomic<std::size_t> size;       // size of heap 
        std::atomic<HeapNode<Node_t, Compare> *> * _data; // of atomic pointers to nodes
        

        inline void swap_helper(handle_t i, handle_t j, HeapNode<Node_t, Compare> * xi, HeapNode<Node_t, Compare> * xj){
            std::swap(xi->handle, xj->handle);                  // workers don't get to touch handles
            _data[i].store(xj, std::memory_order_relaxed);    // We don't care if two workers see the same pointer twice
            _data[j].store(xi, std::memory_order_relaxed);    // swap conserves the number of elements
        }

        inline void swap(handle_t i, handle_t j){
            assert(i < size);
            assert(j < size);
            HeapNode<Node_t, Compare> * xi = _data[i].load(std::memory_order_relaxed);
            HeapNode<Node_t, Compare> * xj = _data[j].load(std::memory_order_relaxed);
            swap_helper(i, j, xi, xj);
        }

        inline void pull_up(handle_t i){
            handle_t j = parent(i); 
            if (i == 0){
                return;
            }
            HeapNode<Node_t, Compare> * xi = _data[i].load(std::memory_order_relaxed);
            HeapNode<Node_t, Compare> * xj = _data[j].load(std::memory_order_relaxed);
            if(Compare()(xi->search_node, xj->search_node)){
                swap_helper(i, j, xi, xj);
                pull_up(j);
            }
            
        }

        inline void push_down(handle_t i, const std::size_t s){
            handle_t l = left_child(i);
            handle_t r = right_child(i); 
            HeapNode<Node_t, Compare> * xi = _data[i].load(std::memory_order_relaxed);
            handle_t smallest_i = i;
            HeapNode<Node_t, Compare> * smallest = xi;
            if(l < s){
                HeapNode<Node_t, Compare> * xl = _data[l].load(std::memory_order_relaxed);
                if(Compare()(xl->search_node, smallest->search_node)){
                    smallest = xl;
                    smallest_i = l;
                }
            }
            if(r < s){
                HeapNode<Node_t, Compare> * xr = _data[r].load(std::memory_order_relaxed);
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

    public:       
        inline CafeMinBinaryHeap(std::size_t capacity):_capacity(capacity){
            // setup heap before workers!
            _data = reinterpret_cast<std::atomic<HeapNode<Node_t, Compare> *> *>(std::malloc(capacity * sizeof(std::atomic<HeapNode<Node_t, Compare> *>)));
            if(_data == nullptr){
                std::cerr << "Error bad malloc\n";
                exit(-1);
            }
            size.store(0, std::memory_order_release);
        }

        inline ~CafeMinBinaryHeap(){
            free(_data);
        }

        inline HeapNode<Node_t, Compare> * get(handle_t i) const{
            assert(size > i);
            return _data[0].load(std::memory_order_acquire);
        }

        inline const HeapNode<Node_t, Compare>& top() const{
            return *get(0);
        }

        inline void pop(){
            std::size_t s = size.load(std::memory_order_relaxed);
            swap(--s, 0);
            push_down(0, s);
            size.store(s, std::memory_order_release); 
        }

        inline void push(HeapNode<Node_t, Compare> * node){
            std::size_t s = size.load(std::memory_order_relaxed);
            node->handle = s;
            node->zero();
            _data[s].store(node, std::memory_order_release);
            pull_up(s);
            size.store(++s, std::memory_order_release); 
        }

        inline void decrease_key(handle_t i, HeapNode<Node_t, Compare> * node){
            node->handle = i;
            node->zero();
            _data[i].store(node, std::memory_order_release);
            pull_up(i);
        }

        inline bool empty() const{ // MAIN THREAD ONLY
            return size.load(std::memory_order_relaxed) == 0;
        }

        inline HeapNode<Node_t, Compare> * fetch_work(size_t start_depth) const{  // for worker
            std::size_t s = size.load(std::memory_order_acquire);
            std::size_t depth = start_depth;  // start at the depth, and go up
            // while(depth < s){
                for (std::size_t i = 0; i < s; i++){
                    // if (i >= s){ // it was segfaulting, This shouldn't be necessary but i guess the open list must change size during the search
                    //     return nullptr;
                    // }
                    HeapNode<Node_t, Compare> * n = _data[i].load(std::memory_order_acquire);
                    if(n->reserve()){
                        return n;
                    }
                }
                // s = size.load(std::memory_order_acquire);
                // depth*=2;
            // }
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
                retval &= (!Compare()(_data[lhs].load()->search_node, _data[i].load()->search_node));
            }
            if (rhs < size){
                retval &= (!Compare()(_data[rhs].load()->search_node, _data[i].load()->search_node));
            }
            if(retval == false){
                std::cerr << *_data[lhs].load() << " " << *_data[i].load() << " " << *_data[rhs].load() << "\n";
            }
            return retval && heap_property_helper(lhs) && heap_property_helper(rhs);
        }

        inline bool heap_property() const{
            return heap_property_helper(0);
        }

        inline bool check_handles() const{
            std::size_t s = size;
            for (std::size_t i = 0; i < s; i++){
                if(_data[i].load()->handle != i){
                    return false;
                }
            }
            return true;
        }

        inline friend void dump(std::ostream& stream, const CafeMinBinaryHeap<Node_t, Compare>& heap, std::size_t i, int depth){
            if (i >= heap.size){
                return;
            }
            for(int j = 0; j < depth; j++){
                stream << " ";
            }
            stream << heap._data[i].load()->search_node << "\n";
            dump(stream, heap, left_child(i), depth + 1);
            dump(stream, heap, right_child(i), depth + 1);
        }

        inline size_t get_size() const{
            return size.load();
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