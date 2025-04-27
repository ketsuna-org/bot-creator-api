#include <dpp/dpp.h>
#include <string>
#include "../include/utils.hpp"
#include "../include/server.cpp"

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        std::string token = argv[1];
        setenv("BOT_TOKEN", token.c_str(), 1);
    }

    const std::string BOT_TOKEN = getenv("BOT_TOKEN");

    dpp::cluster bot(BOT_TOKEN);
    std::unique_ptr<UnixSocketServer> server;
    std::unique_ptr<nlohmann::json> json_data = std::make_unique<nlohmann::json>();
    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([&json_data](const dpp::slashcommand_t &event)
    {
        // let's generate the key-value map
        std::unordered_map<std::string, std::string> key_values = app::generate_key_values(event);
        // let's create a string to send
        std::string response = "Interaction found, but no response found.";
        // let's first check if it's a command or not.
        if(event.command.get_command_name() != ""){
            // let's check if the command is in the json_data
            // display json_data as string in the console
            if (json_data->contains(event.command.get_command_name()))
            {
                // let's check if it does exist
                std::cout << "Command found: " << event.command.get_command_name() << std::endl;
                auto command_data = json_data->at(event.command.get_command_name());
                if (command_data.contains("response"))
                {
                    std::cout << "Response found: " << command_data.at("response") << std::endl;
                    response = command_data.at("response");
                }else
                {
                    // let's display the command data
                    std::cout << "Command data: " << command_data.dump(4) << std::endl;
                    std::cout << "No response found for command: " << event.command.get_command_name() << std::endl;
                }
            }
        }
        event.reply(app::update_string(response, key_values));
    });

    bot.on_ready([&bot, &server, &json_data](const dpp::ready_t &event)
    {
        if (dpp::run_once<struct register_bot_commands>())
        {
            // let's start the server
            std::string socket_path = "/tmp/" + bot.me.id.str() + ".sock";
            server = std::make_unique<UnixSocketServer>(socket_path); // Création explicite

            server->start([&json_data, &server](const std::string& msg)
            {
                // Traitement du message reçu
                nlohmann::json j = app::json_from_string(msg);
                if (j.contains("command"))
                {
                    std::string command = j["command"];
                    // ... [traitement de la commande]
                    if (command == "update")
                    {
                        // ... [traitement de la commande update]
                        json_data = std::make_unique<nlohmann::json>(j["data"]);
                    }
                    else if (command == "stop")
                    {
                        server->stop();
                        std::cout << "Server stopped." << std::endl;
                        exit(0);
                    }
                    // ... [gestion des messages entrants]
                }
            });
        }
    });

    bot.start(dpp::st_wait);
}
