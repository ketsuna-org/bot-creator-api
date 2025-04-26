package main

import (
	"net/http"
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

	panic(http.ListenAndServe(":2030", mux))
}
