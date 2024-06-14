#include <cassert>
#include <boost/heap/d_ary_heap.hpp>
#include "flagged_ptr.hpp"
#include "ordered_circular_queue.hpp"

template <typename Node_t, typename Compare>
class Cafe_Heap{
    private:
        using Queue = boost::heap::d_ary_heap<Node_t *, 
                                boost::heap::arity<2>, 
                                boost::heap::mutable_<true>,
                                boost::heap::compare<flagged_ptr_cmp<Node_t *, Compare>>>;
        Ordered_Circular_Queue<Flagged_Pointer<Node_t *>, flagged_ptr_cmp<Node_t *, Compare>> _head_queue;
        Queue _tail_queue;
    public:
        
};