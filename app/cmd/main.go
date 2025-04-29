package main

import (
	"encoding/json"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"

	"github.com/ketsuna-org/bot-creator-api/internal"
	zmq "github.com/pebbe/zmq4"
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
	botToken := "XXXXXXXXXXXX" // Replace with your bot token

	ctx, err := zmq.NewContext()
	if err != nil {
		log.Fatalf("[SERVER] Failed to create context: %v", err)
	}
	defer ctx.Term()

	dealer, err := ctx.NewSocket(zmq.REP)
	if err != nil {
		log.Fatalf("[SERVER] Failed to create dealer: %v", err)
	}
	defer dealer.Close()

	err = dealer.Bind("tcp://*:5555")
	if err != nil {
		log.Fatalf("[SERVER] Failed to bind dealer: %v", err)
	}

	bot := &internal.Bot{
		BotToken: botToken,
	}

	bot, err = internal.Start(bot, dealer)
	if err != nil {
		log.Fatalf("[SERVER] Error starting bot: %v", err)
	}
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
		log.Fatalf("[SERVER] Error marshaling JSON: %v", err)
	}
	go bot.SendMessage(string(data))

	dataX, err := json.Marshal(map[string]interface{}{
		"command": "update",
		"data": map[string]interface{}{
			"ping": map[string]string{
				"response": "pong ((userName)) avec une modif !",
			},
		},
	})
	if err != nil {
		log.Fatalf("[SERVER] Error marshaling JSON: %v", err)
	}
	go bot.SendMessage(string(dataX))
	// Handle if signal is received

	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt)
	signal.Notify(signals, syscall.SIGTERM)
	go func() {
		sig := <-signals
		log.Printf("Received signal: %s", sig)
		// let's kill the bot
		if bot.Cmd != nil {
			if err := bot.Cmd.Process.Kill(); err != nil {
				log.Printf("[SERVER] Error killing bot process: %v", err)
			} else {
				log.Printf("[SERVER] Bot process killed successfully")
			}
		}
		// let's remove the socket
		os.Exit(0)
	}()
	panic(http.ListenAndServe(":2030", mux))
}
