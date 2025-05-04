package utils

import (
	"log"
	"net/http"
	"os"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

func LogMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		next.ServeHTTP(w, r)
		duration := time.Since(start)

		log.Printf("Request: %s %s took %f seconds", r.Method, r.URL.Path, duration.Seconds())
	})
}

/**
 * AuthMiddleware is a placeholder for authentication middleware.
 * In a real application, this would check for valid authentication tokens or sessions.
 * For now, it just calls the next handler.
 */
func AuthMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		authHeader := r.Header.Get("Authorization")
		if authHeader == "" {
			RespondWithError(w, http.StatusUnauthorized, "Missing authorization header")
			return
		}
		tokenString := authHeader[len("Bearer "):]
		token, err := jwt.Parse(tokenString, func(token *jwt.Token) (interface{}, error) {
			// Validate the algorithm
			if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
				return nil, http.ErrNotSupported
			}
			// Return the secret key for validation
			return []byte(os.Getenv("JWT_SECRET")), nil
		})
		if err != nil {
			RespondWithError(w, http.StatusUnauthorized, "Invalid token")
			return
		}
		if claims, ok := token.Claims.(jwt.MapClaims); ok && token.Valid {
			// You can access claims here
			log.Printf("User ID: %v", claims["user_id"])
		} else {
			RespondWithError(w, http.StatusUnauthorized, "Invalid token claims")
			return
		}

		next.ServeHTTP(w, r)
	})
}

func Chain(middlewares ...func(http.Handler) http.Handler) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		for _, middleware := range middlewares {
			next = middleware(next)
		}
		return next
	}
}
