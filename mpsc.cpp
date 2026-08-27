#include <array>
#include <atomic>
#include <new>
#include <numeric>

template<typename T, int N>
class MPSC {
    static_assert((N & (N-1)) == 0, "MPSC ring buffer size has to be power of 2");
    constexpr static int mask = N - 1;
    std::array<T, N> buffer;
    std::array<std::atomic<int>, N> sequence;
    alignas(std::hardware_destructive_interference_size) std::atomic<int> readidx {0};
    alignas(std::hardware_destructive_interference_size) std::atomic<int> writeidx {0};

public:
    MPSC() {
        for(int i = 0; i < N; ++i)
            sequence.store(i, std::memory_order_release);
    }
    void push(T&& item) {
        auto w = writeidx.fetch_add(1, std::memory_order_acq_rel);
        auto i = w & mask;
        while( sequence[i].load(std::memory_order_acquire) != w ) ;
        buffer[i] = std::move(item);
        sequence[i].store(w+1, std::memory_order_release);
    }
    T pop() {
        auto r = readidx.fetch_add(1, std::memory_order_relaxed);
        auto i = r & mask;
        while( sequence[i].load(std::memory_order_acquire) != r+1 ) ;
        T temp = std::move(buffer[i]);
        sequence[i].store(r+N, std::memory_order_release);
        return temp;
    }
};
