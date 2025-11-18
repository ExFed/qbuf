#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <qbuf/mmap_spsc.hpp>
#include <qbuf/mutex_queue.hpp>
#include <qbuf/spsc.hpp>
#include <string>
#include <thread>
#include <vector>

using namespace qbuf;

// Structure to hold benchmark results
struct BenchmarkResult {
    std::string queue_type;
    std::string operation_type;
    std::size_t capacity;
    int iterations;
    int batch_size;
    double elapsed_us;
    double ops_per_sec;
};

// Simple timer class
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) { }

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
template <std::size_t Capacity>
BenchmarkResult benchmark_individual_ops(int iterations, int batch_size) {
    std::cout << "\n=== Benchmark: Individual Operations ===" << std::endl;
    std::cout << "Iterations: " << iterations << ", Batch Size: " << batch_size << std::endl;

    auto [sink, source] = SPSC<int, Capacity>::make_queue();

    // Producer thread
    Timer timer;
    std::thread producer([sink = std::move(sink), iterations, batch_size]() mutable {
        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < batch_size; ++i) {
                while (!sink.try_enqueue(iter * batch_size + i)) {
                    std::this_thread::yield();
                }
            }
        }
    });

    // Consumer thread
    std::thread consumer([source = std::move(source), iterations, batch_size]() mutable {
        int total_consumed = 0;
        int target = iterations * batch_size;
        while (total_consumed < target) {
            auto value = source.try_dequeue();
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

    return { "SPSC", "Individual", Capacity, iterations, batch_size, elapsed, ops_per_sec };
}

// Benchmark: Bulk enqueue/dequeue operations
template <std::size_t Capacity>
BenchmarkResult benchmark_bulk_ops(int iterations, int batch_size) {
    std::cout << "\n=== Benchmark: Bulk Operations ===" << std::endl;
    std::cout << "Iterations: " << iterations << ", Batch Size: " << batch_size << std::endl;

    auto [sink, source] = SPSC<int, Capacity>::make_queue();

    // Producer thread
    Timer timer;
    std::thread producer([sink = std::move(sink), iterations, batch_size]() mutable {
        std::vector<int> batch(batch_size);
        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < batch_size; ++i) {
                batch[i] = iter * batch_size + i;
            }
            std::size_t enqueued = 0;
            while (enqueued < batch_size) {
                enqueued += sink.try_enqueue(batch.data() + enqueued, batch_size - enqueued);
                if (enqueued < batch_size) {
                    std::this_thread::yield();
                }
            }
        }
    });

    // Consumer thread
    std::thread consumer([source = std::move(source), iterations, batch_size]() mutable {
        std::vector<int> batch(batch_size);
        int total_consumed = 0;
        int target = iterations * batch_size;
        while (total_consumed < target) {
            std::size_t dequeued = source.try_dequeue(batch.data(), batch_size);
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

    return { "SPSC", "Bulk", Capacity, iterations, batch_size, elapsed, ops_per_sec };
}

// Benchmark: Individual enqueue/dequeue operations (MutexQueue)
template <std::size_t Capacity>
BenchmarkResult benchmark_individual_ops_mutex(int iterations, int batch_size) {
    std::cout << "\n=== Benchmark: Individual Operations (MutexQueue) ===" << std::endl;
    std::cout << "Iterations: " << iterations << ", Batch Size: " << batch_size << std::endl;

    auto [sink, source] = MutexQueue<int, Capacity>::make_queue();

    // Producer thread
    Timer timer;
    std::thread producer([sink = std::move(sink), iterations, batch_size]() mutable {
        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < batch_size; ++i) {
                while (!sink.try_enqueue(iter * batch_size + i)) {
                    std::this_thread::yield();
                }
            }
        }
    });

    // Consumer thread
    std::thread consumer([source = std::move(source), iterations, batch_size]() mutable {
        int total_consumed = 0;
        int target = iterations * batch_size;
        while (total_consumed < target) {
            auto value = source.try_dequeue();
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

    return { "MutexQueue", "Individual", Capacity, iterations, batch_size, elapsed, ops_per_sec };
}

// Benchmark: Bulk enqueue/dequeue operations (MutexQueue)
template <std::size_t Capacity>
BenchmarkResult benchmark_bulk_ops_mutex(int iterations, int batch_size) {
    std::cout << "\n=== Benchmark: Bulk Operations (MutexQueue) ===" << std::endl;
    std::cout << "Iterations: " << iterations << ", Batch Size: " << batch_size << std::endl;

    auto [sink, source] = MutexQueue<int, Capacity>::make_queue();

    // Producer thread
    Timer timer;
    std::thread producer([sink = std::move(sink), iterations, batch_size]() mutable {
        std::vector<int> batch(batch_size);
        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < batch_size; ++i) {
                batch[i] = iter * batch_size + i;
            }
            std::size_t enqueued = 0;
            while (enqueued < batch_size) {
                enqueued += sink.try_enqueue(batch.data() + enqueued, batch_size - enqueued);
                if (enqueued < batch_size) {
                    std::this_thread::yield();
                }
            }
        }
    });

    // Consumer thread
    std::thread consumer([source = std::move(source), iterations, batch_size]() mutable {
        std::vector<int> batch(batch_size);
        int total_consumed = 0;
        int target = iterations * batch_size;
        while (total_consumed < target) {
            std::size_t dequeued = source.try_dequeue(batch.data(), batch_size);
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

    return { "MutexQueue", "Bulk", Capacity, iterations, batch_size, elapsed, ops_per_sec };
}

// Benchmark: Individual enqueue/dequeue operations (MmapSPSC)
template <std::size_t Capacity>
BenchmarkResult benchmark_individual_ops_mmap(int iterations, int batch_size) {
    std::cout << "\n=== Benchmark: Individual Operations (MmapSPSC) ===" << std::endl;
    std::cout << "Iterations: " << iterations << ", Batch Size: " << batch_size << std::endl;

    auto [sink, source] = MmapSPSC<int, Capacity>::create();

    // Producer thread
    Timer timer;
    std::thread producer([sink = std::move(sink), iterations, batch_size]() mutable {
        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < batch_size; ++i) {
                while (!sink.try_enqueue(iter * batch_size + i)) {
                    std::this_thread::yield();
                }
            }
        }
    });

    // Consumer thread
    std::thread consumer([source = std::move(source), iterations, batch_size]() mutable {
        int total_consumed = 0;
        int target = iterations * batch_size;
        while (total_consumed < target) {
            auto value = source.try_dequeue();
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

    return { "MmapSPSC", "Individual", Capacity, iterations, batch_size, elapsed, ops_per_sec };
}

// Benchmark: Bulk enqueue/dequeue operations (MmapSPSC)
template <std::size_t Capacity>
BenchmarkResult benchmark_bulk_ops_mmap(int iterations, int batch_size) {
    std::cout << "\n=== Benchmark: Bulk Operations (MmapSPSC) ===" << std::endl;
    std::cout << "Iterations: " << iterations << ", Batch Size: " << batch_size << std::endl;

    auto [sink, source] = MmapSPSC<int, Capacity>::create();

    // Producer thread
    Timer timer;
    std::thread producer([sink = std::move(sink), iterations, batch_size]() mutable {
        std::vector<int> batch(batch_size);
        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < batch_size; ++i) {
                batch[i] = iter * batch_size + i;
            }
            std::size_t enqueued = 0;
            while (enqueued < batch_size) {
                enqueued += sink.try_enqueue(batch.data() + enqueued, batch_size - enqueued);
                if (enqueued < batch_size) {
                    std::this_thread::yield();
                }
            }
        }
    });

    // Consumer thread
    std::thread consumer([source = std::move(source), iterations, batch_size]() mutable {
        std::vector<int> batch(batch_size);
        int total_consumed = 0;
        int target = iterations * batch_size;
        while (total_consumed < target) {
            std::size_t dequeued = source.try_dequeue(batch.data(), batch_size);
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

    return { "MmapSPSC", "Bulk", Capacity, iterations, batch_size, elapsed, ops_per_sec };
}

// Function to write results to CSV
void write_csv(const std::string& filename, const std::vector<BenchmarkResult>& results) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open CSV file: " << filename << std::endl;
        return;
    }

    // Write CSV header
    file << "queue_type,operation_type,capacity,iterations,batch_size,elapsed_us,ops_per_sec\n";

    // Write data rows
    for (const auto& result : results) {
        file << result.queue_type << "," << result.operation_type << "," << result.capacity << ","
             << result.iterations << "," << result.batch_size << "," << std::fixed
             << std::setprecision(2) << result.elapsed_us << "," << std::scientific
             << std::setprecision(6) << result.ops_per_sec << "\n";
    }

    std::cout << "\n✓ CSV results written to: " << filename << std::endl;
}

// Benchmark with varying batch sizes
std::vector<BenchmarkResult> benchmark_comparison() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║           SPSC vs MutexQueue Performance Comparison        ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;

    std::vector<BenchmarkResult> results;

    std::vector<std::pair<int, int>> configs = {
        { 1000000, 1 }, // Many individual ops
        { 100000, 10 }, // Small batches
        { 10000, 100 }, // Medium batches
        { 1000, 1000 }, // Large batches
        { 100, 10000 }, // Very large batches
    };

    std::cout << "\n┌─────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Config: varied batch sizes                                  │" << std::endl;
    std::cout << "│ Queue capacity: 64                                          │" << std::endl;
    std::cout << "│ Implementation: SPSC (lock-free)                            │" << std::endl;
    std::cout << "└─────────────────────────────────────────────────────────────┘" << std::endl;

    for (const auto& [iterations, batch_size] : configs) {
        std::cout << "\n─────────────────────────────────────────────────────────────" << std::endl;
        std::cout << "Configuration: " << iterations << " iterations * " << batch_size
                  << " batch size" << std::endl;
        std::cout << "─────────────────────────────────────────────────────────────" << std::endl;

        results.push_back(benchmark_individual_ops<64>(iterations, batch_size));
        results.push_back(benchmark_bulk_ops<64>(iterations, batch_size));
    }

    std::cout << "\n┌─────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Config: varied batch sizes                                  │" << std::endl;
    std::cout << "│ Queue capacity: 64                                          │" << std::endl;
    std::cout << "│ Implementation: MutexQueue                                  │" << std::endl;
    std::cout << "└─────────────────────────────────────────────────────────────┘" << std::endl;

    for (const auto& [iterations, batch_size] : configs) {
        std::cout << "\n─────────────────────────────────────────────────────────────" << std::endl;
        std::cout << "Configuration: " << iterations << " iterations * " << batch_size
                  << " batch size" << std::endl;
        std::cout << "─────────────────────────────────────────────────────────────" << std::endl;

        results.push_back(benchmark_individual_ops_mutex<64>(iterations, batch_size));
        results.push_back(benchmark_bulk_ops_mutex<64>(iterations, batch_size));
    }

    std::cout << "\n┌─────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Config: varied batch sizes                                  │" << std::endl;
    std::cout << "│ Queue capacity: 4096                                        │" << std::endl;
    std::cout << "│ Implementation: SPSC (lock-free)                            │" << std::endl;
    std::cout << "└─────────────────────────────────────────────────────────────┘" << std::endl;

    for (const auto& [iterations, batch_size] : configs) {
        std::cout << "\n─────────────────────────────────────────────────────────────" << std::endl;
        std::cout << "Configuration: " << iterations << " iterations * " << batch_size
                  << " batch size" << std::endl;
        std::cout << "─────────────────────────────────────────────────────────────" << std::endl;

        results.push_back(benchmark_individual_ops<4096>(iterations, batch_size));
        results.push_back(benchmark_bulk_ops<4096>(iterations, batch_size));
    }

    std::cout << "\n┌─────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Config: varied batch sizes                                  │" << std::endl;
    std::cout << "│ Queue capacity: 4096                                        │" << std::endl;
    std::cout << "│ Implementation: MutexQueue                                  │" << std::endl;
    std::cout << "└─────────────────────────────────────────────────────────────┘" << std::endl;

    for (const auto& [iterations, batch_size] : configs) {
        std::cout << "\n─────────────────────────────────────────────────────────────" << std::endl;
        std::cout << "Configuration: " << iterations << " iterations * " << batch_size
                  << " batch size" << std::endl;
        std::cout << "─────────────────────────────────────────────────────────────" << std::endl;

        results.push_back(benchmark_individual_ops_mutex<4096>(iterations, batch_size));
        results.push_back(benchmark_bulk_ops_mutex<4096>(iterations, batch_size));
    }

    std::cout << "\n┌─────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Config: varied batch sizes                                  │" << std::endl;
    std::cout << "│ Queue capacity: 4096                                        │" << std::endl;
    std::cout << "│ Implementation: MmapSPSC (lock-free, double-mapped)        │" << std::endl;
    std::cout << "└─────────────────────────────────────────────────────────────┘" << std::endl;

    for (const auto& [iterations, batch_size] : configs) {
        std::cout << "\n─────────────────────────────────────────────────────────────" << std::endl;
        std::cout << "Configuration: " << iterations << " iterations * " << batch_size
                  << " batch size" << std::endl;
        std::cout << "─────────────────────────────────────────────────────────────" << std::endl;

        results.push_back(benchmark_individual_ops_mmap<4096>(iterations, batch_size));
        results.push_back(benchmark_bulk_ops_mmap<4096>(iterations, batch_size));
    }

    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    Benchmark Complete                      ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;

    return results;
}

int main(int argc, char* argv[]) {
    std::cout << "Queue Performance Benchmark: SPSC vs MutexQueue vs MmapSPSC\n" << std::endl;

    // Parse command-line arguments
    std::string csv_path;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csv_path = argv[++i];
            // Early validation: try to open the file for writing
            std::ofstream test_file(csv_path);
            if (!test_file.is_open()) {
                std::cerr << "Error: Cannot write to CSV file: " << csv_path << std::endl;
                return 1;
            }
            test_file.close();
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --csv <path>    Write benchmark results to a CSV file\n";
            std::cout << "  --help, -h      Show this help message\n";
            return 0;
        }
    }

    // Run benchmarks
    auto results = benchmark_comparison();

    // Write CSV if requested
    if (!csv_path.empty()) {
        write_csv(csv_path, results);
    }

    return 0;
}
