/*
 * C++ lock free implementation of Single-Producer, Single-Consumer
 */

#include <array>
#include <atomic>
#include <iostream>
#include <new>
#include <thread>

constexpr int N = 100; // buffer size: actually only holds N-1 items
constexpr int M = 10000; // total items
using Item = int;
std::array<Item, N> buffer; // current items at [readidx, writeidx)
alignas(std::hardware_destructive_interference_size) std::atomic<int> readidx{}, writeidx {};
std::atomic<int> sum {}; 

inline bool full(int r, int w) {
    return (w+1) % N == r;
}

inline bool empty(int r, int w) {
    return w == r;
}

inline void write(Item item) {
    int w = writeidx.load(std::memory_order_relaxed), r;
    do {
        r = readidx.load(std::memory_order_acquire);
    }
    while(full(r, w));

    buffer[w] = item;
    writeidx.store((w+1) % N, std::memory_order_release);
}

inline int read() {
    int w, r = readidx.load(std::memory_order_relaxed);
    do {
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
    for(int i = 0; i < M; ++i) {
        s+= read();
    }
    sum.store(s, std::memory_order_release);
}

int main() {
    {
        std::jthread p(produce);
        std::jthread c(consume);
    }
    std::cout << "sum = " << sum.load(std::memory_order_acquire) << "\n";
    return 0;
}
