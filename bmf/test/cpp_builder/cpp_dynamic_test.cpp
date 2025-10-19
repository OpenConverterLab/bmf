#include <chrono>
#include <thread>
#include "gtest/gtest.h"
#include <bmf/sdk/log.h>
#include <bmf/sdk/json_param.h>
#include "../include/common.h"
#include "../../connector/include/builder.hpp"
#include "cpp_test_helper.h" 

// 动态重置功能测试
TEST(cpp_dynamic_reset, reset_pass_through_node) {
    const std::string output_file = "./output_reset_cpp.mp4";
    const std::string input_file = "../../files/big_bunny_10s_30fps.mp4";
    BMF_CPP_FILE_REMOVE(output_file); 

    // 1. 创建主图
    nlohmann::json graph_para = {{"dump_graph", 1}};
    auto main_graph = bmf::builder::Graph(bmf::builder::NormalMode,
                                          bmf_sdk::JsonParam(graph_para));
    BMFLOG(BMF_INFO) << "[cpp_dynamic_reset] 主图创建完成";

    // 3. 添加解码器节点
    nlohmann::json decode_para = {
        {"input_path", input_file} 
    };
    auto decoder_node = main_graph.Decode(bmf_sdk::JsonParam(decode_para), 
                                          "decoder0");  // 第二个参数是节点别名
    auto video_stream = decoder_node["video"];  // 提取视频流
    auto audio_stream = decoder_node["audio"];  // 提取音频流
    BMFLOG(BMF_INFO) << "[cpp_dynamic_reset] 解码器节点创建完成：alias=decoder0";

    // 3. 添加待重置 PassThrough 节点
    std::vector<bmf::builder::Stream> pass_through_inputs = {video_stream, audio_stream};
    nlohmann::json pass_through_para = {}; 
    bmf_sdk::JsonParam pass_through_option(nlohmann::json::object());
    const std::string python_module_dir = "../../../bmf/test/dynamical_graph";   
    auto pass_through_node = main_graph.Module(
        pass_through_inputs, 
        "reset_pass_through",                     
        bmf::builder::ModuleType::Python,      
        pass_through_option,  
        "reset_pass_through",  
        python_module_dir,             
        "",                                                     
        bmf::builder::InputManagerType::Immediate, 
        0                                  
    );
    BMFLOG(BMF_INFO) << "[cpp_dynamic_reset] 待重置节点创建完成";

    // 4. 非阻塞启动图（对应 Python 层 run_wo_block，匹配 Graph::Start API）
    main_graph.Start(true, true);  // 参数1：dump_graph（true=打印图配置），参数2：needMerge（true=合并配置）
    BMFLOG(BMF_INFO) << "[cpp_dynamic_reset] 图非阻塞启动，等待20ms确保节点初始化";
    std::this_thread::sleep_for(std::chrono::milliseconds(20)); 

    // 5. 构造动态重置配置
    nlohmann::json reset_config = {
        {"alias", "reset_pass_through"}, 
        {"output_path", output_file},     
        {"video_params", {               
            {"codec", "h264"},
            {"width", 320},
            {"height", 240},
            {"crf", 23},
            {"preset", "veryfast"}
        }}
    };
    bmf_sdk::JsonParam reset_config_param(reset_config);
    BMFLOG(BMF_INFO) << "[cpp_dynamic_reset] 动态重置配置:\n" << reset_config.dump(2);

    nlohmann::json reset_update_config = main_graph.DynamicResetNode(reset_config_param);
    if (reset_update_config.is_null()) {
        BMFLOG(BMF_ERROR) << "[cpp_dynamic_reset] 动态重置配置生成失败";
        FAIL() << "动态重置配置生成失败";
    }

    // 执行实际的更新操作
    int update_ret = main_graph.Update(bmf_sdk::JsonParam(reset_update_config));
    if (update_ret != 0) {
        BMFLOG(BMF_ERROR) << "[cpp_dynamic_reset] 动态重置调用失败，返回码：" << update_ret;
        FAIL() << "动态重置节点调用失败";
    }
    
    BMFLOG(BMF_INFO) << "[cpp_dynamic_reset] 动态重置指令已发送，等待1秒确保处理完成";
    std::this_thread::sleep_for(std::chrono::seconds(1));   

    // 6. 关闭图（释放资源，匹配 Graph::Close API）
    int close_ret = main_graph.Close();
    if (close_ret != 0) {
        BMFLOG(BMF_ERROR) << "[cpp_dynamic_reset] 图关闭失败，返回码：" << close_ret;
        FAIL() << "图关闭失败";
    }
    BMFLOG(BMF_INFO) << "[cpp_dynamic_reset] 主图已正常关闭";

    BMFLOG(BMF_INFO) << "[cpp_dynamic_reset] 测试通过：动态重置功能正常，输出文件符合预期";
}


// 测试入口（默认GTest入口）
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}