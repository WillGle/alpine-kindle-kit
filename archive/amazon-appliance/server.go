package main

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	_ "embed"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"sync"
	"syscall"
	"time"
	"unicode"
	"unicode/utf8"
)

//go:embed web/index.html
var page []byte

//go:embed web/app.js
var script []byte

type library struct {
	root *os.Root
	mu   sync.Mutex
}

type server struct {
	token   [32]byte
	roots   map[string]*library
	limit   int64
	reserve uint64
	upload  chan struct{}
}

type entry struct {
	Name string `json:"name"`
	Size int64  `json:"size"`
}

func main() {
	addr := flag.String("addr", "127.0.0.1:8080", "loopback listen address")
	books := flag.String("books", "/mnt/us/documents/GleWorks", "book directory (must exist)")
	files := flag.String("files", "/mnt/us/gleworks/files", "file directory (must exist)")
	tokenFile := flag.String("token-file", "/var/local/gleworks/http.token", "private token file")
	initToken := flag.Bool("init-token", false, "create a random token file without replacing an existing one")
	maxMiB := flag.Int64("max-upload-mib", 128, "maximum bytes per upload, in MiB (1–1024)")
	flag.Parse()
	if flag.NArg() != 0 {
		log.Fatal("unexpected positional arguments")
	}
	if *initToken {
		if err := createToken(*tokenFile); err != nil {
			log.Fatal(err)
		}
		fmt.Println("Token file created; transfer it privately to your client.")
		return
	}
	if *maxMiB < 1 || *maxMiB > 1024 {
		log.Fatal("max-upload-mib must be between 1 and 1024")
	}
	host, _, err := net.SplitHostPort(*addr)
	if err != nil || net.ParseIP(host) == nil || !net.ParseIP(host).IsLoopback() {
		log.Fatal("addr must use a numeric loopback IP; use Tailscale or a private reverse proxy")
	}
	key, err := os.ReadFile(*tokenFile)
	if err != nil {
		log.Fatal("cannot read token file: ", err)
	}
	keyText := strings.TrimSpace(string(key))
	decoded, err := hex.DecodeString(keyText)
	if err != nil || len(decoded) != 32 {
		log.Fatal("token file must contain a 64-character hexadecimal token")
	}
	s := &server{
		token: sha256.Sum256([]byte(keyText)),
		roots: make(map[string]*library),
		limit: *maxMiB << 20, reserve: 64 << 20,
		upload: make(chan struct{}, 1),
	}
	for name, dir := range map[string]string{"books": *books, "files": *files} {
		root, err := os.OpenRoot(dir)
		if err != nil {
			log.Fatalf("cannot open %s root: %v", name, err)
		}
		defer root.Close()
		s.roots[name] = &library{root: root}
	}
	httpServer := &http.Server{
		Addr: *addr, Handler: s,
		ReadHeaderTimeout: 5 * time.Second, ReadTimeout: 5 * time.Minute,
		WriteTimeout: 5 * time.Minute, IdleTimeout: 30 * time.Second,
		MaxHeaderBytes: 8 << 10,
	}
	listener, err := net.Listen("tcp", *addr)
	if err != nil {
		log.Fatal(err)
	}
	quit := make(chan os.Signal, 1)
	done := make(chan struct{})
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-quit
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Minute)
		defer cancel()
		if err := httpServer.Shutdown(ctx); err != nil {
			_ = httpServer.Close()
		}
		close(done)
	}()
	log.Printf("GleWorks Kindle listening on %s; no automatic updates or outbound requests", *addr)
	if err := httpServer.Serve(listener); err != nil && !errors.Is(err, http.ErrServerClosed) {
		log.Fatal(err)
	}
	<-done
}

func createToken(path string) error {
	b := make([]byte, 32)
	if _, err := rand.Read(b); err != nil {
		return err
	}
	f, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0600)
	if err != nil {
		return err
	}
	_, writeErr := fmt.Fprintln(f, hex.EncodeToString(b))
	closeErr := f.Close()
	if err := errors.Join(writeErr, closeErr); err != nil {
		_ = os.Remove(path)
		return err
	}
	return nil
}

func (s *server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Cache-Control", "no-store")
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.Header().Set("Referrer-Policy", "no-referrer")
	w.Header().Set("X-Frame-Options", "DENY")
	w.Header().Set("Content-Security-Policy", "default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'")
	if r.URL.Path == "/" && r.Method == http.MethodGet {
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		_, _ = w.Write(page)
		return
	}
	if r.URL.Path == "/app.js" && r.Method == http.MethodGet {
		w.Header().Set("Content-Type", "text/javascript; charset=utf-8")
		_, _ = w.Write(script)
		return
	}
	// Reject cross-origin browser API requests; never rely on cookies for auth.
	if r.Header.Get("Origin") != "" || r.Header.Get("Sec-Fetch-Site") == "cross-site" {
		// Same-origin browser PUTs carry Origin, so match it exactly.
		origin := r.Header.Get("Origin")
		if r.Header.Get("Sec-Fetch-Site") == "cross-site" || (origin != "http://"+r.Host && origin != "https://"+r.Host) {
			http.Error(w, "cross-origin request rejected", http.StatusForbidden)
			return
		}
	}
	provided := strings.TrimPrefix(r.Header.Get("Authorization"), "Bearer ")
	digest := sha256.Sum256([]byte(provided))
	if !strings.HasPrefix(r.Header.Get("Authorization"), "Bearer ") || subtle.ConstantTimeCompare(s.token[:], digest[:]) != 1 {
		http.Error(w, "access token required", http.StatusUnauthorized)
		return
	}
	parts := strings.Split(strings.TrimPrefix(r.URL.Path, "/api/"), "/")
	if !strings.HasPrefix(r.URL.Path, "/api/") || len(parts) < 1 || len(parts) > 2 {
		http.NotFound(w, r)
		return
	}
	lib := s.roots[parts[0]]
	if lib == nil {
		http.NotFound(w, r)
		return
	}
	if len(parts) == 1 && r.Method == http.MethodGet {
		s.list(w, lib)
		return
	}
	if len(parts) != 2 || !validName(parts[1]) {
		http.Error(w, "invalid file name", http.StatusBadRequest)
		return
	}
	switch r.Method {
	case http.MethodGet:
		s.download(w, r, lib, parts[1])
	case http.MethodPut:
		s.uploadFile(w, r, lib, parts[1])
	default:
		w.Header().Set("Allow", "GET, PUT")
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
	}
}

func validName(name string) bool {
	if name == "" || !utf8.ValidString(name) || len(name) > 180 || strings.HasPrefix(name, ".") || strings.TrimSpace(name) != name || strings.HasSuffix(name, ".") {
		return false
	}
	for _, c := range name {
		if unicode.IsControl(c) || strings.ContainsRune("/\\:*?\"<>|", c) {
			return false
		}
	}
	return filepath.Base(name) == name
}

func (s *server) list(w http.ResponseWriter, lib *library) {
	lib.mu.Lock()
	defer lib.mu.Unlock()
	dir, err := lib.root.Open(".")
	if err != nil {
		http.Error(w, "directory unavailable", http.StatusInternalServerError)
		return
	}
	defer dir.Close()
	items := make([]entry, 0)
	for {
		entries, readErr := dir.ReadDir(128)
		for _, e := range entries {
			if !validName(e.Name()) || !e.Type().IsRegular() {
				continue
			}
			info, err := e.Info()
			if err == nil && info.Mode().IsRegular() {
				items = append(items, entry{Name: e.Name(), Size: info.Size()})
			}
		}
		if errors.Is(readErr, io.EOF) {
			break
		}
		if readErr != nil {
			http.Error(w, "cannot list directory", http.StatusInternalServerError)
			return
		}
		if len(items) > 10000 {
			http.Error(w, "directory has too many files", http.StatusServiceUnavailable)
			return
		}
	}
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(items)
}

func (s *server) download(w http.ResponseWriter, r *http.Request, lib *library, name string) {
	lib.mu.Lock()
	// O_NOFOLLOW and O_NONBLOCK also reject symlinks and avoid blocking on FIFOs.
	f, err := lib.root.OpenFile(name, os.O_RDONLY|syscall.O_NOFOLLOW|syscall.O_NONBLOCK, 0)
	lib.mu.Unlock()
	if err != nil {
		http.NotFound(w, r)
		return
	}
	defer f.Close()
	info, err := f.Stat()
	if err != nil || !info.Mode().IsRegular() {
		http.NotFound(w, r)
		return
	}
	w.Header().Set("Content-Type", "application/octet-stream")
	w.Header().Set("Content-Disposition", "attachment")
	http.ServeContent(w, r, name, info.ModTime(), f)
}

func (s *server) uploadFile(w http.ResponseWriter, r *http.Request, lib *library, name string) {
	if r.ContentLength < 0 {
		http.Error(w, "Content-Length required", http.StatusLengthRequired)
		return
	}
	if r.ContentLength > s.limit {
		http.Error(w, "upload exceeds configured limit", http.StatusRequestEntityTooLarge)
		return
	}
	select {
	case s.upload <- struct{}{}:
		defer func() { <-s.upload }()
	default:
		http.Error(w, "another upload is in progress", http.StatusServiceUnavailable)
		return
	}
	lib.mu.Lock()
	defer lib.mu.Unlock()
	dir, err := lib.root.Open(".")
	if err != nil {
		http.Error(w, "directory unavailable", http.StatusInternalServerError)
		return
	}
	var disk syscall.Statfs_t
	err = syscall.Fstatfs(int(dir.Fd()), &disk)
	_ = dir.Close()
	if err != nil || uint64(disk.Bavail)*uint64(disk.Bsize) < uint64(r.ContentLength)+s.reserve {
		http.Error(w, "insufficient free space (64 MiB reserve)", http.StatusInsufficientStorage)
		return
	}
	// Exclusive creation never overwrites books, including case aliases on FAT.
	f, err := lib.root.OpenFile(name, os.O_WRONLY|os.O_CREATE|os.O_EXCL|syscall.O_NOFOLLOW, 0600)
	if errors.Is(err, os.ErrExist) {
		http.Error(w, "file already exists; choose another name", http.StatusConflict)
		return
	}
	if err != nil {
		http.Error(w, "cannot create file", http.StatusInternalServerError)
		return
	}
	complete := false
	defer func() {
		_ = f.Close()
		if !complete {
			_ = lib.root.Remove(name)
		}
	}()
	r.Body = http.MaxBytesReader(w, r.Body, s.limit)
	n, err := io.Copy(f, r.Body)
	if err != nil || n != r.ContentLength {
		http.Error(w, "upload incomplete", http.StatusBadRequest)
		return
	}
	if err := f.Sync(); err != nil {
		http.Error(w, "cannot sync file", http.StatusInsufficientStorage)
		return
	}
	if err := f.Close(); err != nil {
		http.Error(w, "cannot close file", http.StatusInternalServerError)
		return
	}
	complete = true
	w.WriteHeader(http.StatusCreated)
}
