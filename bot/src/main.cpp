#include <dpp/dpp.h>
#include <string>
#include "../include/utils.hpp"
#include "../include/http_webhook_server.hpp"
#include "../include/handle_actions.hpp"
#include <thread>

int main(int argc, char* argv[]) {
    if (argc > 2) {
        setenv("BOT_TOKEN", argv[1], 1);
        setenv("PORT", argv[2], 1);
    }

    const std::string BOT_TOKEN = getenv("BOT_TOKEN");
    const std::string PORT = getenv("PORT");

    dpp::cluster bot(BOT_TOKEN);
    std::unique_ptr<nlohmann::json> json_data = std::make_unique<nlohmann::json>();

    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([&json_data, &bot](const dpp::slashcommand_t& event) -> dpp::task<void> {
        std::unordered_map<std::string, std::string> key_values = app::generate_key_values(event);
        std::string command_name = event.command.get_command_name();
        std::string response = "Interaction found, but no response found.";

        if (!command_name.empty() && json_data->contains(command_name)) {
            auto& command_data = (*json_data)[command_name];
            if (command_data.contains("actions")) {
                auto& action = command_data["actions"];
                // Actions are a list of Objects
                if (action.is_array()) {
                    std::cout << "Executing → Actions: " << action.dump() << std::endl;
                    auto already_returned_message = co_await handle_actions(event, action, key_values);
                    if(!already_returned_message) {
                        std::cout << "Command: " << command_name << " → Action: " << action.dump() << std::endl;
                        co_return;
                    }else {
                        // This mean we need to edit the response, not reply
                        if(command_data.contains("response")) {
                            response = command_data["response"];
                            std::cout << "Command: " << command_name << " → Response: " << response << std::endl;
                        }
                        event.edit_response(app::update_string(response, key_values));
                        co_return;
                    }
                }
            }
            if (command_data.contains("response")) {
                response = command_data["response"];
                std::cout << "Command: " << command_name << " → Response: " << response << std::endl;
            }
        }

        event.reply(app::update_string(response, key_values));
    });

    bot.on_ready([&bot, &json_data, &PORT](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_bot_commands>()) {
            std::thread http_thread([&json_data, &PORT]() {
                try {
                    HttpWebhookServer server(std::stoi(PORT), [&json_data](const HttpWebhookServer::HttpRequest& req) {
                        HttpWebhookServer::HttpResponse res;

                        if (req.method == "POST") {
                            res.status_code = 200;
                            res.headers["Content-Type"] = "application/json";

                            try {
                                nlohmann::json body_json = app::json_from_string(req.body);
                                res.body = R"({"received": "POST request received"})";

                                if (body_json.contains("command") && body_json["command"] == "update") {
                                    json_data = std::make_unique<nlohmann::json>(body_json["data"]);
                                }
                            } catch (const std::exception& e) {
                                res.status_code = 400;
                                res.body = std::string("{\"error\": \"") + e.what() + "\"}";
                            }
                        } else {
                            res.status_code = 400;
                            res.headers["Content-Type"] = "text/plain";
                            res.body = "Invalid request method.";
                        }

                        return res;
                    });

                    std::cout << "[BOT] Webhook server running on port " << PORT << "..." << std::endl;
                    server.start();
                } catch (const std::exception& e) {
                    std::cerr << "[BOT] Server error: " << e.what() << std::endl;
                }
            });

            http_thread.detach();
        }
    });

    bot.start(dpp::st_wait);
}
