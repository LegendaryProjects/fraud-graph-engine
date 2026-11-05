#include "../include/baselines.hpp"
#include <algorithm>

namespace fraud_engine {

    BaselineMutexList::BaselineMutexList(size_t num_nodes) : graph(num_nodes) {}

    void BaselineMutexList::ingest_edge(uint32_t u, uint32_t v) {
        if(u>=graph.size()) return;
        std::lock_guard<std::mutex> lock(graph[u].mtx);
        graph[u].neighbors.push_back(v);
    }

    bool BaselineMutexList::search_target(uint32_t start_node, uint32_t target) {
        if(start_node>=graph.size()) return false;
        std::lock_guard<std::mutex> lock(graph[start_node].mtx);
        for(uint32_t neighbor : graph[start_node].neighbors) {
            if(neighbor==target) return true;
        }
        return false;
    }

    BaselineFlatCSR::BaselineFlatCSR(size_t num_nodes) {
        row_ptrs.assign(num_nodes+1, 0);
    }

    void BaselineFlatCSR::ingest_edge(uint32_t u, uint32_t v) {
        std::unique_lock<std::shared_mutex> lock(global_rw_lock);
        size_t insert_indx = row_ptrs[u+1];
        edges.insert(edges.begin()+insert_indx, v);

        for(size_t i=u+1; i<row_ptrs.size(); i++) row_ptrs[i]++;
    }

    bool BaselineFlatCSR::search_target(uint32_t start_node, uint32_t target) {
        std::shared_lock<std::shared_mutex> lock(global_rw_lock);
        if(start_node+1>=row_ptrs.size()) return false;

        size_t start_idx = row_ptrs[start_node];
        size_t end_idx = row_ptrs[start_node + 1];

        for(size_t i=start_idx; i<end_idx; i++) {
            if(edges[i]==target) return true;
        }
        return false;
    }

    bool BaselineScalarCSR::scan_chunk_scalar(const Chunk* chunk, uint32_t target_id) {
        if(!chunk) return false;

        uint16_t count = chunk->get_count(std::memory_order_relaxed);
        size_t limit = std::min(static_cast<size_t>(count), EDGES_PER_CHUNK);

        for(size_t i=0; i< limit; i++) {
            if(chunk->edges[i]==target_id) return true;
        }
        return false;
    }
}