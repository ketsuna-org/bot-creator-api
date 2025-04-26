#include <dpp/dpp.h>
#include "../include/utils.hpp"
const std::string BOT_TOKEN = getenv("BOT_TOKEN");

int main() {
    dpp::cluster bot(BOT_TOKEN);

    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        // let's generate the key-value map
        std::map<std::string, std::string> key_values = app::generate_key_values(event);
        // let's create a string to send
        std::string response = "Pong! ((userName))";

        if (event.command.get_command_name() == "ping") {
            event.reply(app::update_string(response, key_values));
        }
    });

    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
        }
    });

    bot.start(dpp::st_wait);
}
