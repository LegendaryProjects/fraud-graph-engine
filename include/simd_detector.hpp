#ifndef SIMD_DETECTOR_HPP
#define SIMD_DETECTOR_HPP

#include "types.hpp"
#include "chunked_csr.hpp"
#include <vector>

namespace fraud_engine {

    class SIMDDetector {
    public:
        static bool scan_chunk_avx2(const Chunk* chunk, uint32_t target_id);

        static std::vector<DetectionResult> run_parallel_detection(ChunkedCSR& graph, int num_threads);
    };
}

#endif