#include <cassert>
#include <memory>

using handle_t = std::size_t;

struct PrecomputeFlags{
    bool reserved;
    bool completed;

    constexpr PrecomputeFlags():reserved(false),completed(false){}

    static constexpr PrecomputeFlags both_false(){
        PrecomputeFlags retval(false, false);
        return retval;
    }
};


template <typename Node_t, typename Compare>
struct HeapNode{
    Node_t search_node;  // the usual search node: {state, g, f, parent...}
    Node_t[] precomputed_successors;  // array of precomputed successors 
    handle_t handle;
    std::atomic<PrecomputeFlags> flags; // precomputation flags

    constexpr HeapNode(const Node_t& node, handle_t h):search_node(node),precomputed_successors(nullptr),handle(h){
        flags.store(PrecomputeFlags::both_false(), std::memory_order_release);
    };
    
    constexpr ~HeapNode(){
        if(precomputed_successors != nullptr){
            delete[] precomputed_successors;
        }
    }



};

constexpr handle_t parent(handle_t i) const{
    if (i == 0){
        return 0;
    }
    return (i - 1)/2;
}

constexpr handle_t left_child(handle_t i) const{
    return 2*i + 1;
}

constexpr handle_t right_child(handle_t i) const{
    return 2*i + 2;
}

template <typename Node_t, typename Compare>
inline void swap_helper(i, j, HeapNode<Node_t, Compare> * xi, HeapNode<Node_t, Compare> * xj, std::atomic<HeapNode<Node_t, Compare> *> dat_arr[]){
    std::swap(xi->handle, xj->handle);                  // workers don't get to touch handles
    dat_arr[i].store(xj, std::memory_order_relaxed);    // We don't care if two workers see the same pointer twice
    dat_arr[j].store(xi, std::memory_order_relaxed);    // swap conserves the number of elements
}


template <typename Node_t, typename Compare>
class CafeMinBinaryHeap{
    private:
        std::atomic<HeapNode<Node_t, Compare> *> _data[]; // of atomic pointers to nodes
        std::size_t _capacity;  // size of array

        inline void swap(std::size_t i, std::size_t j){
            assert(i >= 0 && i < size);
            assert(j >= 0 && j < size);
            HeapNode<Node_t, Compare> * xi = _data[i].load(std::memory_order_relaxed);
            HeapNode<Node_t, Compare> * xj = _data[j].load(std::memory_order_relaxed);
            swap_helper(i, j, xi, xj, _data);
        }

        inline void pull_up(handle_t i){
            handle_t j = parent(i); 
            HeapNode<Node_t, Compare> * xi = _data[i].load(std::memory_order_relaxed);
            HeapNode<Node_t, Compare> * xj = _data[j].load(std::memory_order_relaxed);
            if(i == 0 || Compare()(xj->search_node, xi->Search_node)){
                return;
            }
            swap_helper(i, j, xi, xj, _data);
            pull_up(j);
        }

        inline void push_down(handle_t i, const std::size_t s){
            handle_t l = left_child(i);
            handle_t r = right_child(i); 
            HeapNode<Node_t, Compare> * xi = _data[i].load(std::memory_order_relaxed);
            handle_t smallest_i = i;
            HeapNode<Node_t, Compare> * smallest = xi;
            if(l < s){
                HeapNode<Node_t, Compare> * xl = arr[l].load(std::memory_order_relaxed);
                if(Compare()(xl->search_node, smallest->Search_node)){
                    smallest = xl;
                    smallest_i = l;
                }
                if(r < s){
                    HeapNode<Node_t, Compare> * xr = arr[l].load(std::memory_order_relaxed);
                    if(Compare()(xr->search_node, smallest->Search_node)){
                        smallest = xr;
                        smallest_i = r;
                    }
                }
            }
            if(smallest != xi){
                swap_helper(i, smallest_i, xi, smallest, arr);
                push_down(smallest_i, s);
            }
        }

        // inline resize(std::size_t new_capacity){
        //     assert(new_capacity >= size);
        //     auto new_data = std::shared_ptr<Flagged_Pointer<HeapNode>>(new Flagged_Pointer<HeapNode>[new_size], std::default_delete<Flagged_Pointer<HeapNode>>());
        //     for (std::size_t i = 0; i < size; i++){
        //         Flagged_Pointer<HeapNode>::copy_data(_data.get()[i], new_data.get()[i]);
        //     }
        //     _capacity = new_capacity;
        //     _data.store(new_data, std::memory_order_release);
        // }

    public:
        std::atomic<std::size_t> size;       // size of heap        

        inline CafeMinBinaryHeap(std::size_t capacity, std::size_t max_nodes):_capacity(capacity),_max_nodes(max_nodes){
            // setup heap before workers!
            _data = new std::atomic<HeapNode<Node_t, Compare> *>[capacity];
            size.store(0, std::memory_order_release);
        }

        inline HeapNode top() const{
            assert(size > 0);
            return *(_data.load(std::memory_order_acquire)->get_pointer());
        }

        inline void pop(){
            std::size_t s = size.load(std::memory_order_relaxed);
            swap(--s, 0);

            size.store(s, std::memory_order_release); 
        }

        inline void push(HeapNode * node){
            std::size_t s = size.load(std::memory_order_relaxed);
            _data[s].store(node, std::memory_order_relaxed);
            pull_up(s, _data);
            size.store(++s, std::memory_order_release); 
        }

};