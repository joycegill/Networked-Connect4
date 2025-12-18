#define _XOPEN_SOURCE_EXTENDED 1 // For emoji support
#include <ncurses.h>
#include <stdlib.h>
#include <pthread.h>
#include <locale.h>
#include <wchar.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "socket.h"
#include "game.h"
#include "ui.h"

#define BOARD_COLOR 3

struct game_state game;
unsigned char my_player; 

/** Helper function to write all the required bytes
 * @param fd The file descriptor to write to
 * @param buf The buffer containing data to write
 * @param len The number of bytes to write
 * 
 * @return The number of bytes written, or -1 on error
 */
ssize_t write_helper(int fd, const void* buf, size_t len) {
  size_t bytes_written = 0;

  // Write every element in the buffer
  while (bytes_written < len) {
    ssize_t rc = write(fd, (const char*)buf + bytes_written, len - bytes_written);
    if (rc < 0) return rc;
    bytes_written += (size_t) rc;
  }
  return (ssize_t) bytes_written;
}

/** Helper function to read all the required bytes
 * @param fd The file descriptor to read from
 * @param buf The buffer to store read data
 * @param len The number of bytes to read
 * 
 * @return The number of bytes read, or -1 on error
 */
size_t read_helper(int fd, void* buf, size_t len) {
  // Bytes read so far
  size_t bytes_read = 0; 
  
  // Keep reading until the end
  while (bytes_read < len) {
    // Try to read the entire remaining message
    ssize_t rc2 = read(fd, buf + bytes_read, len - bytes_read);
    // Catch error
    if (rc2 < 0) return rc2;
    // Update bytes read so far
    bytes_read += rc2;
  }
  
  // All bytes are read
  return bytes_read;
}

// Networking
static int socket_fd = -1; // Connected socket for peer 
static int server_listen_fd = -1; // Listening fd if acting as server

/** Send a move to the peer over the network
 * Sends a 2-byte message: [sender_id][col]
 * @param col The column index where the token is being placed
 * 
 * @return 0 on success, -1 on error
 */
static int send_move(int col) {
    if (socket_fd == -1) return -1;
    unsigned char buf[2];
    buf[0] = my_player;
    buf[1] = (unsigned char)col;
    ssize_t w = write_helper(socket_fd, buf, 2);
    if (w != 2) return -1;
    return 0;
}

/** Thread function to receive moves from the network peer
 * When a move arrives it is applied to the board and turn switches
 * @param arg Thread argument (unused)
 * 
 * @return NULL
 */
void* recv_thread(void *arg) {
    (void)arg;
    unsigned char buf[2];

    // Loop to reading the opponent's chosen column
    while (1) {
        // Read the column number
        ssize_t r = read_helper(socket_fd, buf, 2);
        if (r == 0) break; // Peer closed
        if (r < 0) break;  // Error
        unsigned char sender = buf[0];
        int col = (int)buf[1];
        if (col < 0 || col >= COLS) continue;

        pthread_mutex_lock(&game.mutex);
        // Unlocks and exits if the game end
        if (game.game_over) {
            pthread_mutex_unlock(&game.mutex);
            break;
        }

        // Token position
        int row = find_row(col, game.cells);
        if (row != -1) { // Column is not full
            // Determine player identity
            unsigned char peer_player = (sender == PLAYER_ONE) ? PLAYER_ONE : PLAYER_TWO;
            // If peer_player != my_player, process the move
            if (peer_player != my_player) {
                game.cells[row * COLS + col] = peer_player;
                
                // Check for win or draw
                if (check_win(game.cells, row, col, peer_player)) {
                    game.winner = peer_player;
                    game.game_over = 1;
                } else if (is_board_full(game.cells)) {
                    game.winner = PLAYER_NONE;
                    game.game_over = 1;
                } else {
                    // After peer moved, now it's the local player's turn
                    game.current_player = my_player;
                }
            }
        }
        // Unlock and update the screen
        pthread_mutex_unlock(&game.mutex);
        update_display();
    }
    return NULL;
}

/** Main entry point for the Connect 4 game
 * @param argc Number of command line arguments
 * @param argv Command line arguments (username for server, or username host port for client)
 * 
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc, char **argv) {
    // Argument parsing
    if (argc != 2 && argc != 4) {
        fprintf(stderr, "Usage:\n  Server: %s <username>\n  Client: %s <username> <server-host> <server-port>\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    // Server mode
    int is_server = (argc == 2);
    unsigned short port = 0;

    // Networking setup
    if (is_server) {
        // Open server socket and listen, then accept one connection
        server_listen_fd = server_socket_open(&port);
        if (server_listen_fd < 0) {
            perror("server_socket_open");
            return EXIT_FAILURE;
        }
        if (listen(server_listen_fd, 1) == -1) {
            perror("listen");
            close(server_listen_fd);
            return EXIT_FAILURE;
        }
        // Print port so peer can connect
        fprintf(stderr, "Listening on port %u\n", port);

        // Accept one connection
        int peer_fd = accept(server_listen_fd, NULL, NULL);
        if (peer_fd < 0) {
            perror("accept");
            close(server_listen_fd);
            return EXIT_FAILURE;
        }
        socket_fd = peer_fd;
    } else {
        // Client: connect to peer
        char *peer_host = argv[2];
        unsigned short peer_port = (unsigned short)atoi(argv[3]);
        int fd = socket_connect(peer_host, peer_port);
        if (fd < 0) {
            perror("socket_connect");
            return EXIT_FAILURE;
        }
        socket_fd = fd;
    }

    // Init UI 
    setlocale(LC_ALL, "");
    if (initscr() == NULL) {
        fprintf(stderr, "initscr failed\n");
        close(socket_fd);
        return EXIT_FAILURE;
    }
    cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    start_color();
    init_pair(PLAYER_ONE, COLOR_RED, COLOR_BLACK);
    init_pair(PLAYER_TWO, COLOR_YELLOW, COLOR_BLACK);
    init_pair(BOARD_COLOR, COLOR_BLUE, COLOR_BLACK);
    init_pair(4, COLOR_WHITE, COLOR_BLACK); // White color for rules

    // Initialize game 
    memset(game.cells, PLAYER_NONE, sizeof(game.cells));
    pthread_mutex_init(&game.mutex, NULL);

    my_player = is_server ? PLAYER_ONE : PLAYER_TWO;
    // Both sides show Player 1 starts 
    game.current_player = PLAYER_ONE;
    game.winner = PLAYER_NONE;
    game.cursor_col = COLS / 2;
    game.game_over = 0;

    // Start recv thread 
    pthread_t rt;
    if (pthread_create(&rt, NULL, recv_thread, NULL) != 0) {
        endwin();
        perror("pthread_create");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    // Initial draw 
    update_display();

    // Main input loop 
    int ch;
    while ((ch = getch()) != 'q' && ch != 'Q') {
        pthread_mutex_lock(&game.mutex);
        if (game.game_over) {
            pthread_mutex_unlock(&game.mutex);
            continue;
        }

        if (ch == KEY_LEFT) {
            if (game.cursor_col > 0) game.cursor_col--;
            pthread_mutex_unlock(&game.mutex);
            update_display();
            continue;
        }
        if (ch == KEY_RIGHT) {
            if (game.cursor_col < COLS - 1) game.cursor_col++;
            pthread_mutex_unlock(&game.mutex);
            update_display();
            continue;
        }

        if (ch == ' ') {
            // Only allow placing if it's this process's player turn 
            if (game.current_player != my_player) {
                pthread_mutex_unlock(&game.mutex);
                continue;
            }
            int col = game.cursor_col;
            int row = find_row(col, game.cells);
            if (row == -1) {
                pthread_mutex_unlock(&game.mutex);
                continue;
            }

            // place locally 
            game.cells[row * COLS + col] = my_player;

            // send to peer 
            if (send_move(col) != 0) {
                // mark game over on error 
                game.game_over = 1;
                game.winner = PLAYER_NONE;
                pthread_mutex_unlock(&game.mutex);
                break;
            }

            // check win/draw 
            if (check_win(game.cells, row, col, my_player)) {
                game.winner = my_player;
                game.game_over = 1;
            } else if (is_board_full(game.cells)) {
                game.winner = PLAYER_NONE;
                game.game_over = 1;
            } else {
                // after our move, it's peer's turn 
                game.current_player = (my_player == PLAYER_ONE) ? PLAYER_TWO : PLAYER_ONE;
            }
        }
        pthread_mutex_unlock(&game.mutex);
        update_display();
    }

    // Quit sequence
    pthread_mutex_lock(&game.mutex);
    game.game_over = 1;
    pthread_mutex_unlock(&game.mutex);

    if (socket_fd != -1) close(socket_fd);
    if (server_listen_fd != -1) close(server_listen_fd);

    pthread_join(rt, NULL);

    pthread_mutex_destroy(&game.mutex);
    endwin();

    return EXIT_SUCCESS;
}
