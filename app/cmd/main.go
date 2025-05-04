package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"

	"github.com/ketsuna-org/bot-creator-api/internal"
)

var botList = make(map[string]*internal.Bot)

func init() {
	// Initialisation de l'application
}

func main() {

	// Créer un ServeMux
	mux := http.NewServeMux()

	// Route principale
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("Hello, World!"))
	})

	// Route POST /create/{bot_token}
	mux.HandleFunc("POST /create/{bot_token}", func(w http.ResponseWriter, r *http.Request) {
		// Extraire le token du bot de l'URL
		botToken := r.PathValue("bot_token")

		// we need to retrieve the amount of bots running

		bot := &internal.Bot{
			BotToken:  botToken,
			ProcessID: fmt.Sprint(len(botList) + 5555), // or any unique identifier
		}

		// Let's check if this discord bot exists
		if _, ok := botList[botToken]; ok {
			log.Printf("[SERVER] Bot already running: %s", botToken)
			http.Error(w, "Bot already running", http.StatusConflict)
			return
		}

		// let's check if it exist on Discord
		httpClient := &http.Client{}
		req, err := http.NewRequest("GET", "https://discord.com/api/v10/users/@me", nil)
		if err != nil {
			log.Printf("[SERVER] Error creating request: %v", err)
			http.Error(w, "Error creating request", http.StatusInternalServerError)
			return
		}
		req.Header.Set("Authorization", "Bot "+botToken)
		resp, err := httpClient.Do(req)
		if err != nil {
			log.Printf("[SERVER] Error sending request: %v", err)
			http.Error(w, "Error sending request", http.StatusInternalServerError)
			return
		}
		defer resp.Body.Close()
		if resp.StatusCode != http.StatusOK {
			log.Printf("[SERVER] Bot not found: %s", botToken)
			http.Error(w, "Bot not found", http.StatusNotFound)
			return
		}
		// let's check if the bot is already running
		// let's parse the body.
		var body map[string]interface{}

		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			log.Printf("[SERVER] Error decoding JSON: %v", err)
			http.Error(w, "Invalid JSON", http.StatusBadRequest)
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
		}

		// let's convert the data to a string
		data, err := json.Marshal(body["data"])
		if err != nil {
			log.Printf("[SERVER] Error marshaling JSON: %v", err)
			http.Error(w, "Error marshaling JSON", http.StatusInternalServerError)
			return
		}

		fmt.Printf("[SERVER] Starting bot with data: %s", string(data))

		bot, err = internal.Start(bot, string(data), fmt.Sprint(body["intents"]))
		if err != nil {
			log.Printf("[SERVER] Error starting bot: %v", err)
			http.Error(w, "Error starting bot", http.StatusInternalServerError)
			return
		}
		botList[botToken] = bot
		log.Printf("[SERVER] Bot started successfully")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("Bot started successfully"))
	})

	// Route POST /stop/{bot_token}
	mux.HandleFunc("POST /stop/{bot_token}", func(w http.ResponseWriter, r *http.Request) {
		// Extraire le token du bot de l'URL
		botToken := r.PathValue("bot_token")

		bot, ok := botList[botToken]
		if !ok {
			http.Error(w, "Bot not found", http.StatusNotFound)
			return
		}
		if err := bot.Stop(); err != nil {
			log.Printf("[SERVER] Error stopping bot: %v", err)
			http.Error(w, "Error stopping bot", http.StatusInternalServerError)
			return
		}
		delete(botList, botToken)
		log.Printf("[SERVER] Bot stopped successfully")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("Bot stopped successfully"))
	})

	// Route POST /update/{bot_token}
	mux.HandleFunc("POST /update/{bot_token}", func(w http.ResponseWriter, r *http.Request) {
		// Extraire le token du bot de l'URL
		botToken := r.PathValue("bot_token")
		bot, ok := botList[botToken]
		if !ok {
			http.Error(w, "Bot not found", http.StatusNotFound)
			return
		}
		body := make(map[string]interface{})
		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			log.Printf("[SERVER] Error decoding JSON: %v", err)
			http.Error(w, "Invalid JSON", http.StatusBadRequest)
			return
		}
		data, err := json.Marshal(body)
		if err != nil {
			log.Printf("[SERVER] Error marshaling JSON: %v", err)
			http.Error(w, "Error marshaling JSON", http.StatusInternalServerError)
			return
		}
		if err := bot.SendMessage(string(data)); err != nil {
			log.Printf("[SERVER] Error sending message: %v", err)
			http.Error(w, "Error sending message", http.StatusInternalServerError)
			return
		}
		log.Printf("[SERVER] Bot updated successfully")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("Bot updated successfully"))
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
