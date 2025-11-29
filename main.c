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
    unsigned char cells[ROWS * COLS] = {0};
    cells[5 * COLS + 3] = PLAYER_ONE; // Example token for Player 1
    cells[5 * COLS + 4] = PLAYER_TWO; // Example token for Player 2

    // Clear the screen and draw the board
    clear();
    mvprintw(1, 2, "Connect 4 board demo (press q to exit)"); // Display instructions
    draw_grid(4, 4);          // Draw the grid
    draw_tokens(4, 4, cells); // Draw the tokens
    refresh();                // Refresh the screen to display changes

    // Wait for the user to enter q or Q to exit
    int ch;
    while ((ch = getch()) != 'q' && ch != 'Q')
        ;
    endwin(); 
    // End ncurses mode
    return EXIT_SUCCESS;
}
