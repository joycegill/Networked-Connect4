#include <ncurses.h>
#include <stdlib.h>

#define SLOT_HEIGHT 2
#define SLOT_WIDTH  4
#define ROWS        6
#define COLS        7

#define PLAYER_NONE 0
#define PLAYER_ONE  1
#define PLAYER_TWO  2
#define BOARD_COLOR 3

/**
 * Draw a single Connect-4 token at a specific board cell.
 * 
 * @param left The leftmost x-coordinate 
 * @param top The topmost y-coordinate 
 * @param col The column index where the token should be drawn 
 * @param row The row index where the token should be drawn
 * @param player The play identifier
*/
static void draw_token(int left, int top, int col, int row, unsigned char player) {
    int x = left + col * SLOT_WIDTH + 1;
    int y = top + row * SLOT_HEIGHT + 1;
    attron(COLOR_PAIR(player)); // Start drawing using the color pair for the given player
    for (int dx = 0; dx < SLOT_WIDTH - 1; ++dx)
        mvaddch(y, x + dx, ACS_BOARD); // Draw the token using that color
    attroff(COLOR_PAIR(player)); // Stop using the color
}

/**
 * Draw all tokens currently placed on the board
 * 
 * @param left The x-coordinate of the top-left corner of the board
 * @param top The y-coordinate of the top-left corner of the board
 * @param cells A pointer to the board array containing player values. 
 */
static void draw_tokens(int left, int top, const unsigned char *cells) {
    for (int row = 0; row < ROWS; ++row) { // Traversing through each row
        for (int col = 0; col < COLS; ++col) { // Check each column
            unsigned char player = cells[row * COLS + col];
            if (player != PLAYER_NONE) {
                draw_token(left, top, col, row, player);
            }
        }
    }
}

/**
 * Draw the Connect-4 board grid using ncurse ACS line characters.
 * 
 * @param left The x-coordinate where the grid begins
 * @param top The y-coordinate where the grid begins
 */
static void draw_grid(int left, int top) {
    int bottom = top + ROWS * SLOT_HEIGHT; // (16)
    int right = left + COLS * SLOT_WIDTH; // (32)

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

int main(void) {
    if (initscr() == NULL)
        return EXIT_FAILURE;

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();

    init_pair(PLAYER_ONE, COLOR_RED, COLOR_BLACK);
    init_pair(PLAYER_TWO, COLOR_YELLOW, COLOR_BLACK);
    init_pair(BOARD_COLOR, COLOR_BLUE, COLOR_BLACK);

    unsigned char cells[ROWS * COLS] = {0};
<<<<<<< Updated upstream
    cells[5 * COLS + 3] = PLAYER_ONE;
    cells[5 * COLS + 4] = PLAYER_TWO;
    cells[4 * COLS + 3] = PLAYER_ONE;

    clear();
    mvprintw(1, 2, "Connect 4 board demo (press q to exit)");
    draw_grid(4, 4);
    draw_tokens(4, 4, cells);
    refresh();

    int ch;
    while ((ch = getch()) != 'q' && ch != 'Q')
        ;
    endwin();
=======
    // cells[5 * COLS + 3] = PLAYER_ONE; // Example token for Player 1
    // cells[5 * COLS + 4] = PLAYER_TWO; // Example token for Player 2

    // Board position 
    int left = 4;
    int top = 4;

    // Track player turn
    unsigned char player = PLAYER_ONE;

    // Column currently selected
    int current_col = 0;

    while(1) {
        // Clear the screen and draw the board
        clear();
        mvprintw(1, 2, "Connect 4 board demo (press q to exit)"); // Display instructions
        draw_grid(4, 4);          // Draw the grid
        draw_tokens(4, 4, cells); // Draw the tokens
        // Draw drop indicator
        mvaddch(top - 1, left + current_col * SLOT_WIDTH + 1, '^');
        // Display turn info
        mvprintw(top + ROWS * SLOT_HEIGHT + 2, left,
                 "Player %d's turn (q to quit)", player);

        refresh();
        

        // LEFT: move left
        // RIGHT: move right
        // SPACE: select move
        // q: Quit
        int ch = getch();
        if (ch == 'q') {
            break; // exit if the user enter q or Q
        }

        switch (ch) {
            case KEY_LEFT: 
                current_col--;
                break;
            case KEY_RIGHT:
                current_col++;
                break;
            case ' ': {
                int row = find_row(current_col, cells);
                if (row != -1) {
                    cells[row*COLS + current_col] = player;

                    // Switch players
                    if (player == PLAYER_ONE) {
                        player = PLAYER_TWO;
                    } else {
                        player = PLAYER_ONE;
                    }
                    break;
                }
            }
        }
    }
    // refresh();                // Refresh the screen to display changes

    // // Wait for the user to enter q or Q to exit
    // int ch;
    // while ((ch = getch()) != 'q' && ch != 'Q')
    //     ;
    endwin(); 
    // End ncurses mode
>>>>>>> Stashed changes
    return EXIT_SUCCESS;
}
