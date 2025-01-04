#include "builder.hpp"
#include "nlohmann/json.hpp"
#include "connector.hpp"
// #include "../../engine/c_engine/include/callback_layer.h"

bmf_sdk::CBytes my_callback_function(bmf_sdk::CBytes input) {
        std::string strInfo;
        strInfo.assign(reinterpret_cast<const char*>(input.buffer), input.size);
        BMFLOG(BMF_INFO) << "====Callback==== " << strInfo;
        uint8_t bytes[] = {97, 98, 99, 100, 101, 0};
        return bmf_sdk::CBytes{bytes, 6};
}

int main() {
    std::string output_file = "./cb.mp4";

    nlohmann::json graph_para = {
        {"dump_graph", 0}
    };
    auto graph = bmf::builder::Graph(bmf::builder::NormalMode,
    bmf_sdk::JsonParam(graph_para));

    nlohmann::json decode_para = {
        {"input_path", "../../files/big_bunny_10s_30fps.mp4"},
    };
    auto video = graph.Decode(bmf_sdk::JsonParam(decode_para));
    nlohmann::json encode_para = {
        {"output_path", output_file},
        {"video_params", {
            {"codec", "h264"},
            {"width", 320},
            {"height", 240},
            {"crf", 23},
            {"preset", "veryfast"}
        }},
        {"audio_params", {
            {"codec", "aac"},
            {"bit_rate", 128000},
            {"sample_rate", 44100},
            {"channels", 2}
        }}
    };

    std::function<bmf_sdk::CBytes(bmf_sdk::CBytes)> callback = my_callback_function;

    auto node = graph.Encode(video["video"], video["audio"],
                bmf_sdk::JsonParam(encode_para));
    
    node.AddCallback(0, callback);

    graph.Run();

}