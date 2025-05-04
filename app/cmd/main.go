package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"

	"github.com/arangodb/go-driver/v2/arangodb"
	"github.com/arangodb/go-driver/v2/connection"
	"github.com/ketsuna-org/bot-creator-api/internal"
	"github.com/ketsuna-org/bot-creator-api/internal/utils"
)

var botList = make(map[string]*internal.Bot)
var database arangodb.Database

func init() {
	// Initialisation de l'application
	password := os.Getenv("DB_PASSWORD")
	user := os.Getenv("DB_USERNAME")
	host := os.Getenv("DB_HOST")
	dbName := os.Getenv("DB_NAME")
	if password == "" || user == "" || host == "" || dbName == "" {
		// display which env variable is missing
		array := []string{"DB_PASSWORD", "DB_USERNAME", "DB_HOST", "DB_NAME"}
		for _, v := range array {
			if os.Getenv(v) == "" {
				log.Printf("[SERVER] %s is missing", v)
			}
		}
	}

	// Connexion à la base de données
	conn := arangodb.NewClient(connection.NewHttp2Connection(connection.Http2Configuration{
		Endpoint: connection.NewRoundRobinEndpoints([]string{
			fmt.Sprintf("https://%s", host),
		}),
		Authentication: connection.NewBasicAuth(user, password),
	}))

	db, err := conn.GetDatabase(context.Background(), dbName, nil)
	if err != nil {
		log.Fatalf("Failed to open database: %v", err)
	}

	database = db
}

func main() {

	// Créer un ServeMux
	mux := http.NewServeMux()

	// Route principale
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("Hello, World!"))
	})

	// Route POST /create/{bot_token}
	//![TODO] : Every bot_token will be replaced by the bot id. to be compliant with Discord TOS, and Token will be send as a secured Header.
	mux.HandleFunc("POST /create/{bot_token}", func(w http.ResponseWriter, r *http.Request) {
		// Extraire le token du bot de l'URL
		botToken := r.PathValue("bot_token")

		// we need to retrieve the amount of bots running

		bot := &internal.Bot{
			BotToken:  botToken,
			ProcessID: fmt.Sprint(len(botList) + 5555), // or any unique identifier
		}

		// let's start the bot
		col, err := database.GetCollection(context.Background(), "bots", nil)
		if err != nil {
			log.Printf("[SERVER] Error getting collection: %v", err)
			utils.RespondWithError(w, http.StatusInternalServerError, "Error getting collection")
			return
		}
		// let's check if the bot already exists

		// Let's check if this discord bot exists
		if _, ok := botList[botToken]; ok {
			log.Printf("[SERVER] Bot already running: %s", botToken)
			utils.RespondWithError(w, http.StatusConflict, "Bot already running")
			return
		}

		// let's check if it exist on Discord
		httpClient := &http.Client{}
		req, err := http.NewRequest("GET", "https://discord.com/api/v10/users/@me", nil)
		if err != nil {
			log.Printf("[SERVER] Error creating request: %v", err)
			utils.RespondWithError(w, http.StatusInternalServerError, "Error creating request")
			return
		}
		req.Header.Set("Authorization", "Bot "+botToken)
		resp, err := httpClient.Do(req)
		if err != nil {
			log.Printf("[SERVER] Error sending request: %v", err)
			utils.RespondWithError(w, http.StatusInternalServerError, "Error sending request")
			return
		}
		defer resp.Body.Close()
		if resp.StatusCode != http.StatusOK {
			log.Printf("[SERVER] Bot not found: %s", botToken)
			utils.RespondWithError(w, http.StatusNotFound, "Bot not found")
			return
		}

		var botData map[string]interface{}
		if err := json.NewDecoder(resp.Body).Decode(&botData); err != nil {
			log.Printf("[SERVER] Error decoding JSON: %v", err)
			utils.RespondWithError(w, http.StatusBadRequest, "Invalid JSON")
			return
		}

		botData["_key"] = botData["id"]
		id := botData["id"].(string)
		botData["token"] = botToken
		exist, err := col.DocumentExists(context.Background(), id)
		if err != nil {
			log.Printf("[SERVER] Error checking document: %v", err)
			utils.RespondWithError(w, http.StatusInternalServerError, "Error checking document")
			return
		}

		if !exist { // let's create the bot
			_, err = col.CreateDocument(context.Background(), botData)
			if err != nil {
				log.Printf("[SERVER] Error creating bot: %v", err)
				utils.RespondWithError(w, http.StatusInternalServerError, "Error creating bot")
				return
			}
			log.Printf("[SERVER] Bot created: %s", botToken)
		} else {
			// let's update the bot
			_, err = col.UpdateDocument(context.Background(), botData["id"].(string), botData)
			if err != nil {
				log.Printf("[SERVER] Error updating document: %v", err)
				utils.RespondWithError(w, http.StatusInternalServerError, "Error updating document")
				return
			}
			log.Printf("[SERVER] Bot updated: %s", botToken)
		}
		// let's check if the bot is already running
		// let's parse the body.
		var body map[string]interface{}

		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			log.Printf("[SERVER] Error decoding JSON: %v", err)
			utils.RespondWithError(w, http.StatusBadRequest, "Invalid JSON")
			return
		}

		if body["intents"] == nil {
			// intents are the default ones we can set the default ones
			body["intents"] = "3243773"
		}
		if body["data"] == nil {
			// data are the default ones we can set the default ones
			fmt.Printf("[SERVER] No data found, setting default ones")
			body["data"] = map[string]interface{}{}
		} else {
			// let's create each commands associated with the bot, and it's corresponding commands ID
			// let's check if the data is a map
			dataToUse, ok := body["data"].(map[string]interface{})
			if !ok {
				log.Printf("[SERVER] Data is not a map")
				utils.RespondWithError(w, http.StatusBadRequest, "Data is not a map")
				return
			}
			// let's check if the data is a map
			// each command is a key. and the value is a map
			for key, value := range dataToUse {
				// let's check if the value is a map
				valueToUse, ok := value.(map[string]interface{})
				if !ok {
					log.Printf("[SERVER] Value is not a map")
					utils.RespondWithError(w, http.StatusBadRequest, "Value is not a map")
					return
				}

				valueToUse["_key"] = key

				col, err := database.GetCollection(context.Background(), "commands", nil)
				// let's check if the name is set
				if err != nil {
					log.Printf("[SERVER] Error getting collection: %v", err)
					utils.RespondWithError(w, http.StatusInternalServerError, "Error getting collection")
					return
				}

				// first check if the command already exists
				exist, err := col.DocumentExists(context.Background(), key)
				if err != nil {
					log.Printf("[SERVER] Error checking document: %v", err)
					utils.RespondWithError(w, http.StatusInternalServerError, "Error checking document")
					return
				}
				if !exist {
					// this mean we can create the command
					// let's create the command
					_, err = col.CreateDocument(context.Background(), valueToUse)
					if err != nil {
						log.Printf("[SERVER] Error creating document: %v", err)
						utils.RespondWithError(w, http.StatusInternalServerError, "Error creating document")
						return
					}
					log.Printf("[SERVER] Command created: %s", key)
				} else {
					// let's update the command
					_, err = col.UpdateDocument(context.Background(), key, valueToUse)
					if err != nil {
						log.Printf("[SERVER] Error updating document: %v", err)
						utils.RespondWithError(w, http.StatusInternalServerError, "Error updating document")
						return
					}
					log.Printf("[SERVER] Command updated: %s", key)
				}

				// let's create edge or update the edge
				edgeCol, err := database.GetCollection(context.Background(), "bots_commands", nil)
				if err != nil {
					log.Printf("[SERVER] Error getting collection: %v", err)
					utils.RespondWithError(w, http.StatusInternalServerError, "Error getting collection")
					return
				}
				// let's check if the edge already exists
				edgeID := fmt.Sprintf("%s_%s", id, key)
				exist, err = edgeCol.DocumentExists(context.Background(), edgeID)
				if err != nil {
					log.Printf("[SERVER] Error checking document: %v", err)
					http.Error(w, "Error checking document", http.StatusInternalServerError)
					return
				}

				if !exist {
					edge := map[string]interface{}{
						"_from": "bots/" + id,
						"_to":   "commands/" + key,
						"_key":  edgeID,
					}
					_, err = edgeCol.CreateDocument(context.Background(), edge)
					if err != nil {
						log.Printf("[SERVER] Error creating document: %v", err)
						utils.RespondWithError(w, http.StatusInternalServerError, "Error creating document")
						return
					}
					log.Printf("[SERVER] Edge created: %s", edgeID)
				} else {
					// let's update the edge
					edge := map[string]interface{}{
						"_from": "bots/" + id,
						"_to":   "commands/" + key,
					}
					_, err = edgeCol.UpdateDocument(context.Background(), edgeID, edge)
					if err != nil {
						log.Printf("[SERVER] Error updating document: %v", err)
						utils.RespondWithError(w, http.StatusInternalServerError, "Error updating document")
						return
					}
					log.Printf("[SERVER] Edge updated: %s", edgeID)
				}
			}
		}

		// let's convert the data to a string
		data, err := json.Marshal(body["data"])
		if err != nil {
			log.Printf("[SERVER] Error marshaling JSON: %v", err)
			utils.RespondWithError(w, http.StatusInternalServerError, "Error marshaling JSON")
			return
		}

		fmt.Printf("[SERVER] Starting bot with data: %s", string(data))

		bot, err = internal.Start(bot, string(data), fmt.Sprint(body["intents"]))
		if err != nil {
			log.Printf("[SERVER] Error starting bot: %v", err)
			utils.RespondWithError(w, http.StatusInternalServerError, "Error starting bot")
			return
		}
		botList[botToken] = bot
		log.Printf("[SERVER] Bot started successfully")
		utils.RespondWithJSON(w, bot, http.StatusOK)
	})

	// Route POST /stop/{bot_token}
	//![TODO] : Every bot_token will be replaced by the bot id. to be compliant with Discord TOS, and Token will be send as a secured Header.
	mux.HandleFunc("POST /stop/{bot_token}", func(w http.ResponseWriter, r *http.Request) {
		// Extraire le token du bot de l'URL
		botToken := r.PathValue("bot_token")

		bot, ok := botList[botToken]
		if !ok {
			log.Printf("[SERVER] Bot not found: %s", botToken)
			utils.RespondWithError(w, http.StatusNotFound, "Bot not found")
			return
		}
		if err := bot.Stop(); err != nil {
			log.Printf("[SERVER] Error stopping bot: %v", err)
			utils.RespondWithError(w, http.StatusInternalServerError, "Error stopping bot")
			return
		}
		delete(botList, botToken)
		log.Printf("[SERVER] Bot stopped successfully")
		utils.RespondWithJSON(w, bot, http.StatusOK)
	})

	// Route POST /update/{bot_token}
	//![TODO] : Every bot_token will be replaced by the bot id. to be compliant with Discord TOS, and Token will be send as a secured Header.
	mux.HandleFunc("POST /update/{bot_token}", func(w http.ResponseWriter, r *http.Request) {
		// Extraire le token du bot de l'URL
		botToken := r.PathValue("bot_token")
		bot, ok := botList[botToken]
		if !ok {
			utils.RespondWithError(w, http.StatusNotFound, "Bot not found")
			return
		}
		body := make(map[string]interface{})
		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			log.Printf("[SERVER] Error decoding JSON: %v", err)
			utils.RespondWithError(w, http.StatusBadRequest, "Invalid JSON")
			return
		}
		data, err := json.Marshal(body)
		if err != nil {
			log.Printf("[SERVER] Error marshaling JSON: %v", err)
			utils.RespondWithError(w, http.StatusInternalServerError, "Error marshaling JSON")
			return
		}
		if err := bot.SendMessage(string(data)); err != nil {
			log.Printf("[SERVER] Error sending message: %v", err)
			utils.RespondWithError(w, http.StatusInternalServerError, "Error sending message")
			return
		}
		log.Printf("[SERVER] Bot updated successfully")
		utils.RespondWithJSON(w, bot, http.StatusOK)
	})

	// Gestion des signaux pour l'arrêt propre
	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt)
	signal.Notify(signals, syscall.SIGTERM)
	go func() {
		sig := <-signals
		log.Printf("Received signal: %s", sig)
		// Arrêter tous les bots en cours
		for _, bot := range botList {
			if err := bot.Stop(); err != nil {
				log.Printf("[SERVER] Error stopping bot: %v", err)
			}
			delete(botList, bot.BotToken)
		}
		// Quitter l'application
		os.Exit(0)
	}()

	// Démarrer le serveur HTTP
	log.Printf("[SERVER] Starting server on :2030")
	log.Fatal(http.ListenAndServe(":2030", mux))
}
