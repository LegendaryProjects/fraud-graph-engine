#ifndef EBR_HPP
#define EBR_HPP

#include "types.hpp"
#include <atomic>
#include <vector>
#include <mutex>
#include <thread>
#include <array>

namespace fraud_engine {

    class EpochReclamation {
        private:
            std::atomic<uint64_t>global_epoch{0};

            static constexpr size_t MAX_THREADS = 32;
            std::array<std::atomic<uint64_t>, MAX_THREADS> local_epochs;

            std::array<std::vector<Chunk*>, 3> retirement_queues;
            std::mutex queue_mutex;

        public:
            EpochReclamation() {
                for(auto& le: local_epochs) {
                    le.store(UINT64_MAX, std::memory_order_relaxed); //its like maximum means inactive
                }
            }

            ~EpochReclamation() {
                for(auto& queue: retirement_queues) {
                    for (Chunk* ptr: queue) {
                        delete ptr;
                    }
                    queue.clear();
                }
            }

            inline void enter_epoch(size_t thread_id) {
                uint64_t current_epoch = global_epoch.load(std::memory_order_acquire);
                local_epochs[thread_id].store(current_epoch, std::memory_order_release);
            }

            inline void exit_epoch(size_t thread_id) {
                local_epochs[thread_id].store(UINT64_MAX, std::memory_order_release);
            }

            void retire_chunk(Chunk* old_chunk) {
                if(!old_chunk) return;

                uint64_t current_epoch = global_epoch.load(std::memory_order_acquire);
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    retirement_queues[current_epoch % 3].push_back(old_chunk);
                }
            }

            void try_advance_and_reclaim() {
                uint64_t current_epoch = global_epoch.load(std::memory_order_acquire);

                bool can_advance = true;
                for(size_t i=0; i<MAX_THREADS; i++) {
                    uint64_t active_epoch = local_epochs[i].load(std::memory_order_acquire);

                    if(active_epoch!=UINT64_MAX && active_epoch < current_epoch) {
                        can_advance = false;
                        break;
                    }
                }

                if(can_advance) {
                    global_epoch.fetch_add(1, std::memory_order_release);

                    uint64_t safe_epoch = (current_epoch +2) % 3; //e to e+1, so e-2 onwards safe to delete
                    std::lock_guard<std::mutex> lock(queue_mutex);

                    for(Chunk* ptr: retirement_queues[safe_epoch]) {
                        delete ptr;
                    }
                    retirement_queues[safe_epoch].clear();
                }
            }
    };
}
#endif