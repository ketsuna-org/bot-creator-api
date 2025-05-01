#include "../include/utils.hpp"

using namespace dpp;
namespace app
{
    // Helpers
    std::string make_avatar_url(const user &u)
    {
        return u.avatar.to_string().empty() ? u.get_default_avatar_url() : u.get_avatar_url(1024, i_webp, true);
    }

    std::string make_guild_icon(const guild &g)
    {
        return g.get_icon_url(1024, i_webp);
    }

    std::string update_string(const std::string &initial, const std::unordered_map<std::string, std::string> &updates)
    {
        static const std::regex placeholderRegex(R"(\(\((.*?)\)\))", std::regex::icase);

        std::string result;
        std::sregex_iterator it(initial.begin(), initial.end(), placeholderRegex);
        std::sregex_iterator end;

        size_t last_pos = 0;
        for (; it != end; ++it)
        {
            const auto &match = *it;
            result.append(initial, last_pos, match.position() - last_pos);

            std::string content = match[1].str();
            std::vector<std::string> keys;
            std::stringstream ss(content);
            std::string key;
            bool replaced = false;

            while (std::getline(ss, key, '|'))
            {
                key = trim(key);
                auto found = updates.find(key);
                if (found != updates.end())
                {
                    result.append(found->second);
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
            {
                // Aucune clé trouvée : chaîne vide
            }

            last_pos = match.position() + match.length();
        }
        result.append(initial, last_pos, std::string::npos);
        return result;
    }
    // Forward declaration
    void process_interaction_option(const slashcommand_t &event, const command_data_option &option, std::unordered_map<std::string, std::string> &kv);

    // Génère la map clé/valeur
    std::unordered_map<std::string, std::string> generate_key_values(const slashcommand_t &event)
    {
        std::unordered_map<std::string, std::string> key_values;
        const guild *g = event.command.is_guild_interaction() ? &event.command.get_guild() : nullptr;
        const channel *channel_ptr = event.command.is_guild_interaction() ? &event.command.get_channel() : nullptr;
        const user &u = event.command.get_issuing_user();
        key_values["commandName"] = event.command.get_command_name();
        key_values["commandId"] = event.command.id.str();
        key_values["commandType"] = std::to_string(event.command.type);
        key_values["userName"] = u.username;
        key_values["userId"] = u.id.str();
        key_values["userAvatar"] = make_avatar_url(u);
        key_values["guildName"] = g ? g->name : "DM";
        key_values["channelName"] = channel_ptr ? channel_ptr->name : "DM";
        key_values["channelId"] = channel_ptr ? channel_ptr->id.str() : "0";
        key_values["channelType"] = channel_ptr ? std::to_string(channel_ptr->get_type()) : "0";
        key_values["guildId"] = g ? g->id.str() : "0";
        key_values["guildIcon"] = g ? make_guild_icon(*g) : "";
        key_values["guildCount"] = g ? std::to_string(g->member_count) : "0";
        key_values["guildOwner"] = g ? g->owner_id.str() : "0";
        key_values["guildCreatedAt"] = g ? std::to_string(g->get_creation_time()) : "0";
        key_values["guildBoostTier"] = g ? std::to_string(g->premium_tier) : "0";
        key_values["guildBoostCount"] = g ? std::to_string(g->premium_subscription_count) : "0";

        // Options de commande
        for (const auto &option : event.command.get_command_interaction().options)
        {
            process_interaction_option(event, option, key_values);
        }
        return key_values;
    }

    // Traite une option d'interaction récursivement
    void process_interaction_option(const slashcommand_t &event, const command_data_option &option, std::unordered_map<std::string, std::string> &kv)
    {
        switch (option.type)
        {
        case co_sub_command:
        case co_sub_command_group:
            for (const auto &subopt : option.options)
            {
                process_interaction_option(event, subopt, kv);
            }
            break;
        case co_user:
        {
            snowflake user_id = std::get<snowflake>(option.value);
            auto user_ptr = event.command.get_resolved_user(user_id);
            const user &u = user_ptr;
            kv["opts." + option.name] = u.username;
            kv["opts." + option.name + ".id"] = u.id.str();
            kv["opts." + option.name + ".avatar"] = make_avatar_url(u);
            kv["opts." + option.name + ".discriminator"] = std::to_string(u.discriminator);
            kv["opts." + option.name + ".bot"] = u.is_bot() ? "true" : "false";
            kv["opts." + option.name + ".created_at"] = std::to_string(u.get_creation_time());
        }
        break;
        case co_channel:
        {
            snowflake chan_id = std::get<snowflake>(option.value);
            auto chan_ptr = event.command.get_resolved_channel(chan_id);
            const channel &c = chan_ptr;
            kv["opts." + option.name] = c.name;
            kv["opts." + option.name + ".id"] = c.id.str();
            kv["opts." + option.name + ".type"] = std::to_string(c.get_type());
            kv["opts." + option.name + ".created_at"] = std::to_string(c.get_creation_time());
        }
        break;
        case co_role:
        {
            snowflake role_id = std::get<snowflake>(option.value);
            auto role_ptr = event.command.get_resolved_role(role_id);
            const role &r = role_ptr;
            kv["opts." + option.name] = r.name;
            kv["opts." + option.name + ".id"] = r.id.str();
            kv["opts." + option.name + ".color"] = std::to_string(r.colour);
            kv["opts." + option.name + ".hoist"] = r.is_hoisted() ? "true" : "false";
            kv["opts." + option.name + ".position"] = std::to_string(r.position);
        }
        break;
        case co_mentionable:
        {
            snowflake mentionable_id = std::get<snowflake>(option.value);
            auto member_ptr = event.command.get_resolved_member(mentionable_id);
            const user &u = *member_ptr.get_user();
            kv["opts." + option.name] = u.username;
            kv["opts." + option.name + ".id"] = u.id.str();
            kv["opts." + option.name + ".avatar"] = make_avatar_url(u);
            kv["opts." + option.name + ".discriminator"] = std::to_string(u.discriminator);
            kv["opts." + option.name + ".bot"] = u.is_bot() ? "true" : "false";
            kv["opts." + option.name + ".created_at"] = std::to_string(u.get_creation_time());
            kv["opts." + option.name + ".nick"] = member_ptr.get_nickname();
            kv["opts." + option.name + ".joined_at"] = std::to_string(member_ptr.joined_at);
        }
        break;
        case co_string:
            kv["opts." + option.name] = std::get<std::string>(option.value);
            break;
        case co_integer:
            kv["opts." + option.name] = std::to_string(std::get<int64_t>(option.value));
            break;
        case co_boolean:
            kv["opts." + option.name] = std::get<bool>(option.value) ? "true" : "false";
            break;
        case co_number:
            kv["opts." + option.name] = std::to_string(std::get<double>(option.value));
            break;
        case co_attachment:
        {
            snowflake attachment_id = std::get<snowflake>(option.value);
            auto att_ptr = event.command.get_resolved_attachment(attachment_id);
            kv["opts." + option.name] = att_ptr.url;
            kv["opts." + option.name + ".id"] = att_ptr.id.str();
            kv["opts." + option.name + ".filename"] = att_ptr.filename;
            kv["opts." + option.name + ".size"] = std::to_string(att_ptr.size);
        }
        break;
        }
    }

    nlohmann::json json_from_string(const std::string &str)
    {
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(str);
        }
        catch (const nlohmann::json::parse_error &e)
        {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }
        return j;
    }

    std::string string_from_json(const nlohmann::json &j)
    {
        std::string str;
        try
        {
            str = j.dump();
        }
        catch (const nlohmann::json::exception &e)
        {
            std::cerr << "JSON exception: " << e.what() << std::endl;
        }
        return str;
    }

    Lang get_available_locale(std::string locale)
    {
        std::transform(locale.begin(), locale.end(), locale.begin(), ::tolower);
        if (locale == "fr" || locale == "fr-fr")
            return Lang::fr;
        else if (locale == "en" || locale == "en-us")
            return Lang::en;
        else
            return Lang::en; // Default to English if no match is found
    }

    std::string translate(const std::string &str, const std::string &locale, const std::unordered_map<Lang, std::unordered_map<std::string, std::string>> &array_translations, const std::unordered_map<std::string, std::string> &args)
    {
        Lang lang = get_available_locale(locale);
        auto it = array_translations.find(lang);
        if (it != array_translations.end())
        {
            auto it2 = it->second.find(str);
            if (it2 != it->second.end())
            {
                std::string translation = it2->second;
                for (const auto &arg : args)
                {
                    translation = update_string(translation, args);
                }
                return translation;
            }
        }
        return str; // Return the original string if no translation is found
    }
}
