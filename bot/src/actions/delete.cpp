#include <dpp/dpp.h>

dpp::task<bool> delete_action(const dpp::slashcommand_t &event, const nlohmann::json &action, const std::unordered_map<std::string, std::string> &key_values, dpp::user &user_ptr, dpp::cluster *cluster)
{
    dpp::guild guild_ptr = event.command.get_guild();
    // let's retrieve the member.
    dpp::guild_member member_ptr = guild_ptr.members.find(user_ptr.id)->second;
    dpp::guild_member bot_member_ptr = guild_ptr.members.find(cluster->me.id)->second;
    std::unordered_map<std::string, std::string> error_messages = {
        {"error", "You need to wait a bit before deleting messages."},
        {"error_amount", "The amount of messages to delete must be between 1 and 100."},
        {"error_perm_channel", "You do not have permission to delete messages in this channel."}};

    if (action.contains("error_amount"))
    {
        error_messages["error_amount"] = action["error_amount"].get<std::string>();
    }

    if (action.contains("error_perm_channel"))
    {
        error_messages["error_perm_channel"] = action["error_perm_channel"].get<std::string>();
    }

    if (action.contains("error"))
    {
        error_messages["error"] = action["error"].get<std::string>();
    }
    // let's retrieve the current channel
    const dpp::channel *channel_ptr = &event.command.get_channel();
    auto user_as_perms = channel_ptr->get_user_permissions(member_ptr).has(dpp::p_manage_messages);
    auto bot_as_perms = channel_ptr->get_user_permissions(bot_member_ptr).has(dpp::p_manage_messages);
    if (!user_as_perms)
    {
        event.edit_response(error_messages["error_perm_channel"]);
        co_return false;
    }
    if (!bot_as_perms)
    {
        event.edit_response(error_messages["error_perm_channel"]);
        co_return false;
    }
    int amount = 0;
    if (action.contains("depend_on"))
    {
        std::string depend_on = action["depend_on"];
        auto it = key_values.find(depend_on);
        if (it != key_values.end())
        {
            std::string depend_on_value = it->second;

            // let's convert the depend_on_value to an int
            amount = std::stoi(depend_on_value);
            if (amount < 0 || amount > 100)
            {
                event.edit_response(error_messages["error_amount"]);
                co_return false;
            }
        }
    }
    if (amount > 0)
    {
        dpp::confirmation_callback_t callback = co_await cluster->co_messages_get(channel_ptr->id, 0, 0, 0, amount);
        if (callback.is_error())
        {
            printf("Error: %s\n", callback.get_error().message.c_str());
            event.edit_response(error_messages["error"]);
            co_return false;
        }
        auto messages = callback.get<dpp::message_map>();
        if (messages.empty())
        {
            event.edit_response("No messages to delete.");
            co_return false;
        }
        std::vector<dpp::snowflake> msg_ids;

        for (const auto &msg : messages)
        {
            // let's check if the message is older than 2 weeks
            if (msg.second.get_creation_time() < dpp::utility::time_f() - 1209600)
            {
                printf("Message is older than 2 weeks\n");
                continue;
            }
            else
            {
                msg_ids.push_back(msg.second.id);
            }
        }

        if (!msg_ids.empty())
        {
            dpp::confirmation_callback_t result;
            if (msg_ids.size() == 1)
            {
                result = co_await cluster->co_message_delete(msg_ids[0], channel_ptr->id);
            }
            else
            {
                result = co_await cluster->co_message_delete_bulk(msg_ids, channel_ptr->id);
            }
            if (result.is_error())
            {
                event.edit_response(error_messages["error"]);
                co_return false;
            }
        }
    }

    co_return true;
}
