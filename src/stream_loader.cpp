#include "../include/types.hpp"
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fraud_engine {
    class StreamLoader {
        public:
            static std::vector<Transaction> load_transactions(const std::string& filepath) {
                std::ifstream file(filepath, std::ios::binary | std::ios::ate);
                if(!file.is_open()) {
                    throw std::runtime_error("Falied to open transaction stream: " + filepath);
                }

                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);

                if(size % sizeof(Transaction)!=0) {
                    throw std::runtime_error("Corrupted dataset: File size is not a multiple of 24 bytes.");
                }

                size_t num_transactions = size/sizeof(Transaction);
                std::vector<Transaction> transactions(num_transactions);

                if(!file.read(reinterpret_cast<char*>(transactions.data()), size)) {
                    throw std::runtime_error("Failed to read transaction stream into memory");
                }

                std::cout << "Loaded " << num_transactions << " transactions (" << (size/(1024.0*1024.0)) << " MB) from " << filepath << std::endl;

                return transactions;
            }
    };
}