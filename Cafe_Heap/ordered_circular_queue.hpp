#pragma once
#include <cassert>
#include <iostream>

template <typename Elem_t, typename Compare>
class Ordered_Circular_Queue{
    private:
        const std::size_t _capacity;
        std::size_t _head;
        Elem_t * _data;

        std::size_t index_of(std::size_t i) const{
            std::size_t ind = (_head + i); // index
            return ind & (2*_capacity - 1);  // circular index 
        }

        inline void swap(std::size_t i, std::size_t j){
            std::swap(_data[i], _data[j]);
            _data[i]->handle = i;
            _data[j]->handle = j;
        }

        inline void bubble(){
            for (std::size_t i = length - 1; i > 0; i--){
                std::size_t j = i - 1;
                std::size_t i_c = index_of(i);
                std::size_t j_c = index_of(j);
                if (Compare()(_data[i_c], _data[j_c])){  //check if vs else
                    swap(i_c, j_c);
                }
                else{
                    return;
                }
            }
        }

    public:
        std::size_t length;
        inline Ordered_Circular_Queue(std::size_t capacity):_capacity(capacity),_head(0),length(0){
            assert(capacity % 2 == 0);
            _data = new Elem_t[capacity];
        }

        inline ~Ordered_Circular_Queue(){
            delete[] _data;
        }

        inline Elem_t peek() const{
            return _data[_head];
        }

        inline void pop(){
            --length;
            _head = index_of(++_head);
        }

        inline Elem_t peek_handle(std::size_t i) const{
            return _data[i];
        }

        inline Elem_t peek_tail() const{
            return peek_handle(index_of(length - 1));
        }

        inline std::pair<bool, Elem_t> push(Elem_t x){
            // retval is (ejected?, ejected elem)
            if (length == _capacity){
                Elem_t retval = peek_tail();
                if (Compare()(x, retval)){
                    std::size_t i = index_of(length - 1);
                    _data[i] = x;
                    x->handle = i;
                    bubble();
                    return std::make_pair(true, retval);
                }
            }
            else{
                std::size_t i = index_of(length);
                _data[i] = x;
                x->handle = i;
                ++length;
                bubble();
            }
            return std::make_pair(false, nullptr); 
        }

        inline friend std::ostream& operator<< (std::ostream& stream, const Ordered_Circular_Queue& queue){
            for (std::size_t i = 0; i < queue.length; i++){
                std::size_t i_c = queue.index_of(i);
                stream << *queue._data[i_c] << " ";
            }
            return stream;
        } 
};