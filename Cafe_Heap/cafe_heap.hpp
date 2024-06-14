#include <cassert>
#include <boost/heap/d_ary_heap.hpp>
#include "flagged_ptr.hpp"
#include "ordered_circular_queue.hpp"

template <typename Node_t, typename Compare>
class Cafe_Heap{
    private:
        using Queue = boost::heap::d_ary_heap<Flagged_Pointer<Node_t *>, 
                                boost::heap::arity<2>, 
                                boost::heap::mutable_<true>,
                                boost::heap::compare<flagged_ptr_cmp<Node_t *, Compare>>>;
        Ordered_Circular_Queue<Flagged_Pointer<Node_t *>, flagged_ptr_cmp<Node_t *, Compare>> _head_queue;
        Queue _tail_queue;
    public:
        inline Node_t * peek() const{
            return _head_queue.peek().get_pointer();
        }

        inline void pop(){
            _head_queue.pop();
            _head_queue.push(_tail_queue.top());
            _tail_queue.pop();
        }        

        inline void push(Node_t * x){
            Flagged_Pointer<Node_t> fx(x);
            if(_head_queue.length < _head_queue._capacity){
                _head_queue.push(fx);
            }
            else if(flagged_ptr_cmp<Node_t *, Compare>(_head_queue.peek_tail(), fx)){
                auto holder = _head_queue.push(fx);
                _tail_queue.push(holder);
            }
            else{
                _tail_queue.push(holder);
            }
        }
};