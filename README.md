# Garnet Thieves

**Garnet Thieves** is a Linux-based multiplayer role strategy game supporting up to 8 concurrent players over TCP connections. Players choose secret and declared identities, compete for garnets, and strategically bluff to win. Designed for real-time interaction and cross-round synchronization.

## Features

- 🌐 Custom client-server protocol over TCP sockets
- 🔁 Real-time broadcasting of game state using poll-based I/O multiplexing
- 🧠 Role logic includes MAFIA, CARTEL, POLICE, and BEGGAR with custom win/loss scoring rules
- 👥 Supports 8 concurrent players with forked child process for round management
- 🧱 Fully built using raw C sockets, no external dependencies

## Technologies

- Language: C
- Networking: `socket`, `bind`, `listen`, `poll`
- Concurrency: `fork`, process management
- OS: Linux

## How to Run

1. Compile both `server.c` and `client.c` using `gcc`.
2. Run `server` first:  
   ```bash
   gcc server.c -o server && ./server
3. Run client (up to 8 terminals):
   gcc client.c -o client && ./client
