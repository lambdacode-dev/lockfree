#include <array>
#include <atomic>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

/*
 * The main idea is to turn MPMC into effectively a SPSC, by generating monotonically
 * a read / write slot for an asking reader/writer, and then reader/writer has exlusive
 * access to the assigned unique slot.
 */
template<typename T, int N>
class MPMC {
    static_assert( (N & (N-1)) == 0, "Ring buffer size N has to be power of 2");

    // let slot index i increase monotonically, and
    // use i & mask for actual indexing into the ring buffer
    constexpr static int mask = N - 1;
    std::array<T,N> buffer;

    // assign each slot i with a seq number s.
    // i == s+1 indicates full / ready for reaad.
    // i == s indicates empty / ready for write.
    struct SeqT {
        alignas(std::hardware_destructive_interference_size) std::atomic<int> seq {};
    };
    std::array<SeqT, N> sequence;

    alignas(std::hardware_destructive_interference_size) std::atomic<int> writeidx {};
    alignas(std::hardware_destructive_interference_size) std::atomic<int> readidx {};

public:
    MPMC() {
        for(int i = 0; i < N; ++i) {
            // initialize to empty state / ready for writing for each slot
            sequence[i].seq.store(i, std::memory_order_release);
        }
    }
    void push(T&& item) {
        auto w = writeidx.fetch_add(1, std::memory_order_acq_rel);
        auto i = w & mask;
        while( sequence[i].seq.load(std::memory_order_acquire) != w ) ;
        buffer[i] = std::move(item);
        sequence[i].seq.store(w+1, std::memory_order_release);
    }
    T pop() {
        auto r = readidx.fetch_add(1, std::memory_order_acq_rel);
        auto i = r & mask;
        while( sequence[i].seq.load(std::memory_order_acquire) != r+1 ) ;
        T item = std::move(buffer[i]);

        // mark this ring buffer slot i as ready to write by
        // next write who grab montonical slot r+N
        // note (r+N) & mask = i
        sequence[i].seq.store(r+N, std::memory_order_release);
        return item;
    }
};


int main() {
    MPMC<int, 8> que;
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
