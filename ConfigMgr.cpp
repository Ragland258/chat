#include "ConfigMgr.h"
#include <algorithm>
#include <cctype>

namespace
{
    bool IsSensitiveConfigKey(std::string key)
    {
        std::transform(
            key.begin(),
            key.end(),
            key.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });

        return key.find("password") != std::string::npos ||
            key.find("passwd") != std::string::npos ||
            key.find("pass") != std::string::npos ||
            key.find("token") != std::string::npos ||
            key.find("secret") != std::string::npos ||
            key.find("auth") != std::string::npos ||
            key.find("key") != std::string::npos;
    }
}
bool ConfigMgr::LoadConfig(const std::string& config)
{
    try
    {
        boost::filesystem::path current_path = boost::filesystem::current_path();
        boost::filesystem::path config_path = current_path / config;

        boost::property_tree::ptree pt;
        boost::property_tree::read_ini(config_path.string(), pt);

        config_map_.clear();

        for (const auto& section_pair : pt)
        {
            const std::string& section_name = section_pair.first;
            const boost::property_tree::ptree& section_tree = section_pair.second;

            SectionInfo section_info;

            for (const auto& key_value : section_tree)
            {
                const std::string& key = key_value.first;
                std::string value = key_value.second.get_value<std::string>();

                section_info.section_data_[key] = value;
            }

            config_map_[section_name] = section_info;
        }
        // Print loaded config for debugging, but never expose secrets.
        for (const auto& section_entry : config_map_)
        {
            const auto& section_name = section_entry.first;
            auto section_config = section_entry.second;
            std::cout << "[" << section_name << "]" << std::endl;
            for (const auto& key_value : section_config.section_data_)
            {
                if (IsSensitiveConfigKey(key_value.first))
                {
                    std::cout
                        << key_value.first
                        << "=<hidden, len="
                        << key_value.second.size()
                        << ">"
                        << std::endl;
                }
                else
                {
                    std::cout
                        << key_value.first
                        << "="
                        << key_value.second
                        << std::endl;
                }
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        std::cout << "LoadConfig failed: " << e.what() << std::endl;
        return false;
    }
}