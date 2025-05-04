package internal

import (
	"fmt"
	"log"
	"net/http"
	"os"
	"os/exec"
	"strings"
	"syscall"
)

type Bot struct {
	BotToken  string    `json:"bot_token"`
	Cmd       *exec.Cmd // Ajouter une référence à la commande
	ProcessID string
	client    *http.Client
}

func Start(b *Bot, data string, intents string) (*Bot, error) {
	// Create a new ZeroMQ socket specifically for this bot
	// Configuration du bot
	cmd := exec.Command("./discord-bot", b.BotToken, b.ProcessID, data, intents)
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

	client := http.Client{}
	// Send data to the bot
	b.client = &client
	log.Printf("[SERVER] Bot %s started successfully with PID %d", b.BotToken, cmd.Process.Pid)
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
	// Check if the bot process is still running
	if err := b.Cmd.Process.Signal(syscall.Signal(0)); err != nil {
		return fmt.Errorf("[SERVER] bot process is not running: %w", err)
	}

	// Check if the message is empty
	if message == "" {
		return fmt.Errorf("[SERVER] message is empty")
	}

	// Send the message
	resp, err := b.client.Post("http://localhost:"+b.ProcessID, "application/json", strings.NewReader(message))
	if err != nil {
		return fmt.Errorf("[SERVER] failed to send message: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("[SERVER] failed to send message: %s", resp.Status)
	}
	// Log the message sent
	log.Printf("[SERVER] Message sent to bot %s: %s", b.BotToken, message)

	return nil
}
