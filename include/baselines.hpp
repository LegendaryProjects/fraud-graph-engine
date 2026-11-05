#ifndef BASELINES_HPP
#define BASELINES_HPP

#include "types.hpp"
#include "chunked_csr.hpp"
#include <vector>
#include <mutex>
#include <shared_mutex>

namespace fraud_engine {
    class BaselineMutexList {
        private:
            struct Node {
                std::vector<uint32_t> neighbors;
                std::mutex mtx;
            };
            std::vector<Node> graph;

        public:
            explicit BaselineMutexList(size_t num_nodes);
            void ingest_edge(uint32_t u, uint32_t v);
            bool search_target(uint32_t start_node, uint32_t target);
    };

    class BaselineFlatCSR {
        private:
            std::vector<uint32_t> row_ptrs;
            std::vector<uint32_t> edges;
            std::shared_mutex global_rw_lock;

        public:
            explicit BaselineFlatCSR(size_t num_nodes);
            void ingest_edge(uint32_t u, uint32_t v);
            bool search_target(uint32_t start_node, uint32_t target);
    };

    class BaselineScalarCSR {
        public:
            static bool scan_chunk_scalar(const Chunk* chunk, uint32_t target_id);
    };
}

#endif