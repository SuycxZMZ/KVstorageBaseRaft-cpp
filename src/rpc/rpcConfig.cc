#include "rpcConfig.h"

#include <cstddef>
#include <string>
#include "spdlog/spdlog.h"

rpcConfig& rpcConfig::GetInstance() {
    static rpcConfig instance;
    return instance;
}

void rpcConfig::LoadConfigFile(const char* config_file) {
    FILE* pf = fopen(config_file, "r");
    if (nullptr == pf) {
        spdlog::critical("{} does not exist!!!", config_file);
        exit(EXIT_FAILURE);
    }

    char buf[512];
    while (fgets(buf, sizeof(buf), pf) != nullptr) {
        // 去掉多余的空格
        std::string str_buf(buf);
        removeSpaces(str_buf);
        if (str_buf.empty() || str_buf[0] == '#') {
            continue;
        }

        // 解析配置项
        std::size_t idx = str_buf.find('=');
        if (std::string::npos == idx) {
            continue;
        }

        std::string key = str_buf.substr(0, idx);
        std::string value = str_buf.substr(idx + 1);
        if (!value.empty() && value.back() == '\n') {
            value.pop_back();  // 移除换行符
        }

        m_configMap.emplace(key, value);
    }

    fclose(pf);  // 确保文件被关闭

    for (const auto& info : m_configMap) {
        spdlog::info("{}: {}", info.first, info.second);
    }
}

// 获取配置项
std::string rpcConfig::Load(const std::string& key) {
    auto it = m_configMap.find(key);
    if (m_configMap.end() == it) return "";
    return it->second;
}

void removeSpaces(std::string& str) {
    size_t readPos = 0;
    size_t writePos = 0;

    // 遍历原始字符串
    while (readPos < str.length()) {
        // 如果当前字符不是空格，将其复制到处理后的字符串中
        if (str[readPos] != ' ') {
            str[writePos++] = str[readPos];
        }
        readPos++;  // 移动读指针
    }

    // 将处理后的字符串的末尾设置为'\0'，以表示字符串的结束
    str.resize(writePos);
}
