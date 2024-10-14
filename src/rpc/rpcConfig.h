#ifndef RPCCONFIG_H
#define RPCCONFIG_H

#include <string>
#include <unordered_map>

void removeSpaces(std::string& str);

class rpcConfig {
   public:
    // 加载配置文件
    void LoadConfigFile(const char* config_file);

    // 取ip和端口号
    std::string Load(const std::string& key);
    static rpcConfig& GetInstance();
   private:
    rpcConfig() = default;
    std::unordered_map<std::string, std::string> m_configMap;
};

#endif