package internal

import (
	"fmt"
	"net"
	"os"
	"os/exec"
	"syscall"
	"time"
)

type Bot struct {
	BotID     string    `json:"bot_id"`
	BotToken  string    `json:"bot_token"`
	Cmd       *exec.Cmd // Ajouter une référence à la commande
	processID int       // Stocker le PGID (Process Group ID)
}

func Start(b *Bot) (net.Conn, error) {
	socketPath := fmt.Sprintf("/tmp/%s.sock", b.BotID)

	// Nettoyage préalable du socket
	if err := os.RemoveAll(socketPath); err != nil && !os.IsNotExist(err) {
		return nil, fmt.Errorf("error cleaning socket: %w", err)
	}

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

	// Mécanisme d'attente intelligente pour le socket
	var conn net.Conn
	maxRetries := 10
	for i := 0; i < maxRetries; i++ {
		var err error
		conn, err = net.Dial("unix", socketPath)
		if err == nil {
			break
		}
		time.Sleep(500 * time.Millisecond)
	}

	if conn == nil {
		return nil, fmt.Errorf("failed to connect to bot socket after %d attempts", maxRetries)
	}

	fmt.Printf("Bot %s started successfully\n", b.BotID)
	return conn, nil
}
