/*
 * C++ lock free implementation of Multiple-Producer, Single-Consumer
 */

#include <array>
#include <atomic>
#include <iostream>
#include <new>
#include <thread>

constexpr int N = 10; // buffer size: actually only hold N-1 items
constexpr int M = 100; // total items
using Item = int;
std::array<Item, N> buffer; // current items at [readidx, writeidx)
alignas(std::hardware_destructive_interference_size) std::atomic<int> readidx{}, writeidx {};
alignas(std::hardware_destructive_interference_size) std::atomic<bool> writing_in_progress {};
std::atomic<int> sum {}; 

inline bool full(int r, int w) {
    return (w+1) % N == r;
}

inline bool empty(int r, int w) {
    return w == r;
}

inline void write(Item item) {
    for(bool done = false; !done; ) {
        int r = readidx.load(std::memory_order_acquire);
        int w = writeidx.load(std::memory_order_acquire);
        if(!full(r, w)) {
            bool writing = false;
            if( writing_in_progress.compare_exchange_strong(writing, true, std::memory_order_acq_rel) ) {
                if( writeidx.compare_exchange_strong(w, (w+1) % N, std::memory_order_acq_rel) ) {
                    buffer[w] = item;
                    done = true;
                }
                writing_in_progress.store(false, std::memory_order_release);
            }
        }
    }
}

inline int read() {
    int w, r = readidx.load(std::memory_order_relaxed);
    do {
        w = r;
        if ( ! writing_in_progress.load(std::memory_order_acquire) )
            w = writeidx.load(std::memory_order_acquire);
    }
    while(empty(r,w));

    int item = buffer[r];
    readidx.store((r+1) % N, std::memory_order_release);
    return item;
}

void produce() {
    for(int i = 1; i <= M; ++i) {
        write(i);
    }
}

void consume() {
    int s= 0;
    for(int i = 0; i < M*3; ++i) {
        s+= read();
    }
    sum.store(s, std::memory_order_release);
}

int main() {
    {
        std::jthread p1(produce);
        std::jthread p2(produce);
        std::jthread p3(produce);
        std::jthread c(consume);
    }
    std::cout << "sum = " << sum.load(std::memory_order_acquire) << "\n";
    return 0;
}
