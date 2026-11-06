#include "../include/simd_detector.hpp"
#include <immintrin.h>
#include <x86intrin.h> // Added for hardware cycle counters (__rdtsc)
#include <omp.h>
#include <algorithm>

namespace fraud_engine {

bool SIMDDetector::scan_chunk_avx2(const Chunk* chunk, uint32_t target_id) {
    if (!chunk) return false;
    __m256i target_vec = _mm256_set1_epi32(target_id);

    __m256i edges_vec1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(&chunk->edges[0]));
    __m256i cmp1 = _mm256_cmpeq_epi32(edges_vec1, target_vec);
    int mask1 = _mm256_movemask_ps(_mm256_castsi256_ps(cmp1));
    if (mask1 != 0) return true;

    __m256i edges_vec2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&chunk->edges[6]));
    __m256i cmp2 = _mm256_cmpeq_epi32(edges_vec2, target_vec);
    int mask2 = _mm256_movemask_ps(_mm256_castsi256_ps(cmp2));
    return mask2 != 0;
}

std::vector<DetectionResult> SIMDDetector::run_parallel_detection(ChunkedCSR& graph, int num_threads) {
    std::vector<DetectionResult> global_alerts;
    
    #pragma omp parallel num_threads(num_threads)
    {
        std::vector<DetectionResult> thread_local_alerts;
        
        #pragma omp for schedule(dynamic, 64)
        for (uint32_t i = 0; i < graph.num_nodes; ++i) {
            NodeDescriptor& node = graph.get_node(i);
            Chunk* curr = node.head.load(std::memory_order_acquire);
            
            // Check for high-degree Mule Clusters dynamically
            unsigned long long start_mule = __rdtsc();
            int total_degree = 0;
            Chunk* temp = curr;
            while(temp != nullptr) {
                total_degree += temp->get_count(std::memory_order_acquire);
                temp = temp->get_next(std::memory_order_acquire);
            }
            
            if (total_degree > 25 && curr != nullptr) {
                unsigned long long end_mule = __rdtsc();
                
                DetectionResult alert;
                alert.is_fraud = true;
                alert.type = FraudType::MULE_CLUSTER;
                alert.source_node = i;
                alert.target_node = curr->edges[0];
                // Convert cycles to microseconds (Assuming ~3.0 GHz base clock)
                alert.detection_latency_us = (end_mule - start_mule) / 3000.0; 
                
                if (thread_local_alerts.size() < 15) thread_local_alerts.push_back(alert);
            }
            
            // Check for 2-Hop Directed Cycles using AVX2
            while (curr != nullptr) {
                uint16_t count = curr->get_count(std::memory_order_acquire);
                int limit = std::min(static_cast<int>(count), 14);
                
                for (int e = 0; e < limit; ++e) {
                    uint32_t neighbor = curr->edges[e];
                    if (neighbor != i && neighbor < graph.num_nodes) {
                        NodeDescriptor& n_node = graph.get_node(neighbor);
                        Chunk* n_curr = n_node.head.load(std::memory_order_acquire);
                        
                        while (n_curr != nullptr) {
                            unsigned long long start_cycle = __rdtsc();
                            
                            if (scan_chunk_avx2(n_curr, i)) {
                                unsigned long long end_cycle = __rdtsc();
                                
                                DetectionResult alert;
                                alert.is_fraud = true;
                                alert.type = FraudType::DIRECTED_CYCLE;
                                alert.source_node = i;
                                alert.target_node = neighbor;
                                alert.detection_latency_us = (end_cycle - start_cycle) / 3000.0; 
                                
                                if (thread_local_alerts.size() < 15) thread_local_alerts.push_back(alert);
                                break;
                            }
                            n_curr = n_curr->get_next(std::memory_order_acquire);
                        }
                    }
                }
                curr = curr->get_next(std::memory_order_acquire);
            }
        }
        
        #pragma omp critical
        {
            if (global_alerts.size() < 100) { 
                global_alerts.insert(global_alerts.end(), 
                                     thread_local_alerts.begin(), 
                                     thread_local_alerts.end());
            }
        }
    }
    return global_alerts;
}
} // namespace fraud_engine