#include "../include/types.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <chrono>

namespace fraud_engine {

struct BenchmarkMetric {
    int threads;
    double throughput_tx_sec;
    double latency_ms;
    double speedup;
};

class TelemetryLogger {
private:
    std::string output_path;
    size_t last_alert_count = 0; // Tracks what has already been logged

public:
    explicit TelemetryLogger(const std::string& path = "data/telemetry_live.json")
        : output_path(path) {
        // Initialize permanent CSV log with headers
        std::ofstream csv("data/fraud_history.csv", std::ios::trunc);
        csv << "Alert_ID,Fraud_Type,Source_Node,Target_Node,Detection_Time_us\n";
    }

    void export_telemetry_json(
        const std::vector<BenchmarkMetric>& scaling_data,
        const std::vector<DetectionResult>& alerts,
        size_t total_processed_edges,
        double total_runtime_seconds) 
    {
        // 1. PERMANENT CSV STORAGE (Append only new alerts)
        if (alerts.size() > last_alert_count) {
            std::ofstream csv("data/fraud_history.csv", std::ios::app);
            for (size_t i = last_alert_count; i < alerts.size(); ++i) {
                const auto& a = alerts[i];
                std::string type_str = (a.type == FraudType::DIRECTED_CYCLE) ? "DIRECTED_CYCLE" : "MULE_CLUSTER";
                csv << (i + 1) << "," << type_str << "," << a.source_node << "," 
                    << a.target_node << "," << std::fixed << std::setprecision(2) << a.detection_latency_us << "\n";
            }
            last_alert_count = alerts.size();
        }

        // 2. LIVE DASHBOARD JSON (Export only the LATEST 100)
        std::ofstream out(output_path);
        if (out.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            out << "{\n";
            out << "  \"timestamp\": " << now_ms << ",\n";
            out << "  \"total_edges_processed\": " << total_processed_edges << ",\n";
            out << "  \"total_runtime_seconds\": " << std::fixed << std::setprecision(3) << total_runtime_seconds << ",\n";

            out << "  \"scaling_metrics\": [\n";
            for (size_t i = 0; i < scaling_data.size(); ++i) {
                const auto& m = scaling_data[i];
                out << "    {\n";
                out << "      \"threads\": " << m.threads << ",\n";
                out << "      \"throughput\": " << std::fixed << std::setprecision(2) << m.throughput_tx_sec << ",\n";
                out << "      \"latency_ms\": " << std::fixed << std::setprecision(4) << m.latency_ms << ",\n";
                out << "      \"speedup\": " << std::fixed << std::setprecision(2) << m.speedup << "\n";
                out << "    }" << (i + 1 < scaling_data.size() ? "," : "") << "\n";
            }
            out << "  ],\n";

            out << "  \"recent_alerts\": [\n";
            
            // Calculate start index to grab exactly the last 100 items
            size_t total_alerts = alerts.size();
            size_t start_idx = (total_alerts > 100) ? total_alerts - 100 : 0;
            
            for (size_t i = start_idx; i < total_alerts; ++i) {
                const auto& a = alerts[i];
                out << "    {\n";
                out << "      \"alert_id\": " << i + 1 << ",\n";
                out << "      \"type\": \"" << (a.type == FraudType::DIRECTED_CYCLE ? "DIRECTED_CYCLE" : "MULE_CLUSTER") << "\",\n";
                out << "      \"source_node\": " << a.source_node << ",\n";
                out << "      \"target_node\": " << a.target_node << ",\n";
                out << "      \"latency_us\": " << std::fixed << std::setprecision(2) << a.detection_latency_us << "\n";
                out << "    }" << (i + 1 < total_alerts ? "," : "") << "\n";
            }
            out << "  ]\n";
            out << "}\n";
            out.close();
        }
    }
};

} // namespace fraud_engine