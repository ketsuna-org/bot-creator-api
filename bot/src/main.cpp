#include <dpp/dpp.h>
#include <string>
#include <zmq.hpp>
#include "../include/utils.hpp"
#include <thread>

int main(int argc, char *argv[]) {
    if (argc > 1) {
        std::string token = argv[1];
        std::string port = argv[2];
        setenv("BOT_TOKEN", token.c_str(), 1);
        setenv("PORT", port.c_str(), 1);
    }

    const std::string BOT_TOKEN = getenv("BOT_TOKEN");
    const std::string PORT = getenv("PORT");

    dpp::cluster bot(BOT_TOKEN);
    std::unique_ptr<nlohmann::json> json_data = std::make_unique<nlohmann::json>();

    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([&json_data](const dpp::slashcommand_t &event) {
        std::unordered_map<std::string, std::string> key_values = app::generate_key_values(event);
        std::string response = "Interaction found, but no response found.";
        if (event.command.get_command_name() != "") {
            if (json_data->contains(event.command.get_command_name())) {
                std::cout << "Command found: " << event.command.get_command_name() << std::endl;
                auto command_data = json_data->at(event.command.get_command_name());
                if (command_data.contains("response")) {
                    std::cout << "Response found: " << command_data.at("response") << std::endl;
                    response = command_data.at("response");
                } else {
                    std::cout << "Command data: " << command_data.dump(4) << std::endl;
                    std::cout << "No response found for command: " << event.command.get_command_name() << std::endl;
                }
            }
        }
        event.reply(app::update_string(response, key_values));
    });

    bot.on_ready([&bot, &json_data, &PORT](const dpp::ready_t &event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            // Lancer la boucle ZMQ dans un thread séparé
            std::thread zmq_thread([&json_data, &PORT]() {
                zmq::context_t ctx;
                zmq::socket_t responder(ctx, zmq::socket_type::req);
                responder.connect("tcp://localhost:" + PORT);
                zmq::message_t ready_msg(5);
                memcpy(ready_msg.data(), "ready", 5);
                responder.send(ready_msg, zmq::send_flags::none);

                while (true) {
                    zmq::message_t reply;
                    if (responder.recv(reply, zmq::recv_flags::none)) {
                        std::string json_str(static_cast<char*>(reply.data()), reply.size());
                        try {
                            nlohmann::json j = app::json_from_string(json_str);
                            if (j.contains("command")) {
                                std::string command = j["command"];
                                if (command == "update") {
                                    json_data = std::make_unique<nlohmann::json>(j["data"]);
                                }
                            }
                            // Répondre de nouveau si nécessaire
                            zmq::message_t ping(4);
                            memcpy(ping.data(), "pong", 4);
                            responder.send(ping, zmq::send_flags::none);
                        } catch (const std::exception& e) {
                            std::cerr << "[BOT] Error parsing JSON: " << e.what() << std::endl;
                        }
                    }
                }

            });

            zmq_thread.detach(); // Le thread tourne en fond
        }
    });

    bot.start(dpp::st_wait);
}
