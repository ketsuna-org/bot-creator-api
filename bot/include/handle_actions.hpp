#include <dpp/dpp.h>

dpp::task<std::unordered_map<std::string, std::string>> handle_actions(const dpp::slashcommand_t& event, const nlohmann::json& actions, const std::unordered_map<std::string, std::string>& key_values);
