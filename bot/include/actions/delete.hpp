#include <dpp/dpp.h>

dpp::task<bool> delete_action(const dpp::slashcommand_t &event, const nlohmann::json &action, const std::unordered_map<std::string, std::string> &key_values, dpp::user &user_ptr, dpp::cluster *cluster);
