# Networked-Connect 4

This is the final project for CSC 213: Operating Systems and Parallel Algorithms by Joyce Gill and Engtieng Ourn. The project implements a Connect 4 game using the `ncurses` library in C.

## Operating Systems Concepts

The following OS concepts are demonstrated in this project:
1. **Parallelism with Threads** - Two player threads run concurrently
2. **Thread Synchronization** - Mutexes coordinate access to shared game state
3. **Networking** - TCP socket-based client-server architecture for remote gameplay. 

## Architecture

### Code Organization

The project is organized into modular components:

- **`main.c`**: Main entry point, networking setup, input handling, and thread management
- **`game.h` / `game.c`**: Game logic including board state, win detection, and move validation
- **`ui.h` / `ui.c`**: User interface and display functions using ncurses
- **`socket.h`**: Network socket utilities for client-server communication

## Game Rules

- Players take turns placing tokens in columns
- Tokens drop to the lowest available row in the selected column
- First player to get 4 tokens in a row (horizontal, vertical, or diagonal) wins
- If the board fills completely with no winner, the game ends in a draw

## Controls

- **Arrow Keys (← →)**: Move cursor to select column
- **Spacebar**: Place token in selected column
- **q**: Quit the game

## How to Build and Run

To compile and run the project, use the provided `Makefile`. 

```bash
make
```

To run as a server (Player 1):
```bash
./connect4 <username>
```

To run as a client (Player 2):
```bash
./connect4 <username> <server-host> <server-port>
```

The server will print the port number it's listening on, which the client needs to connect.

## Dependencies

- `ncurses` - Terminal UI library for drawing the game board
- `pthread` - POSIX threads library for threading and synchronization

## Compilation

The Makefile compiles with:
- `-Wall -Wextra`: Enable all warnings
- `-std=c99`: C99 standard
- `-lncurses -pthread`: Link ncurses and pthread libraries
