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
	processID int
	dealer    *zmq.Socket // Stocker le PGID (Process Group ID)
	read      bool
}

func Start(b *Bot, dealer *zmq.Socket) (*Bot, error) {

	// Configuration du bot
	cmd := exec.Command("../bot/build/discord-bot", b.BotToken)
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Setpgid: true, // Permet de kill le processus enfant si nécessaire
	}

	// Redirection des sorties pour le débogage
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Start(); err != nil {
		return nil, fmt.Errorf("failed to start bot: %w", err)
	}
	b.Cmd = cmd
	b.processID = cmd.Process.Pid
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
	if b.Cmd != nil && b.processID != 0 {
		if err := syscall.Kill(-b.processID, syscall.SIGTERM); err != nil {
			return fmt.Errorf("[SERVER] failed to stop bot: %w", err)
		}
	}
	return nil
}

func (b *Bot) SendMessage(message string) error {
	if b.dealer == nil {
		return fmt.Errorf("[SERVER] sender socket is not initialized")
	}
	if !b.read {
		// Let's read the message before sending
		msg, err := b.dealer.Recv(0)
		if err != nil {
			return fmt.Errorf("[SERVER] failed to receive message: %w", err)
		}
		log.Printf("[SERVER] received message: %s", msg)
		b.read = true // Fix ici !
	}

	_, err := b.dealer.Send(message, 0)
	if err != nil {
		return fmt.Errorf("[SERVER] failed to send message: %w", err)
	}
	log.Printf("[SERVER] sent message: %s", message)

	b.read = false
	return nil
}
