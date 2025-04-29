package internal

import (
	"fmt"
	"log"
	"os"
	"os/exec"
	"syscall"

	zmq "github.com/pebbe/zmq4"
)

type Bot struct {
	BotToken  string    `json:"bot_token"`
	Cmd       *exec.Cmd // Ajouter une référence à la commande
	ProcessID string
	dealer    *zmq.Socket // Stocker le PGID (Process Group ID)
	read      bool
	readyChan chan bool // Canal pour indiquer que le bot est prêt
}

func Start(b *Bot) (*Bot, error) {
	if b.readyChan == nil {
		b.readyChan = make(chan bool)
	}

	// Create a new ZeroMQ socket specifically for this bot
	ctx, err := zmq.NewContext()
	if err != nil {
		return nil, fmt.Errorf("[SERVER] failed to create context: %w", err)
	}

	// Each bot gets its own dealer socket
	dealer, err := ctx.NewSocket(zmq.REP)
	if err != nil {
		return nil, fmt.Errorf("[SERVER] failed to create dealer: %w", err)
	}

	// Binding the socket to a specific address (may need to adjust the address based on your needs)
	err = dealer.Bind(fmt.Sprintf("tcp://localhost:%s", b.ProcessID))
	if err != nil {
		return nil, fmt.Errorf("[SERVER] failed to bind dealer: %w", err)
	}

	// Configuration du bot
	cmd := exec.Command("./discord-bot", b.BotToken, b.ProcessID) // Passer le port unique
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Setpgid: true, // Permet de tuer le processus enfant si nécessaire
	}

	// Redirection des sorties pour le débogage
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Start(); err != nil {
		return nil, fmt.Errorf("failed to start bot: %w", err)
	}
	b.Cmd = cmd
	b.dealer = dealer

	// Here we will receive messages from the bot in a separate goroutine
	for {
		msg, err := dealer.Recv(0)
		if err != nil {
			return nil, fmt.Errorf("[SERVER] failed to receive message: %w", err)
		}
		if msg == "ready" {
			log.Printf("[SERVER] Bot is ready")
			b.read = true
			break
		}
	}
	return b, nil
}

func (b *Bot) Stop() error {
	if b.Cmd != nil && b.Cmd.Process.Pid != 0 {
		if err := syscall.Kill(-b.Cmd.Process.Pid, syscall.SIGTERM); err != nil {
			return fmt.Errorf("[SERVER] failed to stop bot: %w", err)
		}
		log.Printf("[SERVER] Bot %s stopped successfully", b.BotToken)
	}
	return nil
}

func (b *Bot) SendMessage(message string) error {
	if b.dealer == nil {
		return fmt.Errorf("[SERVER] sender socket is not initialized")
	}

	// Wait for the bot to be ready if it's not already
	if !b.read {
		log.Printf("[SERVER] Waiting for bot %s to be ready...", b.BotToken)
		// this mean we should read the recv channel
		msg, err := b.dealer.Recv(0)
		if err != nil {
			return fmt.Errorf("[SERVER] failed to receive message: %w", err)
		}
		log.Printf("[SERVER] Received message from bot %s: %s", b.BotToken, msg)
		b.read = true
	}

	// Check if the bot process is still running
	if err := b.Cmd.Process.Signal(syscall.Signal(0)); err != nil {
		return fmt.Errorf("[SERVER] bot process is not running: %w", err)
	}

	// Check if the message is empty
	if message == "" {
		return fmt.Errorf("[SERVER] message is empty")
	}

	// Send the message
	_, err := b.dealer.Send(message, 0)
	if err != nil {
		return fmt.Errorf("[SERVER] failed to send message to bot %s: %w", b.BotToken, err)
	}
	log.Printf("[SERVER] Sent message to bot %s: %s", b.BotToken, message)

	b.read = false // Reset read state after sending the message
	return nil
}
