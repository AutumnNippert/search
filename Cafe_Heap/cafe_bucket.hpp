#pragma once
#include "cafe_heap2.hpp"
#include <cassert>
#include <cstddef>
#include <forward_list>
#include <memory>
#include <atomic>
#include <vector>
#include <algorithm>
#include <iostream>


template <typename Node_t, typename Compare>
struct BucketNode{
    using handle_t = BucketNode<Node_t, Compare>* *;
    private: 
        std::atomic<BucketNode<Node_t, Compare> *> precomputed_successors;  // array of precomputed successors
        std::size_t n_precomputed_successors;
        std::atomic<bool> reserved;
        // std::atomic<bool> spoiled;
    public:
        Node_t search_node;  // the usual search node: {state, g, f, parent...}
        handle_t handle;
        ExpansionEntry ee;
        
        constexpr BucketNode(){};

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

        inline void set_completed(BucketNode<Node_t, Compare> * pre_array, std::size_t n){ // Worker thread only
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
            precomputed_successors.store((BucketNode<Node_t, Compare> *)pre_array, std::memory_order_release);
        }

        inline bool is_completed() const{ // For main thread only
            return precomputed_successors.load(std::memory_order_acquire) != nullptr;
        }

        inline std::pair<BucketNode<Node_t, Compare> *, std::size_t> get_successors() const{ // main thread only, check is_completed first
            assert(is_completed());
            return std::make_pair(precomputed_successors.load(std::memory_order_acquire), n_precomputed_successors);
        }

        // inline void mark_fresh(){
        //     spoiled = false;
        //     spoiled.store(false, std::memory_order_relaxed);
        // }

        inline BucketNode(const Node_t& node, handle_t h):search_node(node),handle(h){
            // mark_fresh();
            zero();
        }

        inline friend std::ostream& operator<<(std::ostream& stream, const BucketNode<Node_t, Compare>& heap_node){
            stream << "<" << heap_node.search_node ;
            stream << " " << heap_node.reserved ;
            stream <<" "<< heap_node.is_completed() << " ";
            stream << heap_node.handle << ">";
            return stream;
        }


};

template <typename t>
class IntNode2Bucket{
    constexpr t operator()(t x) const {
        return x;
    }
};

template <typename t>
class FloatNode2Bucket{
    constexpr std::size_t operator()(t x) const {
        return static_cast<std::size_t>(x);
    }
};

template <typename Node_t, typename Compare, typename Node2Bucket>
class CafeMinBbucketList{
    using handle_t = typename BucketNode<Node_t, Compare>::handle_t;
    private:
        std::vector<std::forward_list<BucketNode<Node_t, Compare>*>> _data;
        std::size_t active_bucket;
        const unsigned int recent_queue_size; // size of helper queues
        const unsigned int top_queue_size; // size of helper queues
        BucketNode<Node_t, Compare>* * _heap_top;
        BucketNode<Node_t, Compare>* * _recent_push;
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

        inline void setWorkerTop(){
            workerState.store(WorkerState::heapTop, std::memory_order_release);
        }

        inline void setWorkerRecent(){
            workerState.store(WorkerState::recentPush, std::memory_order_release);
        }

        inline void setWorkerBoth(){
            workerState.store(WorkerState::both, std::memory_order_release);
        }

        inline void push_recent(BucketNode<Node_t, Compare> * n){
            assert(workerState == WorkerState::heapTop || workerState == WorkerState::neither);
            assert(n != nullptr);
            _recent_push[recent_next_index()] = n;
        }

        inline void copyTop(){
            assert(workerState == WorkerState::recentPush || workerState == WorkerState::neither);
            // for(std::size_t i = 1; i <= top_queue_size; i++){
            //     if(i >= size){
            //         // if(_heap_top[i] == nullptr){
            //         //     return;
            //         // }
            //         _heap_top[i-1] = nullptr;
            //     }
            //     else{
            //         _heap_top[i-1] = _data[i];
            //     }
            // }
        }

        inline void find_next(){
            for(std::size_t i = active_bucket; i < _data.size(); i++){
                if(!_data[i].empty()){
                    active_bucket = i;
                    return;
                }
            }
            active_bucket = _data.size();
        }


    public:  
        std::atomic<WorkerState> workerState;     
        inline CafeMinBbucketList(std::size_t capacity, std::size_t recent_queue_capacity, std::size_t top_queue_capacity):recent_queue_size(recent_queue_capacity),top_queue_size(top_queue_capacity),_recent_push_index(0),workerState(WorkerState::neither){
            // setup heap before workers!
            _heap_top = new BucketNode<Node_t, Compare>*[top_queue_capacity];
            _recent_push = new BucketNode<Node_t, Compare>*[recent_queue_capacity];
            for(std::size_t i = 0; i < top_queue_capacity; i++){
                _heap_top[i] = nullptr;
            }
            for(std::size_t i = 0; i < recent_queue_capacity; i++){
                _recent_push[i] = nullptr;
            }
        }

        inline BucketNode<Node_t, Compare> * get(handle_t i) const{
            return *i;
            // assert(size > i);
            // return _data[0];
        }

        inline const BucketNode<Node_t, Compare>& top() const{
            return _data[active_bucket].front();
        }

        inline void pop(){
            find_next();
            setWorkerRecent();
            copyTop();
            setWorkerBoth();
            _data[active_bucket].pop_front();
        }

        inline void batch_recent_push(BucketNode<Node_t, Compare> * nodes, const unsigned int nodes_size){
            setWorkerTop();
            for(unsigned int i = 0; i < std::min(nodes_size, recent_queue_size); i++){
                push_recent(&nodes[i]);
                nodes[i].zero();
            }
            setWorkerBoth();
        }

        inline void push(BucketNode<Node_t, Compare> * node){
            std::size_t bucket = Node2Bucket()(node);
            if( bucket >= _data.size()){
                _data.resize(2*bucket);
            }
            _data[bucket].push_front(node);
            node->handle = &_data[bucket].front();
        }
//// here
        inline void decrease_key(handle_t i, BucketNode<Node_t, Compare> * node){
            node->handle = i;
            _data[i] = node;
            pull_up(i);
        }

        inline bool empty() const{ // MAIN THREAD ONLY
            return size == 0;
        }

        inline friend BucketNode<Node_t, Compare> * fetch_work(const CafeMinBinaryHeap<Node_t, Compare>& heap, WorkerMetadata& mdat){  // for worker
            unsigned int i = 0;
            while (i < heap.top_queue_size+heap.recent_queue_size){
                unsigned int q_ind;
                BucketNode<Node_t, Compare>* * both_q;
                BucketNode<Node_t, Compare>* n;
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