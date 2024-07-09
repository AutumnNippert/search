#pragma once
#include <cstdint>
#include <cassert>
#include <atomic>

template <typename ptr_t>
class Flagged_Pointer{
    private:
        std::atomic<uintptr_t> _data;  
        constexpr static uint64_t pointer_mask = 0xFFFFFFFFFFFFFFFC;
        constexpr static uint64_t working_bit = 0x2;
        constexpr static uint64_t expanded_bit = 0x1;   
    public:
        constexpr Flagged_Pointer(ptr_t pointer){
            _data.store((uintptr_t)pointer, // cast to int type
                        std::memory_order_release); // publish to others
            assert((_data & 0x3) == 0x0); // double check that bottom bits are free for flags
        }
  


        friend constexpr void copy_data(const Flagged_Pointer<ptr_t>& from, Flagged_Pointer<ptr_t>& to){
            to._data = from._data.load(std::memory_order_acquire);
        }

        friend constexpr void swap(Flagged_Pointer<ptr_t>& left, Flagged_Pointer<ptr_t>& right) const{
            left._data.store(right._data.exchange(left._data, std::memory_order_relaxed), 
                                std::memory_order_relaxed);
        }

        constexpr ptr_t get_pointer() const{
            return (ptr_t)(pointer_mask & _data);
        }

        constexpr ~Flagged_Pointer(){
            auto ptr = get_pointer();
            if(ptr != nullptr){
                delete *ptr;
            }
        }

        constexpr bool worker_claim(){
            return working_bit & _data.fetch_or(working_bit,  // try and claim, check if available
                                  std::memory_order_acquire); // pull from others  NOTE, we only need atomic for other workers, 
                                                              // but acquire is so that the main thread can gate the node creation behind a release
        }

        constexpr bool worker_finalize(){
            return expanded_bit & _data.fetch_or(expanded_bit,  // set expansion is precomputed 
                                    std::memory_order_release); // push info to main thread
        }

        constexpr bool isExpanded() const{
            return expanded_bit & _data.load(std::memory_order_acquire);  // Check if expansion has been computed, pulling info first
        }  
};

template <typename ptr_t, typename Compare> 
struct flagged_ptr_cmp{
    bool operator () (const Flagged_Pointer<ptr_t>& x, const Flagged_Pointer<ptr_t>& y) const {
        return Compare()(x.get_pointer(), y.get_pointer());
    }
};