/*
 * C++ lock free implementation of Multiple-Producer, Single-Consumer
 */

#include <array>
#include <atomic>
#include <iostream>
#include <new>
#include <thread>

constexpr int N = 32; // buffer size
constexpr int MASK = N-1; 
static_assert( (N & MASK) == 0);
constexpr int M = 10000; // total items for each producer

template<typename T>
class MPSC {
    std::array<T, N> buffer; 
    alignas(std::hardware_destructive_interference_size) std::atomic<unsigned int> tail {};
    alignas(std::hardware_destructive_interference_size) unsigned int head = 0;

    struct alignas(std::hardware_destructive_interference_size) SeqT {
        std::atomic<unsigned int> val;
    };
    std::array<SeqT, N> sequence;

public:
    MPSC() {
        for(int i = 0; i < N; ++i) 
            sequence[i].val.store(i, std::memory_order_release);
    }
    void write(T item) {
        unsigned int p = tail.fetch_add(1, std::memory_order_relaxed);
        unsigned int w = p & MASK;
        while( sequence[w].val.load(std::memory_order_acquire) != p ) {
            std::this_thread::yield();
        }
        buffer[w] = item;
        sequence[w].val.store(p+1, std::memory_order_release);
    }

    int read() {
        unsigned int expect = head+1;
        unsigned int r = head & MASK;
        int item {};
        while(true) {
            if( sequence[r].val.load(std::memory_order_acquire) == expect ) {
                item = buffer[r];
                sequence[r].val.store(head + N, std::memory_order_release);
                head++;
                break;
            } 
            std::this_thread::yield();
        }
        return item;
    }
};

MPSC<int> queue;
std::atomic<int> sum {}; 

void produce() {
    for(int i = 1; i <= M; ++i) {
        queue.write(i);
    }
}

void consume() {
    int s= 0;
    for(int i = 0; i < M*5; ++i) {
        s+= queue.read();
    }
    sum.store(s, std::memory_order_release);
}

int main() {
  {
    std::jthread p1(produce);
    std::jthread p2(produce);
    std::jthread p3(produce);
    std::jthread p4(produce);
    std::jthread p5(produce);
    std::jthread c(consume);
  }
    std::cout << "sum = " << sum.load(std::memory_order_acquire) << "\n";
    return 0;
}
