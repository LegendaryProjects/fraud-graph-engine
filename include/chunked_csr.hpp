#ifndef CHUNKED_CSR_HPP
#define CHUNKED_CSR_HPP

#include "types.hpp"
#include <cstdlib>
#include <new>
#include <malloc.h>
#include <stdexcept>
#include <cstring>

namespace fraud_engine {
    
    class ChunkedCSR {
        public:
            NodeDescriptor* nodes;
            size_t num_nodes;

            explicit ChunkedCSR(size_t n) : num_nodes(n) {
                void* ptr = _aligned_malloc(num_nodes * sizeof(NodeDescriptor), 64);
                if (!ptr) {
                    throw std::bad_alloc();
                }
                memset(ptr, 0, num_nodes * sizeof(NodeDescriptor));
                nodes = new (ptr) NodeDescriptor[num_nodes];
            }

            ~ChunkedCSR() {
                for(size_t i=0; i<num_nodes; i++) {
                    Chunk* curr = nodes[i].head.load(std::memory_order_relaxed);
                    while(curr != nullptr) {
                        Chunk* next = curr->get_next(std::memory_order_relaxed);
                        delete curr;
                        curr = next;
                    } 
                }

                for(size_t i=0; i<num_nodes; i++) {
                    nodes[i].~NodeDescriptor();
                }

                free(nodes);
            }

            Chunk* allocate_chunk() const {
                return new Chunk();
            }

            inline NodeDescriptor& get_node(uint32_t node_id) {
                return nodes[node_id];
            }
    };
}

#endif