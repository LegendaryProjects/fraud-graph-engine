#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <atomic>
#include <immintrin.h>

namespace fraud_engine {

    #pragma pack(push, 1)
    struct Transaction {
        uint32_t src;
        uint32_t dst;
        double amount;
        uint64_t timestamp;
    };
    #pragma pack(pop)

    static_assert(sizeof(Transaction)== 24, "Transaction struct must be exactly 24 bytes");

    constexpr uint32_t EMPTY_EDGE = 0XFFFFFFFF;

    constexpr size_t EDGES_PER_CHUNK = 14;

    constexpr uint64_t COUNT_SHIFT = 48ULL; //110000
    constexpr uint64_t ADDR_MASK = 0x0000FFFFFFFFFFFFULL;

    struct alignas(64) Chunk {
        uint32_t edges[EDGES_PER_CHUNK];
        std::atomic<uint64_t> tagged_next;

        Chunk() {
            for(size_t i=0; i< EDGES_PER_CHUNK; i++) {
                edges[i] = EMPTY_EDGE;
            }
            tagged_next.store(0, std::memory_order_relaxed);
        }

        inline Chunk* get_next(std::memory_order order = std::memory_order_acquire) const noexcept {
            uint64_t val = tagged_next.load(order);
            return reinterpret_cast<Chunk*>(val & ADDR_MASK);
        }

        inline uint16_t get_count(std::memory_order order = std::memory_order_relaxed) const noexcept {
            uint64_t val = tagged_next.load(order);
            return static_cast<uint16_t>(val >> COUNT_SHIFT);
        }

        inline void set_next(Chunk* next_ptr, std::memory_order order = std::memory_order_release) noexcept {
            uint64_t expected = tagged_next.load(std::memory_order_relaxed);
            uint64_t desired;
            do {
                uint64_t count_part = expected & ~ADDR_MASK;
                desired = count_part | (reinterpret_cast<uint64_t>(next_ptr) & ADDR_MASK);
            } while (!tagged_next.compare_exchange_weak(expected, desired, order, std::memory_order_relaxed));
        }

        inline uint16_t increment_count(std::memory_order order = std::memory_order_acq_rel) noexcept {
            uint64_t step = 1ULL << COUNT_SHIFT;
            uint64_t prev = tagged_next.fetch_add(step, order);
            return static_cast<uint16_t>((prev >> COUNT_SHIFT) + 1);
        }
    };

    static_assert(sizeof(Chunk) == 64, "Chunk struct must be exactly 64 bytes (1 CPU cache line)");

    struct alignas(64) NodeDescriptor {
        std::atomic<uint64_t> version{0};
        std::atomic<Chunk*> head{nullptr};
        std::atomic<Chunk*> tail{nullptr};
        std::atomic<uint32_t> degree{0};
        uint8_t pad[36];
    };

    static_assert(sizeof(NodeDescriptor)== 64, "NodeDescriptor must be exactly 64 bytes");

    enum class FraudType : uint8_t {
        NONE = 0,
        DIRECTED_CYCLE = 1,
        MULE_CLUSTER = 2
    };

    struct DetectionResult {
        bool is_fraud{false};
        FraudType type{FraudType::NONE};
        uint32_t source_node{0};
        uint32_t target_node{0};
        uint32_t cycle_length{0};
        double detection_latency_us{0.0};
    };
}

#endif