#include <array>
#include <atomic>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

/*
 * The main idea is to turn MPMC into effectively a SPSC, by generating monotonically
 * a read / write ticket for an asking reader/writer, and then reader/writer has exlusive
 * access to the assigned unique slot = ticket % buffersize.
 */
template<typename T, int N>
class MPMC {
    static_assert( (N & (N-1)) == 0, "Ring buffer size N has to be power of 2");

    constexpr static int mask = N - 1;
    std::array<T,N> buffer;

    // let ticket i (wirteidx/read_ticket) increase monotonically, and
    // use i & mask for actual indexing into the ring buffer
    alignas(std::hardware_destructive_interference_size) std::atomic<int> write_ticket {};
    alignas(std::hardware_destructive_interference_size) std::atomic<int> read_ticket {};

    // Assign each slot i, holded by ticket t, with a seq number.
    // t == sequence[i] indicates empty / ready for write.
    // t == sequence[i]+1 indicates full / ready for read.
    struct SeqT {
        alignas(std::hardware_destructive_interference_size) std::atomic<int> val {};
    };
    std::array<SeqT, N> sequence;

public:
    MPMC() {
        for(int i = 0; i < N; ++i) {
            // initialize to empty state / ready for writing for each slot
            sequence[i].val.store(i, std::memory_order_release);
        }
    }
    void push(T&& item) {
        auto w = write_ticket.fetch_add(1, std::memory_order_acq_rel);
        auto i = w & mask;
        while( sequence[i].val.load(std::memory_order_acquire) != w ) ;
        buffer[i] = std::move(item);
        sequence[i].val.store(w+1, std::memory_order_release);
    }
    T pop() {
        auto r = read_ticket.fetch_add(1, std::memory_order_acq_rel);
        auto i = r & mask;
        while( sequence[i].val.load(std::memory_order_acquire) != r+1 ) ;
        T item = std::move(buffer[i]);

        // Mark this ring buffer slot i as ready to write for
        // next write whoever grab ticket r+N.
        // Note (r+N) & mask = slot i
        sequence[i].val.store(r+N, std::memory_order_release);
        return item;
    }
};


int main() {
    MPMC<int, 64> que;
    constexpr int M = 128;
    alignas(std::hardware_destructive_interference_size) std::atomic<int> sum {};
    {
        std::vector<std::jthread> producers;
        for(int i = 0; i < M; ++i) {
            producers.push_back( std::jthread([&que] {
                        for(int j = 1; j <= M; ++j) {
                        que.push(std::move(j));
                        }
                        }));
        }
        std::vector<std::jthread> consumers;
        for(int i = 0; i < M; ++i) {
            consumers.push_back( std::jthread([&que, &sum] {
                        for(int j = 1; j <= M; ++j) {
                        auto val = que.pop();
                        sum.fetch_add(val, std::memory_order_acq_rel);
                        }
                        }));
        }
    }
    std::cout << "sum " << sum << "\n";
    return 0;
}
