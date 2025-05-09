#include "builder.hpp"
#include "nlohmann/json.hpp"
#include "connector.hpp"

#include <regex>

// Global variable or pass-by-reference as needed
int frame_number_total = 0;
int frame_number_global = 0;

int process_number = 0;
double rest_time = 0;

std::chrono::system_clock::time_point last_encoder_call; // Track last call time
bool first_encoder_call = true; // Flag for first call

std::vector<double> duration_history;  // Store recent durations for averaging
constexpr size_t max_history_size = 20;  // Limit for the number of durations tracked
constexpr double min_duration_threshold = 10.0;  // Ignore durations < 10 ms

double compute_smooth_duration(double new_duration) {
    if (new_duration >= min_duration_threshold) {
        duration_history.push_back(new_duration);
        if (duration_history.size() > max_history_size) {
            duration_history.erase(duration_history.begin());
        }
    }
    return duration_history.empty() ? 0.0 :
           std::accumulate(duration_history.begin(), duration_history.end(), 0.0) / duration_history.size();
}

// std::string format_time(const std::tm& tm) {
//     char buffer[20];  // Buffer to hold formatted time string
//     std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
//                   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
//                   tm.tm_hour, tm.tm_min, tm.tm_sec);
//     return std::string(buffer);
// }

bmf_sdk::CBytes decoder_callback(bmf_sdk::CBytes input) {
        std::string strInfo;
        strInfo.assign(reinterpret_cast<const char*>(input.buffer), input.size);
        // BMFLOG(BMF_INFO) << "====Callback==== " << strInfo;


        std::regex frame_regex(R"(\btotal frame number:\s*(\d+))");
        std::smatch match;

        if (std::regex_search(strInfo, match, frame_regex) && match.size() > 1) {
            std::istringstream(match[1]) >> frame_number_total; // Convert to int
            BMFLOG(BMF_DEBUG) << "Extracted Frame Number: " << frame_number_total;
        } else {
            BMFLOG(BMF_WARNING) << "Failed to extract frame number";
        }

        uint8_t bytes[] = {97, 98, 99, 100, 101, 0};
        return bmf_sdk::CBytes{bytes, 6};
}

bmf_sdk::CBytes encoder_callback(bmf_sdk::CBytes input) {
        std::string strInfo;
        strInfo.assign(reinterpret_cast<const char*>(input.buffer), input.size);
        // BMFLOG(BMF_INFO) << "====Callback==== " << strInfo;

        std::regex frame_regex(R"(\bframe number:\s*(\d+))");
        std::smatch match;

        if (std::regex_search(strInfo, match, frame_regex) && match.size() > 1) {
            std::istringstream(match[1]) >> frame_number_global; // Convert to int
            BMFLOG(BMF_DEBUG) << "Extracted Total Frame Number: " << frame_number_global;
            process_number = frame_number_global * 100 / frame_number_total;
                
            static auto last_encoder_call_time = std::chrono::system_clock::now();
            auto now = std::chrono::system_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_encoder_call_time).count();
            last_encoder_call_time = now;

            double smooth_duration = compute_smooth_duration(duration);
            if (frame_number_global > 0 && frame_number_total > 0) {
                // double progress = static_cast<double>(frame_number_global) / frame_number_total;
                rest_time = smooth_duration * (frame_number_total-frame_number_global) / 1000;
            }

            BMFLOG(BMF_INFO) << "Process Number (percentage): " << process_number << "%\t"
                            << "Current duration (milliseconds): " << duration << "\t"
                            << "Smoothed Duration: " << smooth_duration << " ms\t"
                            << "Estimated Rest Time (seconds): " << rest_time;



            if (frame_number_global == frame_number_total) {
                BMFLOG(BMF_INFO) << "====Callback==== Finish";
            }

        } else {
            BMFLOG(BMF_WARNING) << "Failed to extract frame number";
        }

        uint8_t bytes[] = {97, 98, 99, 100, 101, 0};
        return bmf_sdk::CBytes{bytes, 6};
}

int main() {
    std::string output_file = "./cb.mp4";

    std::function<bmf_sdk::CBytes(bmf_sdk::CBytes)> de_callback = decoder_callback;
    std::function<bmf_sdk::CBytes(bmf_sdk::CBytes)> en_callback = encoder_callback;

    nlohmann::json graph_para = {
        {"dump_graph", 0}
    };
    auto graph = bmf::builder::Graph(bmf::builder::NormalMode,
    bmf_sdk::JsonParam(graph_para));

    nlohmann::json decode_para = {
        {"input_path", "../../files/big_bunny_10s_30fps.mp4"},
    };
    auto video = graph.Decode(bmf_sdk::JsonParam(decode_para));
    video.AddCallback(0, std::function<bmf_sdk::CBytes(bmf_sdk::CBytes)>(decoder_callback));

    nlohmann::json encode_para = {
        {"output_path", output_file},
        {"video_params", {
            {"codec", "h264"},
            {"crf", 23},
            {"preset", "veryslow"}
        }},
        {"audio_params", {
            {"codec", "aac"},
            {"bit_rate", 128000},
            {"sample_rate", 44100},
            {"channels", 2}
        }}
    };



    auto node = graph.Encode(video["video"], video["audio"],
                bmf_sdk::JsonParam(encode_para));
    
    node.AddCallback(0, en_callback);

    graph.Run();

}