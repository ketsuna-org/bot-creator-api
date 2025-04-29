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
	readyChan chan bool // Canal pour indiquer que le bot est prêt
}

func Start(b *Bot, dealer *zmq.Socket) (*Bot, error) {

	// Configuration du bot
	cmd := exec.Command("./discord-bot", b.BotToken)
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
	b.readyChan = make(chan bool)

	// Goroutine pour écouter les messages du bot
	go func() {
		// Boucle de réception des messages du bot via ZeroMQ
		for {
			msg, err := dealer.Recv(0)
			if err != nil {
				log.Printf("[SERVER] Failed to receive message from bot %s: %v", b.BotToken, err)
				continue // Continue à recevoir les messages même si une erreur se produit
			}

			if msg == "ready" {
				log.Printf("[SERVER] Bot %s is ready", b.BotToken)
				b.read = true
				b.readyChan <- true // Indiquer que le bot est prêt
				break
			}
		}
	}()

	return b, nil
}

func (b *Bot) Stop() error {
	if b.Cmd != nil && b.processID != 0 {
		if err := syscall.Kill(-b.processID, syscall.SIGTERM); err != nil {
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

	// Attendre que le bot soit prêt si ce n'est pas déjà fait
	if !b.read {
		log.Printf("[SERVER] Waiting for bot %s to be ready...", b.BotToken)
		<-b.readyChan // Attendre que le bot soit prêt
	}

	// Envoi du message
	_, err := b.dealer.Send(message, 0)
	if err != nil {
		return fmt.Errorf("[SERVER] failed to send message to bot %s: %w", b.BotToken, err)
	}
	log.Printf("[SERVER] Sent message to bot %s: %s", b.BotToken, message)

	b.read = false // Réinitialiser l'état de lecture après l'envoi
	return nil
}
