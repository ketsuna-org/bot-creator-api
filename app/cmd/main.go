package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"

	"github.com/ketsuna-org/bot-creator-api/internal"
)

func init() {
	// Initialize the application
}

func main() {

	mux := http.NewServeMux()
	// Start the application
	mux.HandleFunc("GET /", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("Hello, World!"))
	})
	botId := "XXXXX"
	botToken := "XXXXX"

	bot := &internal.Bot{
		BotID:    botId,
		BotToken: botToken,
	}
	conn, err := internal.Start(bot)
	if err != nil {
		log.Fatalf("Error starting bot: %v", err)
	}
	defer conn.Close()
	// Handle the bot connection
	data, err := json.Marshal(map[string]interface{}{
		"command": "update",
		"data": map[string]interface{}{
			"ping": map[string]string{
				"response": "pong ((userName))",
			},
		},
	})
	if err != nil {
		log.Fatalf("Error marshaling JSON: %v", err)
	}
	conn.Write(data)

	// Handle if signal is received

	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt)
	signal.Notify(signals, os.Kill)
	go func() {
		sig := <-signals
		log.Printf("Received signal: %s", sig)
		// let's kill the bot
		if bot.Cmd != nil {
			if err := bot.Cmd.Process.Kill(); err != nil {
				log.Printf("Error killing bot process: %v", err)
			} else {
				log.Printf("Bot process killed successfully")
			}
		}
		// let's remove the socket
		socketPath := fmt.Sprintf("/tmp/%s.sock", bot.BotID)
		if err := os.RemoveAll(socketPath); err != nil {
			log.Printf("Error removing socket: %v", err)
		} else {
			log.Printf("Socket removed successfully")
		}
		os.Exit(0)
	}()
	panic(http.ListenAndServe(":2030", mux))
}
