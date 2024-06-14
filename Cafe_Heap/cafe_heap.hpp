#include <cassert>
#include <boost/heap/d_ary_heap.hpp>
#include "flagged_ptr.hpp"

template <class Node_t>
class Cafe_Heap{
    private:
        using Queue = boost::heap::d_ary_heap<Node_t *, 
                                boost::heap::arity<2>, 
                                boost::heap::mutable_<true>,
                                boost::heap::compare<std::greater<Node_t *>>>;
        Flagged_Pointer<Node_t *> * _head_queue;
        std::size_t _head_capacity
        Queue _tail_queue;
    public:


};