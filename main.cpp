#include "lzav.h" // 包含 LZAV 算法的头文件
#include <iostream>
#include <vector>
#include <string>
#include <chrono>   // 用于高精度计时
#include <numeric>  // 用于 std::iota (可选，如果生成数字序列数据)
#include <algorithm> // 用于 std::equal 和 std::generate
#include <random>    // 用于生成随机数据 (可选)

// 辅助函数：将 LZAV 错误码转换为可读字符串
std::string getLzavErrorString(int errorCode) {
    switch (errorCode) {
        case LZAV_E_PARAMS: return "LZAV_E_PARAMS: 参数不正确。";
        case LZAV_E_SRCOOB: return "LZAV_E_SRCOOB: 源缓冲区越界。";
        case LZAV_E_DSTOOB: return "LZAV_E_DSTOOB: 目标缓冲区越界。";
        case LZAV_E_REFOOB: return "LZAV_E_REFOOB: 后向引用越界。";
        case LZAV_E_DSTLEN: return "LZAV_E_DSTLEN: 解压缩长度不匹配。";
        case LZAV_E_UNKFMT: return "LZAV_E_UNKFMT: 未知流格式。";
        case LZAV_E_PTROVR: return "LZAV_E_PTROVR: 指针溢出。";
        default: return "未知 LZAV 错误";
    }
}

int main() {
    // --- 1. 准备源数据 ---
    // 为了进行有意义的基准测试，请使用更大、更多样化的数据集。
    // 这里我们使用一个简单的重复模式作为演示，它具有很好的可压缩性。
    // 您也可以尝试使用随机数据来测试不可压缩情况。
    const size_t src_len = 10 * 1024 * 1024; // 10 MB 的数据量，可以根据需要调整
    std::vector<char> src_buf(src_len);

    // 填充一个可压缩的模式（例如，重复的字母表）
    for (size_t i = 0; i < src_len; ++i) {
        src_buf[i] = (char)('A' + (i % 26)); // 重复的 A-Z 模式
    }
    // 或者，填充随机数据（可压缩性较低）
    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::uniform_int_distribution<> distrib(0, 255);
    // std::generate(src_buf.begin(), src_buf.end(), [&](){ return static_cast<char>(distrib(gen)); });


    std::cout << "原始数据长度: " << src_len << " 字节" << std::endl;

    // --- 2. 使用 lzav_compress_default 进行压缩 ---
    int max_comp_len_default = lzav_compress_bound(static_cast<int>(src_len));
    if (max_comp_len_default == 0 && src_len != 0) {
        std::cerr << "错误: lzav_compress_bound 对非零源长度返回 0。" << std::endl;
        return 1;
    }
    std::vector<char> comp_buf_default(max_comp_len_default);

    auto start_comp_default = std::chrono::high_resolution_clock::now();
    int comp_len_default = lzav_compress_default(
        src_buf.data(), 
        comp_buf_default.data(), 
        static_cast<int>(src_len), 
        max_comp_len_default
    );
    auto end_comp_default = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> comp_duration_default = end_comp_default - start_comp_default;

    if (comp_len_default == 0 && src_len != 0) {
        std::cerr << "默认压缩错误! lzav_compress_default 返回 0。" << std::endl;
        return 1;
    } else if (comp_len_default < 0) {
        std::cerr << "默认压缩错误! lzav_compress_default 返回: " << comp_len_default << std::endl;
        return 1;
    }

    std::cout << "\n--- LZAV 默认压缩 (lzav_compress_default) ---" << std::endl;
    std::cout << "压缩数据长度: " << comp_len_default << " 字节" << std::endl;
    double compression_ratio_default = static_cast<double>(comp_len_default) / src_len * 100.0;
    std::cout << "压缩率: " << compression_ratio_default << "%" << std::endl;
    std::cout << "压缩耗时: " << comp_duration_default.count() * 1000.0 << " 毫秒" << std::endl;
    if (comp_duration_default.count() > 0) {
        double comp_speed_mbps_default = (static_cast<double>(src_len) / (1024 * 1024)) / comp_duration_default.count();
        std::cout << "压缩速度: " << comp_speed_mbps_default << " MB/秒" << std::endl;
    }

    // --- 3. 解压缩数据 (默认压缩) ---
    std::vector<char> decomp_buf_default(src_len); // 解压缩缓冲区必须与原始长度相同

    auto start_decomp_default = std::chrono::high_resolution_clock::now();
    int decomp_len_default = lzav_decompress(
        comp_buf_default.data(), 
        decomp_buf_default.data(), 
        comp_len_default, 
        static_cast<int>(src_len)
    );
    auto end_decomp_default = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> decomp_duration_default = end_decomp_default - start_decomp_default;

    if (decomp_len_default < 0) {
        std::cerr << "默认解压缩错误! 返回 " << decomp_len_default << " (" << getLzavErrorString(decomp_len_default) << ")" << std::endl;
        return 1;
    }

    std::cout << "解压缩数据长度: " << decomp_len_default << " 字节" << std::endl;
    std::cout << "解压缩耗时: " << decomp_duration_default.count() * 1000.0 << " 毫秒" << std::endl;
    if (decomp_duration_default.count() > 0) {
        double decomp_speed_mbps_default = (static_cast<double>(src_len) / (1024 * 1024)) / decomp_duration_default.count();
        std::cout << "解压缩速度: " << decomp_speed_mbps_default << " MB/秒" << std::endl;
    }

    // --- 4. 验证数据完整性 (默认压缩) ---
    if (static_cast<size_t>(decomp_len_default) == src_len &&
        std::equal(src_buf.begin(), src_buf.end(), decomp_buf_default.begin())) {
        std::cout << "数据验证 (默认): 通过" << std::endl;
    } else {
        std::cerr << "数据验证 (默认): 失败! 解压缩数据与原始数据不匹配。" << std::endl;
        return 1;
    }

    // --- 5. 测试更高比率压缩 (lzav_compress_hi) ---
    std::cout << "\n--- LZAV 更高比率压缩 (lzav_compress_hi) ---" << std::endl;

    int max_comp_len_hi = lzav_compress_bound_hi(static_cast<int>(src_len));
    if (max_comp_len_hi == 0 && src_len != 0) {
        std::cerr << "错误: lzav_compress_bound_hi 对非零源长度返回 0。" << std::endl;
        return 1;
    }
    std::vector<char> comp_buf_hi(max_comp_len_hi);

    auto start_comp_hi = std::chrono::high_resolution_clock::now();
    int comp_len_hi = lzav_compress_hi(
        src_buf.data(), 
        comp_buf_hi.data(), 
        static_cast<int>(src_len), 
        max_comp_len_hi
    );
    auto end_comp_hi = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> comp_duration_hi = end_comp_hi - start_comp_hi;

    if (comp_len_hi == 0 && src_len != 0) {
        std::cerr << "压缩 (高比率) 错误! lzav_compress_hi 返回 0。" << std::endl;
        return 1;
    } else if (comp_len_hi < 0) {
        std::cerr << "压缩 (高比率) 错误! lzav_compress_hi 返回: " << comp_len_hi << std::endl;
        return 1;
    }

    std::cout << "压缩数据 (高比率) 长度: " << comp_len_hi << " 字节" << std::endl;
    double compression_ratio_hi = static_cast<double>(comp_len_hi) / src_len * 100.0;
    std::cout << "压缩率 (高比率): " << compression_ratio_hi << "%" << std::endl;
    std::cout << "压缩耗时 (高比率): " << comp_duration_hi.count() * 1000.0 << " 毫秒" << std::endl;
    if (comp_duration_hi.count() > 0) {
        double comp_speed_mbps_hi = (static_cast<double>(src_len) / (1024 * 1024)) / comp_duration_hi.count();
        std::cout << "压缩速度 (高比率): " << comp_speed_mbps_hi << " MB/秒" << std::endl;
    }

    // --- 6. 解压缩数据 (高比率压缩) ---
    std::vector<char> decomp_buf_hi(src_len);

    auto start_decomp_hi = std::chrono::high_resolution_clock::now();
    int decomp_len_hi = lzav_decompress(
        comp_buf_hi.data(), 
        decomp_buf_hi.data(), 
        comp_len_hi, 
        static_cast<int>(src_len)
    );
    auto end_decomp_hi = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> decomp_duration_hi = end_decomp_hi - start_decomp_hi;

    if (decomp_len_hi < 0) {
        std::cerr << "解压缩 (高比率) 错误! 返回 " << decomp_len_hi << " (" << getLzavErrorString(decomp_len_hi) << ")" << std::endl;
        return 1;
    }

    std::cout << "解压缩数据 (高比率) 长度: " << decomp_len_hi << " 字节" << std::endl;
    std::cout << "解压缩耗时 (高比率): " << decomp_duration_hi.count() * 1000.0 << " 毫秒" << std::endl;
    if (decomp_duration_hi.count() > 0) {
        double decomp_speed_mbps_hi = (static_cast<double>(src_len) / (1024 * 1024)) / decomp_duration_hi.count();
        std::cout << "解压缩速度 (高比率): " << decomp_speed_mbps_hi << " MB/秒" << std::endl;
    }

    // --- 7. 验证数据完整性 (高比率压缩) ---
    if (static_cast<size_t>(decomp_len_hi) == src_len &&
        std::equal(src_buf.begin(), src_buf.end(), decomp_buf_hi.begin())) {
        std::cout << "数据验证 (高比率): 通过" << std::endl;
    } else {
        std::cerr << "数据验证 (高比率): 失败! 解压缩数据 (高比率) 与原始数据不匹配。" << std::endl;
        return 1;
    }

    return 0;
}
