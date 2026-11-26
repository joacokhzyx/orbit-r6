package main

import (
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
)

func main() {
	mux := http.NewServeMux()
	mux.HandleFunc("GET /fixed", func(w http.ResponseWriter, r *http.Request) {
		reply(w, 200, "ok")
	})
	mux.HandleFunc("GET /item/{name}", func(w http.ResponseWriter, r *http.Request) {
		reply(w, 200, r.PathValue("name"))
	})
	mux.HandleFunc("POST /body", func(w http.ResponseWriter, r *http.Request) {
		body, err := io.ReadAll(http.MaxBytesReader(w, r.Body, 12000))
		if err != nil {
			reply(w, 413, "too large")
			return
		}
		if len(body) != 4096 {
			reply(w, 400, "bad body")
			return
		}
		reply(w, 200, "ok")
	})
	ln, err := net.Listen("tcp4", "127.0.0.1:" + os.Args[1])
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("http://localhost:%d\n", ln.Addr().(*net.TCPAddr).Port)
	server := &http.Server{Handler: mux, MaxHeaderBytes: 12000}
	log.Fatal(server.Serve(ln))
}

func reply(w http.ResponseWriter, status int, body string) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Content-Length", fmt.Sprint(len(body)))
	w.WriteHeader(status)
	fmt.Fprint(w, body)
}
