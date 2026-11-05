#include "../include/types.hpp"
#include "../include/chunked_csr.hpp"
#include "../include/ebr.hpp"
#include "../include/stm.hpp"
#include "../include/simd_detector.hpp"

#include "stream_loader.cpp"
#include "engine.cpp"
#include "telemetry.cpp"

#include <iostream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <thread>
#include <algorithm>

using namespace fraud_engine;

int main(int argc, char* argv[]) {
    std::string dataset_path = "transactions.bin";
    if (argc > 1) dataset_path = argv[1];

    try {
        std::cout << "========================================================\n";
        std::cout << " Streaming Graph Fraud Analytics Engine (STM + AVX2)\n";
        std::cout << "========================================================\n\n";

        std::cout << "[*] Loading synthetic streaming dataset...\n";
        std::vector<Transaction> transactions = StreamLoader::load_transactions(dataset_path);
        
        constexpr size_t NUM_NODES = 500005; 
        Engine* engine = new Engine(NUM_NODES);

        std::vector<int> thread_schedule = {1, 2, 4, 8, 12, 16};
        int optimal_threads = 8; 
        
        std::vector<BenchmarkMetric> scaling_data;
        std::vector<DetectionResult> active_alerts;
        
        double base_throughput = 0.0;
        double total_runtime = 0.0;
        size_t total_processed = 0;

        TelemetryLogger logger("data/telemetry_live.json");

        std::cout << "\n[*] Initiating Real-Time Streaming Simulation...\n";
        
        size_t offset = 0;
        size_t i = 0;
        
        while (offset < transactions.size()) {
            // DUAL-PHASE: Massive 500k batches for scaling benchmarks, fast 25k batches for live UI
            size_t target_batch = (i < thread_schedule.size()) ? 500000 : 25000;
            size_t current_batch = std::min(target_batch, transactions.size() - offset);
            int current_threads = (i < thread_schedule.size()) ? thread_schedule[i] : optimal_threads;
            
            std::vector<Transaction> batch(
                transactions.begin() + offset, 
                transactions.begin() + offset + current_batch
            );

            auto start_t = std::chrono::high_resolution_clock::now();
            double throughput = engine->ingest_stream_parallel(batch, current_threads);
            auto alerts = engine->run_continuous_analytics(current_threads);
            auto end_t = std::chrono::high_resolution_clock::now();
            
            total_runtime += std::chrono::duration<double>(end_t - start_t).count();
            total_processed += current_batch;

            if (!alerts.empty()) {
                active_alerts.insert(active_alerts.end(), alerts.begin(), alerts.end());
            }

            double avg_latency = alerts.empty() ? 0.0 : alerts[0].detection_latency_us / 1000.0;

            if (i < thread_schedule.size()) {
                if (current_threads == 1) base_throughput = throughput;
                double speedup = throughput / base_throughput;
                scaling_data.push_back({current_threads, throughput, avg_latency, speedup});
            }

            logger.export_telemetry_json(scaling_data, active_alerts, total_processed, total_runtime);

            std::cout << "[*] Streamed " << total_processed << "/" << transactions.size() 
                      << " edges | Threads: " << std::setw(2) << current_threads 
                      << " | Throughput: " << std::fixed << std::setprecision(2) << throughput << " tx/sec\n";

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            offset += current_batch;
            i++;
        }

        std::cout << "\n[*] Streaming Evaluation Complete. All alerts saved to data/fraud_history.csv\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}