#include "../include/types.hpp"
#include "../include/chunked_csr.hpp"
#include "../include/stm.hpp"
#include "../include/ebr.hpp"
#include "../include/simd_detector.hpp"

#include <vector>
#include <omp.h>
#include <chrono>
#include <iostream>
#include <atomic>

namespace fraud_engine {
    class Engine {
        private:
            ChunkedCSR graph;
            EpochReclamation ebr;

        public:
            explicit Engine(size_t num_nodes) : graph(num_nodes) {}

            double ingest_stream_parallel(const std::vector<Transaction>& transactions, int num_threads) {
                size_t total_tx = transactions.size();
                auto start_time = std::chrono::high_resolution_clock::now();

                #pragma omp parallel for num_threads(num_threads) schedule(static, 2048)
                for(size_t i=0; i < total_tx; i++) {
                    STMIngestor::ingest_edge(transactions[i], graph);

                    if(i%10000==0) ebr.try_advance_and_reclaim();
                }

                auto end_time = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = end_time - start_time;

                double throughput = total_tx/elapsed.count();
                std::cout << "Ingestion complete: " << total_tx << " edges in " << elapsed.count() << 
                " seconds (" << throughput << " tx/sec)  using " << num_threads << " threads." << std::endl;

                return throughput;
            }

            std::vector<DetectionResult> run_continuous_analytics(int num_threads) {
                auto start_time = std::chrono::high_resolution_clock::now();

                #pragma omp parallel num_threads(num_threads)
                {
                    size_t thread_id = omp_get_thread_num();
                    ebr.enter_epoch(thread_id);
                }

                std::vector<DetectionResult> alerts = SIMDDetector::run_parallel_detection(graph, num_threads);

                #pragma omp parallel num_threads(num_threads)
                {
                    size_t thread_id = omp_get_thread_num();
                    ebr.exit_epoch(thread_id);
                }

                auto end_time = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed_ms = end_time - start_time;

                std::cout << "Analytics scan complete: Found " << alerts.size() << 
                " suspicious loops/clusters in " << elapsed_ms.count() << " ms." << std::endl;

                return alerts;
            }
    };
}