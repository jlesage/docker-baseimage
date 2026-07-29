package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"strings"

	"github.com/tredoe/crypt/sha512_crypt"
)

func usage() {
	fmt.Fprintf(os.Stderr, "Usage: %s [password]\n", os.Args[0])
	fmt.Fprintf(os.Stderr, "\n")
	fmt.Fprintf(os.Stderr, "Generate a SHA-512 crypt password hash.\n")
	fmt.Fprintf(os.Stderr, "If password is omitted, it is read from stdin.\n")
}

func readPassword() (string, error) {
	if len(os.Args) >= 2 {
		if os.Args[1] == "-h" || os.Args[1] == "--help" {
			usage()
			os.Exit(0)
		}
		return os.Args[1], nil
	}

	// Read password from stdin (first line only).
	reader := bufio.NewReader(os.Stdin)
	line, err := reader.ReadString('\n')
	if err != nil && err != io.EOF {
		return "", err
	}
	password := strings.TrimRight(line, "\r\n")
	if password == "" && err == io.EOF {
		return "", fmt.Errorf("no password provided on stdin")
	}
	return password, nil
}

func main() {
	password, err := readPassword()
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
	if password == "" {
		usage()
		os.Exit(1)
	}

	c := sha512_crypt.New()
	hash, err := c.Generate([]byte(password), nil) // nil = random salt
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
	fmt.Println(string(hash))
}
