#ifndef STM_HPP
#define STM_HPP

#include "types.hpp"
#include "chunked_csr.hpp"
#include "ebr.hpp"
#include <thread>

namespace fraud_engine {
    
    class STMIngestor {
        public:
            static void ingest_edge(const Transaction& tx, ChunkedCSR& graph) {
                uint32_t u = tx.src;
                uint32_t v = tx.dst;
                NodeDescriptor& node = graph.get_node(u);

                uint64_t expected_version;

                while(true) {
                    expected_version = node.version.load(std::memory_order_acquire);

                    if((expected_version&1)!=0) {
                        std::this_thread::yield();
                        continue;
                    }

                    if(node.version.compare_exchange_strong(expected_version, expected_version+1, std::memory_order_acquire)) {
                        break;
                    }
                }

                Chunk* tail = node.tail.load(std::memory_order_relaxed);

                if(tail==nullptr) {
                    tail = graph.allocate_chunk();
                    node.head.store(tail, std::memory_order_release);
                    node.tail.store(tail, std::memory_order_release);
                }

                uint16_t current_count = tail->get_count(std::memory_order_relaxed);
                if(current_count >= EDGES_PER_CHUNK) {
                    Chunk* new_chunk = graph.allocate_chunk();
                    tail->set_next(new_chunk, std::memory_order_release);

                    node.tail.store(new_chunk, std::memory_order_release);
                    tail = new_chunk;
                    current_count = 0;
                }

                tail->edges[current_count] = v;
                tail->increment_count(std::memory_order_release);
                node.degree.fetch_add(1, std::memory_order_relaxed);

                node.version.store(expected_version+2, std::memory_order_release);
            }
    };
}

#endif