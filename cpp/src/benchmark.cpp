#include <qbuf/spsc.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace qbuf;

// Simple timer class
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        return duration.count() / 1000.0;
    }

    double elapsed_us() const {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        return duration.count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

// Benchmark: Individual enqueue/dequeue operations
template<std::size_t Capacity>
void benchmark_individual_ops(int iterations, int batch_size) {
    std::cout << "\n=== Benchmark: Individual Operations ===" << std::endl;
    std::cout << "Iterations: " << iterations << ", Batch Size: " << batch_size << std::endl;

    SPSC<int, Capacity> queue;

    // Producer thread
    Timer timer;
    std::thread producer([&queue, iterations, batch_size]() {
        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < batch_size; ++i) {
                while (!queue.try_enqueue(iter * batch_size + i)) {
                    std::this_thread::yield();
                }
            }
        }
    });

    // Consumer thread
    std::thread consumer([&queue, iterations, batch_size]() {
        int total_consumed = 0;
        int target = iterations * batch_size;
        while (total_consumed < target) {
            auto value = queue.try_dequeue();
            if (value.has_value()) {
                ++total_consumed;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    double elapsed = timer.elapsed_us();
    double ops_per_sec = (iterations * batch_size * 2.0) / (elapsed / 1e6);

    std::cout << "Total ops (enq+deq): " << (iterations * batch_size * 2) << std::endl;
    std::cout << "Time: " << std::fixed << std::setprecision(2) << elapsed << " μs" << std::endl;
    std::cout << "Throughput: " << std::scientific << ops_per_sec << " ops/sec" << std::endl;
}

// Benchmark: Bulk enqueue/dequeue operations
template<std::size_t Capacity>
void benchmark_bulk_ops(int iterations, int batch_size) {
    std::cout << "\n=== Benchmark: Bulk Operations ===" << std::endl;
    std::cout << "Iterations: " << iterations << ", Batch Size: " << batch_size << std::endl;

    SPSC<int, Capacity> queue;

    // Producer thread
    Timer timer;
    std::thread producer([&queue, iterations, batch_size]() {
        std::vector<int> batch(batch_size);
        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < batch_size; ++i) {
                batch[i] = iter * batch_size + i;
            }
            std::size_t enqueued = 0;
            while (enqueued < batch_size) {
                enqueued += queue.try_enqueue(batch.data() + enqueued,
                                                     batch_size - enqueued);
                if (enqueued < batch_size) {
                    std::this_thread::yield();
                }
            }
        }
    });

    // Consumer thread
    std::thread consumer([&queue, iterations, batch_size]() {
        std::vector<int> batch(batch_size);
        int total_consumed = 0;
        int target = iterations * batch_size;
        while (total_consumed < target) {
            std::size_t dequeued = queue.try_dequeue(batch.data(), batch_size);
            total_consumed += dequeued;
            if (dequeued == 0) {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    double elapsed = timer.elapsed_us();
    double ops_per_sec = (iterations * batch_size * 2.0) / (elapsed / 1e6);

    std::cout << "Total ops (enq+deq): " << (iterations * batch_size * 2) << std::endl;
    std::cout << "Time: " << std::fixed << std::setprecision(2) << elapsed << " μs" << std::endl;
    std::cout << "Throughput: " << std::scientific << ops_per_sec << " ops/sec" << std::endl;
}

// Benchmark with varying batch sizes
void benchmark_comparison() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║           SPSC Performance Comparison                      ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;

    std::vector<std::pair<int, int>> configs = {
        {100000, 1},    // Many individual ops
        {10000, 10},    // Small batches
        {5000, 20},     // Medium batches
        {1000, 100},    // Large batches
        {500, 200},     // Very large batches
    };

    std::cout << "\n┌─────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Config: 10k total ops, varied batch sizes                   │" << std::endl;
    std::cout << "│ Queue capacity: 64                                          │" << std::endl;
    std::cout << "└─────────────────────────────────────────────────────────────┘" << std::endl;

    for (const auto& [iterations, batch_size] : configs) {
        std::cout << "\n─────────────────────────────────────────────────────────────" << std::endl;
        std::cout << "Configuration: " << iterations << " iterations * " << batch_size << " batch size" << std::endl;
        std::cout << "─────────────────────────────────────────────────────────────" << std::endl;

        benchmark_individual_ops<64>(iterations, batch_size);
        benchmark_bulk_ops<64>(iterations, batch_size);
    }

    std::cout << "\n┌─────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Config: 10k total ops, varied batch sizes                   │" << std::endl;
    std::cout << "│ Queue capacity: 4096                                        │" << std::endl;
    std::cout << "└─────────────────────────────────────────────────────────────┘" << std::endl;

    for (const auto& [iterations, batch_size] : configs) {
        std::cout << "\n─────────────────────────────────────────────────────────────" << std::endl;
        std::cout << "Configuration: " << iterations << " iterations * " << batch_size << " batch size" << std::endl;
        std::cout << "─────────────────────────────────────────────────────────────" << std::endl;

        benchmark_individual_ops<4096>(iterations, batch_size);
        benchmark_bulk_ops<4096>(iterations, batch_size);
    }

    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    Benchmark Complete                      ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
}

int main() {
    std::cout << "SPSC Benchmark: Individual vs Bulk Operations\n" << std::endl;

    benchmark_comparison();

    return 0;
}
