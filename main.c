#include <ncurses.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

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
    int cursor_col;
    int move_made;
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
    attron(COLOR_PAIR(player));
    for (int dx = 0; dx < SLOT_WIDTH - 1; ++dx)
        // Draw the token
        // Cite: https://linux.die.net/man/3/mvaddch 
        mvaddch(y, x + dx, ACS_BOARD);
    attroff(COLOR_PAIR(player));
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
    for (int row = ROWS - 1; row >= 0; row--){
        if (cells[row*COLS + col] == PLAYER_NONE) {
            return row;
        }
    }
    return -1; // No space available 
}

// Check if a player has won by counting tokens in a direction
static int count_in_direction(const unsigned char *cells, int row, int col, 
                               int row_step, int col_step, unsigned char player) {
    int count = 0;
    int current_row = row;
    int current_col = col;
    
    // Count tokens to the right
    while (current_row >= 0 && current_row < ROWS && 
           current_col >= 0 && current_col < COLS &&
           cells[current_row * COLS + current_col] == player) {
        count++;
        current_row += row_step;
        current_col += col_step;
    }
    
    // Count tokens to the left
    current_row = row - row_step;
    current_col = col - col_step;
    while (current_row >= 0 && current_row < ROWS && 
           current_col >= 0 && current_col < COLS &&
           cells[current_row * COLS + current_col] == player) {
        count++;
        current_row -= row_step;
        current_col -= col_step;
    }
    
    return count;
}

// Check if placing a token at (row, col) results in a win
int check_win(const unsigned char *cells, int row, int col, unsigned char player) {
    // Check horizontal (left-right)
    if (count_in_direction(cells, row, col, 0, 1, player) >= 4)
        return 1;
    
    // Check vertical (up-down)
    if (count_in_direction(cells, row, col, 1, 0, player) >= 4)
        return 1;
    
    // Check diagonal (top-left to bottom-right)
    if (count_in_direction(cells, row, col, 1, 1, player) >= 4)
        return 1;
    
    // Check diagonal (top-right to bottom-left)
    if (count_in_direction(cells, row, col, 1, -1, player) >= 4)
        return 1;
    
    return 0;
}

// Draw condition
// Check if the board is completely full 
int is_board_full(const unsigned char *cells) {
    // Check if top row (row 0) is completely filled
    for (int col = 0; col < COLS; col++) {
        if (cells[0 * COLS + col] == PLAYER_NONE)
            return 0;
    }
    return 1;
}

// Player thread using round robin
void* player_thread(void *arg) {
    unsigned char my_player = *(unsigned char*)arg;
    
    while (1) {
        pthread_mutex_lock(&game.mutex);
        
        // Wait until it's my turn AND a move has been made
        while ((game.current_player != my_player || !game.move_made) && !game.game_over) {
            pthread_cond_wait(&game.turn_cond, &game.mutex);
        }
        
        // Exit if game ended
        if (game.game_over) {
            pthread_mutex_unlock(&game.mutex);
            break;
        }
        
        // Copy move data and reset flag immediately
        // Prevent race condition
        int selected_column = game.selected_col;
        game.move_made = 0;
        
        // Find where token will land and place it
        int landing_row = find_row(selected_column, game.cells);
        game.cells[landing_row * COLS + selected_column] = my_player;
        
        // Check game end conditions
        if (check_win(game.cells, landing_row, selected_column, my_player)) {
            game.winner = my_player;
            game.game_over = 1;
        } else if (is_board_full(game.cells)) {
            game.winner = PLAYER_NONE;
            game.game_over = 1;
        } else {
            // Switch to other player (round-robin)
            game.current_player = (my_player == PLAYER_ONE) ? PLAYER_TWO : PLAYER_ONE;
        }
        
        // Cite: https://linux.die.net/man/3/pthread_cond_broadcast
        pthread_cond_broadcast(&game.turn_cond);
        pthread_mutex_unlock(&game.mutex);
    }
    
    return NULL;
}

// Update and redraw the entire game display
void update_display(void) {
    clear();
    // Cite: https://linux.die.net/man/3/mvprintw 
    mvprintw(0, 2, "Connect 4");
    
    // Show game status message
    if (game.game_over) {
        if (game.winner == PLAYER_NONE) {
            mvprintw(2, 2, "Game Over: Draw!");
        } else {
            mvprintw(2, 2, "Game Over: Player %d Wins!", game.winner);
        }
    } else {
        mvprintw(2, 2, "Player %d's turn (Arrow keys to move, Space to place, q to quit)", 
                 game.current_player);
    }
    
    // Draw the board
    draw_grid(4, 4);
    draw_tokens(4, 4, game.cells);
    
    // Draw cursor indicator above selected column
    if (!game.game_over) {
        int cursor_x = 4 + game.cursor_col * SLOT_WIDTH + SLOT_WIDTH / 2;
        int cursor_y = 3;
        attron(COLOR_PAIR(BOARD_COLOR));
        mvaddch(cursor_y, cursor_x, '^');
        attroff(COLOR_PAIR(BOARD_COLOR));
    }
    
    // Show controls
    mvprintw(ROWS * SLOT_HEIGHT + 6, 2, "Controls: Arrow keys = Move cursor, Space = Place token, q = Quit");
    refresh();
}

int main(void) {
    // Initialize ncurses
    if (initscr() == NULL)
        return EXIT_FAILURE;

    // Configure ncurses environment
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();

    // Initialize color pairs for players and the board
    init_pair(PLAYER_ONE, COLOR_RED, COLOR_BLACK);
    init_pair(PLAYER_TWO, COLOR_YELLOW, COLOR_BLACK);
    init_pair(BOARD_COLOR, COLOR_BLUE, COLOR_BLACK);

    // Initialize the board state
    for (int i = 0; i < ROWS * COLS; i++)
        game.cells[i] = PLAYER_NONE;
    
    game.current_player = PLAYER_ONE;
    game.winner = PLAYER_NONE;
    game.selected_col = 0;
    game.cursor_col = 3;
    game.move_made = 0;
    game.game_over = 0;
    
    pthread_mutex_init(&game.mutex, NULL);
    pthread_cond_init(&game.turn_cond, NULL);

    unsigned char player_one_id = PLAYER_ONE;
    unsigned char player_two_id = PLAYER_TWO;
    pthread_t player_one_thread;
    pthread_t player_two_thread;
    
    pthread_create(&player_one_thread, NULL, player_thread, &player_one_id);
    pthread_create(&player_two_thread, NULL, player_thread, &player_two_id);

    update_display();
    
    // Main game loop: handle user input
    int user_input;
    while ((user_input = getch()) != 'q' && user_input != 'Q') {
        // Ignore input if game is over
        if (game.game_over)
            continue;
        
        pthread_mutex_lock(&game.mutex);
        
        // Handle arrow key navigation
        if (user_input == KEY_LEFT && game.cursor_col > 0) {
            game.cursor_col--;
            pthread_mutex_unlock(&game.mutex);
            update_display();
            continue;
        }
        if (user_input == KEY_RIGHT && game.cursor_col < COLS - 1) {
            game.cursor_col++;
            pthread_mutex_unlock(&game.mutex);
            update_display();
            continue;
        }
        
        // Handle token placement (Space)
        if (user_input == ' ') {
            int selected_column = game.cursor_col;
            // Check if column has space before accepting the move
            int landing_row = find_row(selected_column, game.cells);
            
            // Only accept move if column has space, it's a valid player's turn, and no move is pending
            if (landing_row != -1 && game.current_player != 0 && !game.move_made) {
                game.selected_col = selected_column;
                game.move_made = 1;
                // Wake up the player thread waiting for a move
                pthread_cond_broadcast(&game.turn_cond);
                
                // Wait for move to be processed (move_made will be reset by thread)
                while (game.move_made && !game.game_over) {
                    pthread_cond_wait(&game.turn_cond, &game.mutex);
                }
            }
        }
        
        pthread_mutex_unlock(&game.mutex);
        update_display();
    }

    // Signal threads to exit
    pthread_mutex_lock(&game.mutex);
    game.game_over = 1;
    pthread_cond_broadcast(&game.turn_cond);
    pthread_mutex_unlock(&game.mutex);

    // Wait for both threads to finish
    pthread_join(player_one_thread, NULL);
    pthread_join(player_two_thread, NULL);

    // Clean up synchronization primitives
    pthread_mutex_destroy(&game.mutex);
    pthread_cond_destroy(&game.turn_cond);

    endwin();
    // End ncurses mode
    return EXIT_SUCCESS;
}
