#define _XOPEN_SOURCE_EXTENDED 1 // For emoji support
#include <ncurses.h>
#include <stdlib.h>
#include <pthread.h>
#include <locale.h>
#include <wchar.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "socket.h" 

#define SLOT_HEIGHT 2
#define SLOT_WIDTH  4
#define ROWS        6
#define COLS        7

#define PLAYER_NONE 0
#define PLAYER_ONE  1
#define PLAYER_TWO  2
#define BOARD_COLOR 3

struct game_state {
    unsigned char cells[ROWS * COLS];
    unsigned char current_player;
    unsigned char winner;
    int selected_col;
    int move_made;
    int cursor_col;
    pthread_mutex_t mutex;
    pthread_cond_t turn_cond;
    int game_over;
};

static struct game_state game;

// Function to draw a single token on the board
static void draw_token(int left, int top, int col, int row, unsigned char player) {
    // Calculate the x-coordinate and y-coordinates
    int x = left + col * SLOT_WIDTH + 1;
    int y = top + row * SLOT_HEIGHT + 1;
    // Cite: https://linux.die.net/man/3/attron
    // Set the color for the player
    attron(COLOR_PAIR(player) | A_BOLD);
    // Draw the token using # characters
    for (int dx = 0; dx < SLOT_WIDTH - 1; ++dx)
        // Draw the token
        // Cite: https://linux.die.net/man/3/mvaddch 
        mvaddch(y, x + dx, '#');
    attroff(COLOR_PAIR(player) | A_BOLD);
}

// Function to draw all tokens on the board
static void draw_tokens(int left, int top, const unsigned char *cells) {
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            // Get the player at the current cell
            unsigned char player = cells[row * COLS + col];
            // Draw the token if the cell is not empty
            if (player != PLAYER_NONE) {
                draw_token(left, top, col, row, player);
            }
        }
    }
}

// Function to draw the grid for the Connect 4 board
// Cite: https://c-for-dummies.com/ncurses/tables/table04-03.php 
static void draw_grid(int left, int top) {
    // Calculate the bottom and right edges of the grid
    int bottom = top + ROWS * SLOT_HEIGHT;
    int right = left + COLS * SLOT_WIDTH;

    // Set the color for the board
    attron(COLOR_PAIR(BOARD_COLOR));
    // Draw horizontal lines
    for (int y = top; y <= bottom; y += SLOT_HEIGHT)
        for (int x = left; x <= right; ++x)
            mvaddch(y, x, ACS_HLINE);
    // Draw vertical lines
    for (int x = left; x <= right; x += SLOT_WIDTH)
        for (int y = top; y <= bottom; ++y)
            mvaddch(y, x, ACS_VLINE);
    // Draw corners
    mvaddch(top, left, ACS_ULCORNER);
    mvaddch(top, right, ACS_URCORNER);
    mvaddch(bottom, left, ACS_LLCORNER);
    mvaddch(bottom, right, ACS_LRCORNER);
    attroff(COLOR_PAIR(BOARD_COLOR));
}

/**  Find which row the token should drop to
 * @param col The column index where the token is being dropped
 * @param player The player placing the token
 * 
 * @return The row index where the token should land
 * */
int find_row(int col, const unsigned char *cells) {
    // check for the available row in the column 
    for (int row = ROWS - 1; row >= 0; row--) {
        if (cells[row * COLS + col] == PLAYER_NONE) {
            return row;
        }
    }
    return -1; // No space available 
}

// Check if a player has won by counting tokens in a direction
static int count_in_direction(const unsigned char *cells, int row, int col,
                               int row_step, int col_step, unsigned char player) {
    int count = 0;
    int r = row;
    int c = col;
    while (r >= 0 && r < ROWS && c >= 0 && c < COLS &&
           cells[r * COLS + c] == player) {
        count++;
        r += row_step;
        c += col_step;
    }
    // count the opposite direction, skip the original cell to avoid double-count
    r = row - row_step;
    c = col - col_step;
    while (r >= 0 && r < ROWS && c >= 0 && c < COLS &&
           cells[r * COLS + c] == player) {
        count++;
        r -= row_step;
        c -= col_step;
    }
    return count;
}

// Check if placing a token at (row, col) results in a win
int check_win(const unsigned char *cells, int row, int col, unsigned char player) {
    if (player == PLAYER_NONE) return 0;
    // Check horizontal (left-right)
    if (count_in_direction(cells, row, col, 0, 1, player) >= 4) return 1;
    // Check vertical (up-down)
    if (count_in_direction(cells, row, col, 1, 0, player) >= 4) return 1;
    // Check diagonal (top-left to bottom-right)
    if (count_in_direction(cells, row, col, 1, 1, player) >= 4) return 1;
    // Check diagonal (top-right to bottom-left)
    if (count_in_direction(cells, row, col, 1, -1, player) >= 4) return 1;
    return 0;
}

// Draw condition
// Check if the board is completely full 
int is_board_full(const unsigned char *cells) {
    // Check if top row (row 0) is completely filled
    for (int col = 0; col < COLS; col++) {
        if (cells[col] == PLAYER_NONE)
            return 0;
    }
    return 1;
}

static int socket_fd = -1; // connected socket for peer (server accepted or client connected)
static int server_listen_fd = -1; // listening fd if acting as server

static int send_move(int col) {
    if (socket_fd == -1) return -1;
    unsigned char b = (unsigned char)col;
    ssize_t w = write(socket_fd, &b, 1);
    if (w != 1) return -1;
    return 0;
}

void update_display(void) {
    clear();
    attron(A_BOLD);
    wchar_t game_emoji[2] = {L'🎮', L'\0'};
    mvaddwstr(0, 2, game_emoji);
    mvprintw(0, 4, "Connect 4!!");
    attroff(A_BOLD);

    pthread_mutex_lock(&game.mutex);
    unsigned char winner = game.winner;
    unsigned char current = game.current_player;
    int game_over = game.game_over;
    int cursor_col = game.cursor_col;
    unsigned char cells_copy[ROWS * COLS];
    memcpy(cells_copy, game.cells, ROWS * COLS);
    pthread_mutex_unlock(&game.mutex);

    if (game_over) {
        if (winner == PLAYER_NONE) {
            wchar_t handshake[2] = {L'🤝', L'\0'};
            mvaddwstr(2, 2, handshake);
            mvprintw(2, 4, "Game Over: Draw!");
        } else {
            wchar_t trophy[2] = {L'🏆', L'\0'};
            mvaddwstr(2, 2, trophy);
            mvprintw(2, 4, "Player %d Wins!", winner);
        }
    } else {
        mvprintw(2, 2, "Player %d's turn (Arrow keys to move, Space to place, q to quit)",
                 current);
    }

    draw_grid(4, 5);
    draw_tokens(4, 5, cells_copy);

    if (!game_over) {
        int cursor_x = 4 + cursor_col * SLOT_WIDTH + SLOT_WIDTH / 2;
        int cursor_y = 4;
        attron(COLOR_PAIR(BOARD_COLOR) | A_BOLD);
        mvprintw(cursor_y, cursor_x - 1, "^^^");
        attroff(COLOR_PAIR(BOARD_COLOR) | A_BOLD);
    }

    int rules_x = COLS * SLOT_WIDTH + 10;
    attron(A_BOLD);
    wchar_t rules_emoji[2] = {L'📋', L'\0'};
    mvaddwstr(5, rules_x, rules_emoji);
    mvprintw(5, rules_x + 2, "RULES:");
    attroff(A_BOLD);

    mvprintw(6, rules_x, "1. Players take turns");
    mvprintw(7, rules_x + 2, "placing tokens in columns");
    mvprintw(8, rules_x, "2. Tokens drop to the");
    mvprintw(9, rules_x + 2, "lowest empty row");
    mvprintw(10, rules_x, "3. First to get 4 in a");
    wchar_t trophy2[2] = {L'🏆', L'\0'};
    mvaddwstr(11, rules_x + 2, trophy2);
    mvprintw(11, rules_x + 4, "row wins!");
    mvprintw(12, rules_x + 2, "(horizontal, vertical,");
    mvprintw(13, rules_x + 2, "or diagonal)");

    wchar_t keyboard[2] = {L'⌨', L'\0'};
    mvaddwstr(ROWS * SLOT_HEIGHT + 7, 2, keyboard);
    mvprintw(ROWS * SLOT_HEIGHT + 7, 4, "Controls: Arrow keys = Move cursor, Space = Place token, q = Quit");

    refresh();
}

// When a move arrives it is applied to the board and turn switches.
void* recv_thread(void *arg) {
    (void)arg;
    unsigned char buf;
    while (1) {
        ssize_t r = read(socket_fd, &buf, 1);
        if (r <= 0) break; // disconnected or error
        int col = (int)buf;
        if (col < 0 || col >= COLS) continue;

        pthread_mutex_lock(&game.mutex);
        if (game.game_over) {
            pthread_mutex_unlock(&game.mutex);
            break;
        }
        int row = find_row(col, game.cells);
        if (row != -1) {
            // peer is the other player
            unsigned char peer_player = (game.current_player == PLAYER_ONE) ? PLAYER_TWO : PLAYER_ONE;
            game.cells[row * COLS + col] = peer_player;

            if (check_win(game.cells, row, col, peer_player)) {
                game.winner = peer_player;
                game.game_over = 1;
            } else if (is_board_full(game.cells)) {
                game.winner = PLAYER_NONE;
                game.game_over = 1;
            } else {
                // switch turn back to local player
                game.current_player = (peer_player == PLAYER_ONE) ? PLAYER_TWO : PLAYER_ONE;
            }
        }
        pthread_mutex_unlock(&game.mutex);
        update_display();
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2 && argc != 4) {
        fprintf(stderr, "Usage: %s <username> [<peer-host> <peer-port>]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Networking setup
    int is_server = (argc == 2);
    unsigned short port = 0;
    socket_fd = -1;
    server_listen_fd = -1;

    if (is_server) {
        // open server socket and listen, then accept one connection
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
        // print port so peer can connect (optional)
        fprintf(stderr, "Listening on port %u\n", port);

        // accept a single connection (blocking)
        int peer_fd = accept(server_listen_fd, NULL, NULL);
        if (peer_fd < 0) {
            perror("accept");
            close(server_listen_fd);
            return EXIT_FAILURE;
        }
        socket_fd = peer_fd;
    } else {
        // client: connect to peer
        const char *peer_host = argv[2];
        unsigned short peer_port = (unsigned short)atoi(argv[3]);
        int fd = socket_connect(peer_host, peer_port);
        if (fd < 0) {
            perror("socket_connect");
            return EXIT_FAILURE;
        }
        socket_fd = fd;
    }

    // Initialize ncurses and locale
    setlocale(LC_ALL, "");
    if (initscr() == NULL) {
        fprintf(stderr, "initscr failed\n");
        close(socket_fd);
        return EXIT_FAILURE;
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    init_pair(PLAYER_ONE, COLOR_RED, COLOR_BLACK);
    init_pair(PLAYER_TWO, COLOR_YELLOW, COLOR_BLACK);
    init_pair(BOARD_COLOR, COLOR_BLUE, COLOR_BLACK);

    // Initialize game state
    memset(game.cells, PLAYER_NONE, sizeof(game.cells));
    pthread_mutex_init(&game.mutex, NULL);
    game.current_player = is_server ? PLAYER_ONE : PLAYER_TWO;
    game.winner = PLAYER_NONE;
    game.cursor_col = COLS / 2;
    game.game_over = 0;

    // start recv thread
    pthread_t rt;
    if (pthread_create(&rt, NULL, recv_thread, NULL) != 0) {
        endwin();
        perror("pthread_create");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    update_display();

    // main input loop: local player places moves when it's their turn
    int ch;
    while ((ch = getch()) != 'q' && ch != 'Q') {
        pthread_mutex_lock(&game.mutex);
        if (game.game_over) {
            pthread_mutex_unlock(&game.mutex);
            continue;
        }

        // navigation
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
            unsigned char my_player = is_server ? PLAYER_ONE : PLAYER_TWO;
            if (game.current_player != my_player) {
                // not our turn
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
                // handle send error: mark game over and break
                game.game_over = 1;
                game.winner = PLAYER_NONE;
                pthread_mutex_unlock(&game.mutex);
                break;
            }

            // check win
            if (check_win(game.cells, row, col, my_player)) {
                game.winner = my_player;
                game.game_over = 1;
            } else if (is_board_full(game.cells)) {
                game.winner = PLAYER_NONE;
                game.game_over = 1;
            } else {
                // switch turn to peer
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

    // close socket to cause recv thread to exit
    if (socket_fd != -1) close(socket_fd);
    if (server_listen_fd != -1) close(server_listen_fd);

    // join thread
    pthread_join(rt, NULL);

    // cleanup
    pthread_mutex_destroy(&game.mutex);
    endwin();
    return EXIT_SUCCESS;
}
