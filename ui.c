#define _XOPEN_SOURCE_EXTENDED 1 // For emoji support
#include <ncurses.h>
#include <locale.h>
#include <wchar.h>
#include <string.h>
#include <pthread.h>

#include "ui.h"
#include "game.h"

// External game state (defined in main.c)
extern struct game_state game;
extern unsigned char my_player;

/** Function to draw a single token on the board
 * @param left The left x-coordinate of the board
 * @param top The top y-coordinate of the board
 * @param col The column index of the token
 * @param row The row index of the token
 * @param player The player number (PLAYER_ONE or PLAYER_TWO)
 */
static void draw_token(int left, int top, int col, int row, unsigned char player) {
    // Calculate the x-coordinate and y-coordinate
    int x = left + col * SLOT_WIDTH + 1;
    int y = top + row * SLOT_HEIGHT + 1;
    // Cite: https://linux.die.net/man/3/attron
    // Set the color for the player
    attron(COLOR_PAIR(player) | A_BOLD);
    // Draw the token as a filled circle using block characters
    mvaddch(y, x, ACS_BLOCK);
    mvaddch(y, x + 1, ACS_BLOCK);
    mvaddch(y, x + 2, ACS_BLOCK);
    attroff(COLOR_PAIR(player) | A_BOLD);
}

/** Function to draw all tokens on the board
 * @param left The left x-coordinate of the board
 * @param top The top y-coordinate of the board
 * @param cells The game board cells array
 */
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

/** Function to draw the grid for the Connect 4 board
 * Cite: https://c-for-dummies.com/ncurses/tables/table04-03.php 
 * @param left The left x-coordinate of the grid
 * @param top The top y-coordinate of the grid
 */
static void draw_grid(int left, int top) {
    // Calculate the bottom and right edges of the grid
    int bottom = top + ROWS * SLOT_HEIGHT;
    int right = left + COLS * SLOT_WIDTH;
    // Set the color for the board
    attron(COLOR_PAIR(BOARD_COLOR));
    
    // Draw horizontal lines
    for (int y = top; y <= bottom; y += SLOT_HEIGHT) {
        for (int x = left; x <= right; ++x) {
            if (x == left) {
                if (y == top) mvaddch(y, x, ACS_ULCORNER);
                else if (y == bottom) mvaddch(y, x, ACS_LLCORNER);
                else mvaddch(y, x, ACS_VLINE);
            } else if (x == right) {
                if (y == top) mvaddch(y, x, ACS_URCORNER);
                else if (y == bottom) mvaddch(y, x, ACS_LRCORNER);
                else mvaddch(y, x, ACS_VLINE);
            } else {
                mvaddch(y, x, ACS_HLINE);
            }
        }
    }
    
    // Draw vertical lines
    for (int x = left; x <= right; x += SLOT_WIDTH) {
        for (int y = top; y <= bottom; ++y) {
            if (y != top && y != bottom) {
                mvaddch(y, x, ACS_VLINE);
            }
        }
    }
    attroff(COLOR_PAIR(BOARD_COLOR));
}

/** Function to redraw the entire screen
 * Updates the display with current game state, including board, tokens, rules, and controls
 */
void update_display(void) {
    // Clear the screen and print the title
    clear();
    
    // Print the title with better formatting
    attron(A_BOLD | COLOR_PAIR(4));
    wchar_t game_emoji[2] = {L'🎮', L'\0'};
    mvaddwstr(1, 2, game_emoji);
    mvprintw(1, 4, "Networked-Connect4");
    attroff(A_BOLD | COLOR_PAIR(4));

    // Lock the game state and copy values
    pthread_mutex_lock(&game.mutex);
    unsigned char winner = game.winner;
    unsigned char current = game.current_player;
    int game_over = game.game_over;
    int cursor_col = game.cursor_col;
    unsigned char cells_copy[ROWS * COLS];
    memcpy(cells_copy, game.cells, ROWS * COLS);
    pthread_mutex_unlock(&game.mutex);

    // Game_over screen
    attron(A_BOLD);
    if (game_over) {
        if (winner == PLAYER_NONE) {
            wchar_t handshake[2] = {L'🤝', L'\0'};
            mvaddwstr(3, 2, handshake);
            attron(COLOR_PAIR(BOARD_COLOR));
            mvprintw(4, 4, "   GAME OVER: It's a Draw!  ");
            attroff(COLOR_PAIR(BOARD_COLOR));
        } else {
            wchar_t trophy[2] = {L'🏆', L'\0'};
            mvaddwstr(3, 2, trophy);
            attron(COLOR_PAIR(winner));
            mvprintw(4, 4, "   🎉 PLAYER %d WINS! 🎉  ", winner);
            attroff(COLOR_PAIR(winner));
        }
    } else {
        // Game not yet over
        attron(COLOR_PAIR(current));
        mvprintw(3, 2, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        mvprintw(4, 2, "  Player %d's Turn  ", current);
        attroff(COLOR_PAIR(current));
    }
    attroff(A_BOLD);

    // Draw grid and tokens
    int board_top = 8;
    int board_left = 4;
    draw_grid(board_left, board_top);
    draw_tokens(board_left, board_top, cells_copy);

    // Draw the cursor 
    if (!game_over) {
        int cursor_x = board_left + cursor_col * SLOT_WIDTH + SLOT_WIDTH / 2;
        int cursor_y = board_top - 1;
        attron(COLOR_PAIR(BOARD_COLOR) | A_BOLD);
        mvprintw(cursor_y, cursor_x - 1, "▼▼▼");
        attroff(COLOR_PAIR(BOARD_COLOR) | A_BOLD);
    }
    
    // Draw the rules box with a border
    int rules_x = COLS * SLOT_WIDTH + 12;
    int rules_y = board_top;
    
    // Draw box border in white
    attron(COLOR_PAIR(4));
    mvaddch(rules_y, rules_x - 1, ACS_ULCORNER);
    mvaddch(rules_y, rules_x + 30, ACS_URCORNER);
    mvaddch(rules_y + 10, rules_x - 1, ACS_LLCORNER);
    mvaddch(rules_y + 10, rules_x + 30, ACS_LRCORNER);
    for (int x = rules_x; x < rules_x + 30; x++) {
        mvaddch(rules_y, x, ACS_HLINE);
        mvaddch(rules_y + 10, x, ACS_HLINE);
    }
    for (int y = rules_y + 1; y < rules_y + 10; y++) {
        mvaddch(y, rules_x - 1, ACS_VLINE);
        mvaddch(y, rules_x + 30, ACS_VLINE);
    }
    attroff(COLOR_PAIR(4));
    
    // Print rules header in white
    attron(A_BOLD | COLOR_PAIR(4));
    wchar_t rules_emoji[2] = {L'📋', L'\0'};
    mvaddwstr(rules_y + 1, rules_x, rules_emoji);
    mvprintw(rules_y + 1, rules_x + 2, " RULES ");
    attroff(A_BOLD | COLOR_PAIR(4));

    // Print rule text in white
    attron(COLOR_PAIR(4));
    mvprintw(rules_y + 3, rules_x, "1. Players take turns");
    mvprintw(rules_y + 4, rules_x + 2, "   placing tokens");
    mvprintw(rules_y + 5, rules_x, "2. Tokens drop to");
    mvprintw(rules_y + 6, rules_x + 2, "   lowest empty row");
    mvprintw(rules_y + 7, rules_x, "3. First to get 4");
    mvprintw(rules_y + 8, rules_x + 2, "   in a row wins!");
    wchar_t trophy2[2] = {L'🏆', L'\0'};
    mvaddwstr(rules_y + 8, rules_x + 18, trophy2);
    mvprintw(rules_y + 9, rules_x + 2, "(any direction)");
    attroff(COLOR_PAIR(4));

    // Draw controls screen with better formatting
    int controls_y = board_top + ROWS * SLOT_HEIGHT + 2;
    attron(COLOR_PAIR(4));
    wchar_t keyboard[2] = {L'⌨', L'\0'};
    mvaddwstr(controls_y, 2, keyboard);
    mvprintw(controls_y, 4, " CONTROLS: ");
    mvprintw(controls_y, 16, "← → Move | Space Place | q Quit");
    attroff(COLOR_PAIR(4));

    // Render
    refresh();
}

