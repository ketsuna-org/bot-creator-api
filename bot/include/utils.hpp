// utils.hpp
#pragma once
#ifndef UTILS_HPP
#define UTILS_HPP

#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>
#include <map>
#include <string>
#include <regex>
#include <sstream>
#include <algorithm>
using namespace dpp;

enum class Lang { en, fr };

namespace app
{

    /**
     * @brief Creates a URL for a user's avatar
     *
     * @param u The user object
     * @return std::string The URL to the user's avatar
     */
    std::string make_avatar_url(const user &u);

    /**
     * @brief Creates a URL for a guild's icon
     *
     * @param g The guild object
     * @return std::string The URL to the guild's icon
     */
    std::string make_guild_icon(const guild &g);

    /**
     * @brief Updates a string by replacing placeholders with values from a map
     *
     * @param initial The initial string with placeholders in the format ((key))
     * @param updates A map of key-value pairs to replace placeholders
     * @return std::string The updated string with placeholders replaced
     */
    std::string update_string(const std::string &initial, const std::unordered_map<std::string, std::string> &updates);

    /**
     * @brief Processes a command option recursively and adds values to the key-value map
     *
     * @param event The slash command event
     * @param option The command option to process
     * @param kv The key-value map to update
     */
    void process_interaction_option(const slashcommand_t &event, const command_data_option &option, std::unordered_map<std::string, std::string> &kv);

    /**
     * @brief Generates a map of key-value pairs from a slash command event
     *
     * @param event The slash command event
     * @return std::map<std::string, std::string> A map containing information about the command, user, guild, and options
     */
    std::unordered_map<std::string, std::string> generate_key_values(const slashcommand_t &event);

    /**
     * @brief Handles actions specified in the slash command event
     *
     * @param event The slash command event
     * @param actions The JSON object containing actions to be handled
     * @param key_values The map of key-value pairs to be used in the actions
     */
    bool handle_actions(const slashcommand_t &event, const nlohmann::json &actions, const std::unordered_map<std::string, std::string> &key_values);

    /**
     * @brief Parses a JSON string into a JSON object
     *
     * @param str The JSON string to parse
     * @return nlohmann::json The parsed JSON object
     */
    nlohmann::json json_from_string(const std::string &str);

    /**
     * @brief Converts a JSON object into a string
     *
     * @param j The JSON object to convert
     * @return std::string The string representation of the JSON object
     */
    std::string string_from_json(const nlohmann::json &j);

    /**
     * @brief Gets the available locale for a given locale string
     *
     * @param locale The locale string to check
     * @return std::string The available locale or "en" if not found
     */
     std::string get_available_locale(std::string locale);

    /**
    * @brienf translate a string from a locale, optionnal parameters and a default value
    * @param str The string to translate to found in an Array of translations
    * @param locale The locale to use for translation
    * @param array_translations The array of translations
    * @param args The optional parameters to replace in the string
    * @return std::string The translated string
    */
    std::string translate(const std::string &str, const std::string &locale, const std::map<Lang, std::map<std::string, std::string>> &array_translations, const std::unordered_map<std::string, std::string> &args = {});


} // namespace dpp

#endif // UTILS_HPP
// utils.hpp
