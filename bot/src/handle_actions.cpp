#include "../include/actions/delete.hpp"
dpp::task<std::unordered_map<std::string, std::string>> handle_actions(const dpp::slashcommand_t &event, const nlohmann::json &actions, const std::unordered_map<std::string, std::string> &key_values)
{
    std::unordered_map<std::string, std::string> workflow_key_values = {{}};
    dpp::cluster *cluster = event.owner;
    dpp::user user_ptr = event.command.get_issuing_user();
    dpp::async thinking = event.co_thinking(false);
    if (actions.is_array())
    {
        int i = 0;
        for (const auto &action : actions)
        {
            i++;
            if (action.contains("type"))
            {
                std::string action_type = action["type"];
                if (action_type == "delete_messages" && event.command.is_guild_interaction())
                {
                    std::unordered_map<std::string, std::string> return_value = co_await delete_action(event, action, key_values, user_ptr, cluster);
                   co_await thinking;
                   // if it's a false, we need to return false !
                     if (return_value.contains("error"))
                     {
                          co_return return_value;
                     }
                     else
                     {
                          // return everything expect the success message
                            for (const auto &[key, value] : return_value)
                            {
                                if (key != "success")
                                {
                                    workflow_key_values[key] = value;
                                }
                            }
                     }


                }
            }
            if (i == actions.size())
            {

                co_await thinking;
                co_return workflow_key_values;
            }
        }
    }
    co_await thinking;
    co_return workflow_key_values;
}
