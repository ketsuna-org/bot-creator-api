package utils

import (
	"encoding/json"
	"net/http"
)

func RespondWithError(w http.ResponseWriter, code int, message string) {
	RespondWithJSON(w, map[string]string{"error": message}, code)
}

func RespondWithJSON(w http.ResponseWriter, payload interface{}, code ...int) {
	var c int = http.StatusOK
	if len(code) > 0 {
		c = code[0]
	}
	response, _ := json.Marshal(payload)

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(c)
	w.Write(response)
}
