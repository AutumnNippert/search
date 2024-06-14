#include <iostream>
#include <vector>
#include <limits>
#include "flagged_ptr.hpp"
#include "Ordered_Circular_Queue.hpp"

struct DummyNode{
    int x;
    std::size_t handle;
    DummyNode():x(0),handle(std::numeric_limits<std::size_t>::max()){};
    DummyNode(int _x): x(_x),handle(std::numeric_limits<std::size_t>::max()){};

    inline friend std::ostream& operator<< (std::ostream& stream, const DummyNode& n){
        stream << n.x;
        return stream;
    } 
};

struct int_ptr_cmp{
    bool operator () (const DummyNode * x, const DummyNode * y) const {
        return x->x < y->x;
    }
};

int main() { 
    for (int i = 0; i < 1000; i++){
        int * x = new int;
        Flagged_Pointer y(x);
        assert(y.get_pointer() == x);
        assert(y.claim() == false);
        assert(y.claim() == true);
        assert(y.expanded() == false);
        y.finalize();
        assert(y.expanded());
        delete x;
    }
    Ordered_Circular_Queue<DummyNode *,  int_ptr_cmp> queue(4);
    std::vector<DummyNode> a;
    for (int i = 0; i < 10; i++){
        a.push_back(i);
    }
    std::cerr << "Inserting 1, 2, 3\n";
    std::cerr << queue << "\n";
    queue.push(&a[0] + 1);
    std::cerr << queue << "\n";
    queue.push(&a[0] + 2);
    std::cerr << queue << "\n";
    queue.push(&a[0] + 3);
    std::cerr << queue << "\n";
    queue.push(&a[0] + 4);
    std::cerr << queue << "\n";
    queue.push(&a[0] + 5);
    std::cerr << queue << "\n";
    queue.push(&a[0] + 6);
    std::cerr << queue << "\n";
    queue.push(&a[0] + 0);
    std::cerr << queue << "\n";
}