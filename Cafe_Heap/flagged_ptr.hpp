#pragma once
#include <cstdint>
#include <cassert>
#include <atomic>

template <class ptr_t>
class Flagged_Pointer{
    private:
        std::atomic<uintptr_t> _data;  
        constexpr static uint64_t pointer_mask = 0xFFFFFFFFFFFFFFFC;
        constexpr static uint64_t working_bit = 0x2;
        constexpr static uint64_t expanded_bit = 0x1;   
    public:
        constexpr Flagged_Pointer(ptr_t pointer){
            _data = (uintptr_t)pointer; // cast to int type
            assert((_data & 0x3) == 0x0); // double check that bottom bits are free for flags
        }

        constexpr ptr_t get_pointer() const{
            return (ptr_t)(pointer_mask & _data);
        }

        constexpr bool claim(){
            return _data.fetch_or(working_bit) & working_bit;
        }

        constexpr bool finalize(){
            return _data.fetch_or(expanded_bit) & expanded_bit;
        }

        constexpr bool expanded() const{
            return _data.load() & expanded_bit;
        }  
};