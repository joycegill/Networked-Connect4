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

## Example Walkthrough


### Step 1: Start the Server (Player 1)

On the first machine, start the server:
```bash
./connect4 Joyce
```

**Expected Output:**
```
Listening on port 54321
```

The server will display a Connect 4 game board in the terminal with:
- A title "🎮 Networked-Connect4" at the top
- A status line showing "Player 1's Turn" (in red)
- A 6x7 game board with a blue grid
- A cursor (▼▼▼) above the board indicating the selected column
- Rules and controls displayed on the right side

### Step 2: Connect the Client (Player 2)

On the second machine, connect to the server using the hostname/IP and port number:
```bash
./connect4 ET 123.456.7.890 54321
```

**Expected Output:**
Both players will see the same game board. Player 2's screen will show:
- The same game board layout
- Status line showing "Player 1's Turn" (since Player 1 goes first)
- The cursor can be moved with arrow keys

### Step 3: Playing the Game

**Player 1's Actions:**
1. Use left/right arrow keys to move the cursor to column 3
2. Press spacebar to place a red token
3. The token drops to the bottom of column 3
4. The status line updates to "Player 2's Turn" (in yellow)

**Player 2's Screen (automatically updates):**
- The red token appears in column 3
- The status line changes to "Player 2's Turn" (in yellow)
- Player 2 can now make a move

**Player 2's Actions:**
1. Use arrow keys to move cursor to column 4
2. Press spacebar to place a yellow token
3. The token drops to the bottom of column 4
4. The status line updates back to "Player 1's Turn"

### Step 4: Game End Scenario

When a player wins (gets 4 in a row):
- The status line shows "🏆 🎉 PLAYER 1 WINS! 🎉" (or PLAYER 2)
- The game board is frozen
- No further moves can be made
- Both players see the same win message

When the board fills with no winner:
- The status line shows "🤝 GAME OVER: It's a Draw!"
- Both players see the draw message

### Step 5: Quitting

Either player can press 'q' to quit the game. The connection closes and both programs exit.

## Dependencies

- `ncurses` - Terminal UI library for drawing the game board
- `pthread` - POSIX threads library for threading and synchronization

## Compilation

The Makefile compiles with:
- `-Wall -Wextra`: Enable all warnings
- `-std=c99`: C99 standard
- `-lncurses -pthread`: Link ncurses and pthread libraries
