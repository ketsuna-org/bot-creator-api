#include "../../include/utils.hpp"


const std::unordered_map<Lang, std::unordered_map<std::string, std::string>> error_messages_map = {
    {Lang::en, {
        {"error_no_messages", "No message to delete."},
        {"error", "You need to wait a bit before deleting messages."},
        {"error_amount", "The amount of messages to delete must be between 1 and 100."},
        {"error_perm_channel", "You do not have permission to delete messages in this channel."}
    }},
    {Lang::fr, {
        {"error_no_messages", "Aucun message à supprimer."},
        {"error", "Vous devez attendre un peu avant de supprimer des messages."},
        {"error_amount", "Le nombre de messages à supprimer doit être compris entre 1 et 100."},
        {"error_perm_channel", "Vous n'avez pas la permission de supprimer des messages dans ce canal."}
    }}
};

dpp::task<bool> delete_action(const dpp::slashcommand_t &event, const nlohmann::json &action,
                              const std::unordered_map<std::string, std::string> &key_values,
                              dpp::user &user_ptr, dpp::cluster *cluster)
{
    // setup locale for gettext
    std::string locale = event.command.locale;
    const dpp::channel *channel_ptr = &event.command.get_channel();
    const auto &guild_ptr = event.command.get_guild();

    const auto member_it = guild_ptr.members.find(user_ptr.id);
    const auto *member_ptr = (member_it != guild_ptr.members.end()) ? &member_it->second : nullptr;

    const auto bot_member_it = guild_ptr.members.find(cluster->me.id);
    const auto *bot_member_ptr = (bot_member_it != guild_ptr.members.end()) ? &bot_member_it->second : nullptr;

    const std::unordered_map<std::string, std::string> error_messages = [&action,&locale]()
    {
        std::unordered_map<std::string, std::string> defaults = {
            {"error_no_messages", app::translate("error_no_messages", locale, error_messages_map)},
            {"error", app::translate("error", locale, error_messages_map)},
            {"error_amount", app::translate("error_amount", locale, error_messages_map)},
            {"error_perm_channel", app::translate("error_perm_channel", locale, error_messages_map)},
        };
        if (action.contains("error_amount"))
            defaults["error_amount"] = action["error_amount"];
        if (action.contains("error_perm_channel"))
            defaults["error_perm_channel"] = action["error_perm_channel"];
        if (action.contains("error"))
            defaults["error"] = action["error"];
        return defaults;
    }();

    if (!member_ptr || !bot_member_ptr)
    {
        event.edit_response(error_messages.at("error_perm_channel"));
        co_return false;
    }

    const bool has_permissions = channel_ptr->get_user_permissions(*member_ptr).has(dpp::p_manage_messages) && channel_ptr->get_user_permissions(*bot_member_ptr).has(dpp::p_manage_messages);
    if (!has_permissions)
    {
        event.edit_response(error_messages.at("error_perm_channel"));
        co_return false;
    }

    int amount = 0;
    if (action.contains("depend_on"))
    {
        const std::string &depend_on = action["depend_on"];
        if (const auto it = key_values.find(depend_on); it != key_values.end())
        {
            try
            {
                amount = std::stoi(it->second);
                if (amount < 1 || amount > 100)
                {
                    event.edit_response(error_messages.at("error_amount"));
                    co_return false;
                }
            }
            catch (const std::exception &e)
            {
                event.edit_response(error_messages.at("error"));
                co_return false;
            }
        }
    }

    if (amount > 0)
    {
        const time_t two_weeks_ago = dpp::utility::time_f() - 1209600;

        auto callback = co_await cluster->co_messages_get(channel_ptr->id, 0, 0, 0, amount);
        if (callback.is_error())
        {
            event.edit_response(error_messages.at("error"));
            co_return false;
        }

        const auto &messages = callback.get<dpp::message_map>();
        if (messages.empty())
        {
            event.edit_response(error_messages.at("error_no_messages"));
            co_return false;
        }

        std::vector<dpp::snowflake> msg_ids;
        msg_ids.reserve(messages.size());

        for (const auto &[id, msg] : messages)
        {
            if (msg.get_creation_time() >= two_weeks_ago)
            {
                msg_ids.emplace_back(id);
            }
        }

        if (!msg_ids.empty())
        {
            const auto delete_result = msg_ids.size() == 1
                                           ? co_await cluster->co_message_delete(msg_ids.front(), channel_ptr->id)
                                           : co_await cluster->co_message_delete_bulk(msg_ids, channel_ptr->id);

            if (delete_result.is_error())
            {
                event.edit_response(error_messages.at("error"));
                co_return false;
            }
        }
    }

    co_return true;
}
