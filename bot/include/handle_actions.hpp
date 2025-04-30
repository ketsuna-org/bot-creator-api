#include <dpp/dpp.h>

dpp::task<bool> handle_actions(const dpp::slashcommand_t& event, const nlohmann::json& actions, const std::unordered_map<std::string, std::string>& key_values);
