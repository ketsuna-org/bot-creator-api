#include "../include/handle_actions.hpp"
#include "../include/http_webhook_server.hpp"
#include "../include/utils.hpp"
#include <thread>
#include <unordered_map>

dpp::activity_type activity_type_from_string(const std::string &type) {
  if (type == "playing") {
    return dpp::activity_type::at_game;
  } else if (type == "streaming") {
    return dpp::activity_type::at_streaming;
  } else if (type == "listening") {
    return dpp::activity_type::at_listening;
  } else if (type == "watching") {
    return dpp::activity_type::at_watching;
  } else if (type == "custom") {
    return dpp::activity_type::at_custom;
  } else if (type == "competing") {
    return dpp::activity_type::at_competing;
  } else {
    return dpp::activity_type::at_game; // Default to "playing" if the type is
                                        // unknown
  }
}

int main(int argc, char *argv[]) {
  int intents = dpp::i_default_intents;
  std::unique_ptr<nlohmann::json> json_data =
      std::make_unique<nlohmann::json>();
  if (argc > 2) {
    setenv("BOT_TOKEN", argv[1], 1);
    setenv("PORT", argv[2], 1);
    if (argc > 3) {
      json_data =
          std::make_unique<nlohmann::json>(nlohmann::json::parse(argv[3]));
    }
    if (argc > 4) {
      intents = std::stoi(argv[4]);
    }
  }

  const std::string BOT_TOKEN = getenv("BOT_TOKEN");
  const std::string PORT = getenv("PORT");
  dpp::cluster bot(BOT_TOKEN, intents);

  bot.on_log(dpp::utility::cout_logger());

  bot.on_slashcommand([&json_data, &bot](
                          const dpp::slashcommand_t &event) -> dpp::task<void> {
    std::unordered_map<std::string, std::string> key_values =
        app::generate_key_values(event);
    std::string command_name = event.command.get_command_name();
    std::string response = "Interaction found, but no response found.";

    if (!command_name.empty() && json_data->contains(command_name)) {
      auto &command_data = (*json_data)[command_name];
      if (command_data.contains("actions")) {
        auto &action = command_data["actions"];
        // Actions are a list of Objects
        if (action.is_array()) {
          std::cout << "Executing → Actions: " << action.dump() << std::endl;
          std::unordered_map<std::string, std::string>
              already_returned_message =
                  co_await handle_actions(event, action, key_values);
          if (already_returned_message.contains("error")) {
            std::cout << "Error: " << already_returned_message["error"]
                      << std::endl;
          } else {
            std::cout << "Actions executed successfully." << std::endl;
            // let's push the values to the key_values
            for (const auto &[key, value] : already_returned_message) {
              if (key != "success") {
                key_values[key] = value;
              }
            }
          }
        }
        // let's retrieve final response.
        response = command_data["response"];
        auto update_response = app::update_string(response, key_values);
        std::cout << "Executing → Response: " << update_response << std::endl;
        co_return event.edit_response(update_response);
      }
      if (command_data.contains("response")) {
        response = command_data["response"];
      }
      //
    }

    auto update_response = app::update_string(response, key_values);
    std::cout << "Executing → Response: " << update_response << std::endl;
    co_return event.reply(update_response);
  });

  bot.on_ready([&bot, &json_data, &PORT](const dpp::ready_t &event) {
    if (dpp::run_once<struct register_bot_commands>()) {
      std::thread http_thread([&json_data, &PORT, &bot]() {
        try {
          HttpWebhookServer server(std::stoi(PORT), [&json_data, &bot](
                                                        const HttpWebhookServer::
                                                            HttpRequest &req) {
            HttpWebhookServer::HttpResponse res;

            if (req.method == "POST") {
              res.status_code = 200;
              res.headers["Content-Type"] = "application/json";

              try {
                nlohmann::json body_json = app::json_from_string(req.body);
                res.body = R"({"received": "POST request received"})";

                if (body_json.contains("command")) {
                  if (body_json["command"] == "update") {
                    json_data =
                        std::make_unique<nlohmann::json>(body_json["data"]);
                  } else if (body_json["command"] == "update_status") {
                    std::string status = body_json.contains("status")
                                             ? body_json["status"]
                                             : "online";
                    std::string activity = body_json.contains("activity")
                                               ? body_json["activity"]
                                               : "";
                    std::string activity_status =
                        body_json.contains("activity_status")
                            ? body_json["activity_status"]
                            : "";
                    std::string activity_url =
                        body_json.contains("activity_url")
                            ? body_json["activity_url"]
                            : "";
                    std::string activity_type =
                        body_json.contains("activity_type")
                            ? body_json["activity_type"]
                            : "playing";
                    dpp::presence p;
                    if (status == "online") {
                      p = dpp::presence(
                          dpp::presence_status::ps_online,
                          dpp::activity(
                              activity_type_from_string(activity_type),
                              activity, activity_status, activity_url));
                    } else if (status == "offline") {
                      p = dpp::presence(
                          dpp::presence_status::ps_offline,
                          dpp::activity(
                              activity_type_from_string(activity_type),
                              activity, activity_status, activity_url));
                    } else if (status == "dnd") {
                      p = dpp::presence(
                          dpp::presence_status::ps_dnd,
                          dpp::activity(
                              activity_type_from_string(activity_type),
                              activity, activity_status, activity_url));
                    } else if (status == "idle") {
                      p = dpp::presence(
                          dpp::presence_status::ps_idle,
                          dpp::activity(
                              activity_type_from_string(activity_type),
                              activity, activity_status, activity_url));
                    } else if (status == "invisible") {
                      p = dpp::presence(
                          dpp::presence_status::ps_invisible,
                          dpp::activity(
                              activity_type_from_string(activity_type),
                              activity, activity_status, activity_url));
                    }
                    bot.set_presence(p);
                  }
                  res.body =
                      R"({"status": "success", "message": "Command executed successfully"})";
                }
              } catch (const std::exception &e) {
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

          std::cout << "[BOT] Webhook server running on port " << PORT << "..."
                    << std::endl;
          server.start();
        } catch (const std::exception &e) {
          std::cerr << "[BOT] Server error: " << e.what() << std::endl;
        }
      });

      http_thread.detach();
    }
  });

  bot.start(dpp::st_wait);
}
