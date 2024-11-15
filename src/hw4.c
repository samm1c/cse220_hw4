#include <stdio.h> // library for many commonly-used functions
#include <stdlib.h> //            memory management
#include <string.h> //            string manipulation
#include <unistd.h> //            close function
#include <arpa/inet.h> //         conversion btwn binary and text IP format
#include <sys/socket.h> //        socket functions

#define PORT1 2201 // -> player 1
#define PORT2 2202 // -> player 2
#define BUFFER_SIZE 1024

/* define objects: */
typedef struct {
    int socket;
    char **board;
    int num_guesses;
    int ships_remaining;    // number of PLAYER's own ships remaining
} Player;

/* function prototypes: */
char **create_board(int rows, int cols);
void free_board(char **board, int rows);
void print_board(char **board, int height, int width);
void clear_board(char ***board, int rows, int cols);
int can_place_ship(Player player, int piece[4][4], int row, int col, int width, int height);
void place_ship(Player *player, int piece[4][4], int row, int col);
void anchor(int piece[4][4], int *row, int *col);
char *build_query(Player player, Player enemy, int height, int width);
char *build_shot(Player player, Player enemy, char result);
int is_ship_destroyed(char **board, int row, int col, int width, int height);
void forfeit(Player *player, Player *enemy, int height);
int min(int a, int b);

// defines the different shapes/rotations ships can take on
int pieces[7][4][4][4] = { // expecting 7 types of shapes, 4 different rotations for each, each represented in a 4x4 grid
    { // shape 1 -> square!
        {
            {1, 1, 0, 0},
            {1, 1, 0, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 1, 0, 0},
            {1, 1, 0, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 1, 0, 0},
            {1, 1, 0, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 1, 0, 0},
            {1, 1, 0, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        }
    },

    { // shape 2 -> long 4
        {
            {1, 0, 0, 0},
            {1, 0, 0, 0},
            {1, 0, 0, 0},
            {1, 0, 0, 0}
        },
        {
            {1, 1, 1, 1},
            {0, 0, 0, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 0, 0, 0},
            {1, 0, 0, 0},
            {1, 0, 0, 0},
            {1, 0, 0, 0}
        },
        {
            {1, 1, 1, 1},
            {0, 0, 0, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        }
    },

    { // shape 3 -> s
        {
            {0, 1, 1, 0},
            {1, 1, 0, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 0, 0, 0},
            {1, 1, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {0, 1, 1, 0},
            {1, 1, 0, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 0, 0, 0},
            {1, 1, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 0, 0}
        }
    },

    { // shape 4 -> L
        {
            {1, 0, 0, 0},
            {1, 0, 0, 0},
            {1, 1, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 1, 1, 0},
            {1, 0, 0, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 1, 0, 0},
            {0, 1, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {0, 0, 1, 0},
            {1, 1, 1, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        }
    },

    { // shape 5 -> z
        {
            {1, 1, 0, 0},
            {0, 1, 1, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {0, 1, 0, 0},
            {1, 1, 0, 0},
            {1, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 1, 0, 0},
            {0, 1, 1, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {0, 1, 0, 0},
            {1, 1, 0, 0},
            {1, 0, 0, 0},
            {0, 0, 0, 0}
        }
    },

    { // shape 6 -> mirrored L
        {
            {0, 1, 0, 0},
            {0, 1, 0, 0},
            {1, 1, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 0, 0, 0},
            {1, 1, 1, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 1, 0, 0},
            {1, 0, 0, 0},
            {1, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 1, 1, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        }
    },

    { // shape 7 -> T
        {
            {1, 1, 1, 0},
            {0, 1, 0, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {0, 1, 0, 0},
            {1, 1, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {0, 1, 0, 0},
            {1, 1, 1, 0},
            {0, 0, 0, 0},
            {0, 0, 0, 0}
        },
        {
            {1, 0, 0, 0},
            {1, 1, 0, 0},
            {1, 0, 0, 0},
            {0, 0, 0, 0}
        }
    }
};

int main() {
    /* initialize variables for 2 players -> 2 different ports */
    int listen_fd_p1, listen_fd_p2;
    int conn_p1, conn_p2;
    struct sockaddr_in address1, address2;
    int opt = 1;
    int addrlen1 = sizeof(address1);
    int addrlen2 = sizeof(address2);   
    char buffer[BUFFER_SIZE] = {0};

    /* create sockets */
    if ( (listen_fd_p1 = socket(AF_INET, SOCK_STREAM, 0)) == 0 ) {
        perror("socket for player 1 failed!");
        exit(EXIT_FAILURE);
    }
    if ( (listen_fd_p2 = socket(AF_INET, SOCK_STREAM, 0)) == 0 ) {
        perror("socket for player 2 failed!");
        exit(EXIT_FAILURE);
    }

    /* set socket options */
    if (setsockopt(listen_fd_p1, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt() p1 addr failed!");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(listen_fd_p2, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt() p2 addr failed!");
        exit(EXIT_FAILURE);
    }

    /* bind sockets to their respective ports */
    address1.sin_family = AF_INET;
    address1.sin_addr.s_addr = INADDR_ANY;
    address1.sin_port = htons(PORT1);
    if (bind(listen_fd_p1, (struct sockaddr *)&address1, sizeof(address1)) < 0) {
        perror("[Server] bind()1 failed.");
        exit(EXIT_FAILURE);
    }

    address2.sin_family = AF_INET;
    address2.sin_addr.s_addr = INADDR_ANY;
    address2.sin_port = htons(PORT2);
    if (bind(listen_fd_p2, (struct sockaddr *)&address2, sizeof(address2)) < 0) {
        perror("[Server] bind()2 failed.");
        exit(EXIT_FAILURE);
    }

    /* listen for incoming connections; maximum 2 players */
    if (listen(listen_fd_p1, 3) < 0) {
        perror("[Server] listen() failed.");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd_p2, 3) < 0) {
        perror("[Server] listen() failed.");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Listening on port %d and %d\n", PORT1, PORT2);

    /* accept connection from player 1 */
    printf("[Server] Connecting player 1...\n");
    if ( (conn_p1 = accept(listen_fd_p1, (struct sockaddr *)&address1, (socklen_t *)&addrlen1)) < 0 ) {
        perror("[Server] player1's accept() failed.");
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 1 connected.\n");
    
    /* accept connection from player 2 */
    printf("[Server] Connecting player 2...\n");
    if ( (conn_p2 = accept(listen_fd_p2, (struct sockaddr *)&address2, (socklen_t *)&addrlen2)) < 0 ) {
        perror("[Server] player2's accept() failed.");
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 2 connected.\n");

    // close listen_fd because we only need conn_fd now
    close(listen_fd_p1);
    close(listen_fd_p2);

    /* battleship! */

    /* BEGIN */
    // variables
    char packet_type = ' ';
    int width = 0, height = 0, num_bytes = 0, num_scanned = 0;
    char msg[BUFFER_SIZE];
    int playing = 1; // true!
    char *ptr;

    // create the 2 players
    Player players[2] = {
        {.socket = conn_p1, .board = NULL, .num_guesses = 0, .ships_remaining = 0},
        {.socket = conn_p2, .board = NULL, .num_guesses = 0, .ships_remaining = 0}
    };

    // player 1 declares board size first
    while (playing) {
        memset(buffer, 0, sizeof(buffer)); // clear buffer
        num_bytes = read(conn_p1, buffer, BUFFER_SIZE - 1);
        buffer[num_bytes] = '\0'; // add null character at end 
        ptr = &buffer[2];

        printf("[Server] Received: %s\n", buffer);

        num_scanned = sscanf(buffer, "%c %d %d", &packet_type, &width, &height);  // read "B 11 11"
        
        while (*ptr >= 'A' && *ptr <= 'Z') { ptr++; } // skip first char
        while (*ptr == ' ') { ptr++; }                // skip first space
        while (*ptr >= '0' && *ptr <= '9') { ptr++; } // skip first number
        while (*ptr == ' ') { ptr++; }                // skip second space
        while (*ptr >= '0' && *ptr <= '9') { ptr++; } // skip second number

        if (packet_type == 'F') {
            printf("[Server] Player 1 has forfeited.\n");
            forfeit(&players[0], &players[1], height);
            return EXIT_SUCCESS;
        } else if (packet_type != 'B') { // not a Begin packet!!
            printf("[Server] E 100\n");
            send(conn_p1, "E 100", 5, 0);
            continue;
        } else if (num_scanned != 3 || *ptr != '\0' || width < 10 || height < 10) {
            printf("[Server] E 200\n");
            send(conn_p1, "E 200", 5, 0);
            continue;
        }

        // otherwise, success!
        send(conn_p1, "A", 1, 0);
        break;
    }

    // player 2 sends only "B"
    while (playing) {
        memset(buffer, 0, sizeof(buffer)); // clear buffer
        num_bytes = read(conn_p2, buffer, BUFFER_SIZE - 1);
        buffer[num_bytes] = '\0';
        packet_type = buffer[0];
        printf("[Server] Received: %s\n", buffer);

        if (packet_type == 'F') {
            printf("[Server] Player 2 has forfeited.\n");
            forfeit(&players[1], &players[0], height);
            return EXIT_FAILURE;
        } else if (packet_type != 'B') { // not a Begin packet!!
            printf("[Server] E 100\n");
            send(conn_p2, "E 100", 5, 0);
            continue;
        } else if (strcmp(buffer, "B") != 0) { // comparing strings b/c we want ONE B -> otherwise INVALID number of parameters
            printf("[Server] E 200\n"); // invalid packet type
            send(conn_p2, "E 200", 5, 0);
            continue;
        }

        // otherwise, success!
        send(conn_p2, "A", 1, 0);
        break;
    } 

    // create boards now that you have height and width
    players[0].board = create_board(height, width);
    players[1].board = create_board(height, width);

    /* INITIALIZE -> set 5 pieces for own board */
    for (int p = 0; p < 2; p++) {
        int not_initialized = 1;

        while (not_initialized) {
            not_initialized = 0; // assume it is initialized for now
            memset(buffer, 0, sizeof(buffer)); // clear buffer
            num_bytes = read(players[p].socket, buffer, BUFFER_SIZE - 1);
            buffer[num_bytes] = '\0';
            printf("[Server] Received: %s\n", buffer);

            packet_type = buffer[0];

            if (packet_type == 'F') {
                printf("[Server] Player %d has forfeited.\n", p+1);
                forfeit(&players[p], &players[p == 0 ? 1 : 0], height);
                return EXIT_SUCCESS;
            } else if (packet_type != 'I') { // not an Initialize packet!!
                printf("[Server] E 101\n");
                send(players[p].socket, "E 101", 5, 0);
                not_initialized = 1;
                continue;
            }

            ptr = &buffer[2]; // pointer to traverse through the buffer

            // make sure the number of parameters is correct!
            int moved = 0;
            for (int i = 0; i < 20; i++) {
                moved = 0;
                if (*ptr == '-') { moved = 1; ptr++; } // skip hyphen for negative
                while (*ptr >= '0' && *ptr <= '9') { moved = 1; ptr++; } // skip ith number
                while (*ptr == ' ') { moved = 1; ptr++; }
                
                if (!moved) {
                    break;
                }
            }

            if (!moved || *ptr != '\0') { // restart at beginning!
                printf("[Server] E 201\n");
                send(players[p].socket, "E 201", 5, 0);
                not_initialized = 1;
                continue;
            }

            ptr = &buffer[2]; // reset pointer

            int piece_types[5], piece_rotations[5], piece_columns[5], piece_rows[5];

            num_scanned = sscanf(ptr, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                                &piece_types[0], &piece_rotations[0], &piece_columns[0], &piece_rows[0],
                                &piece_types[1], &piece_rotations[1], &piece_columns[1], &piece_rows[1],
                                &piece_types[2], &piece_rotations[2], &piece_columns[2], &piece_rows[2],
                                &piece_types[3], &piece_rotations[3], &piece_columns[3], &piece_rows[3],
                                &piece_types[4], &piece_rotations[4], &piece_columns[4], &piece_rows[4]);
            
            // check for scan error
            if (num_scanned != 20) {
                printf("[Server] E 201\n");
                send(players[p].socket, "E 201", 5, 0);
                not_initialized = 1;
                continue;
            }

            int five_init = 1; // flag to keep track of whether 5 ships were successfully initialized
            int error = __INT_MAX__; // random number; holds lowest error code

            for (int i = 0; i < 5; i++) {
                // check for error
                if (piece_types[i] < 1 || piece_types[i] > 7) {
                    error = min(error, 300);
                    // bookkeeping  
                    clear_board(&players[p].board, height, width);
                    players[p].ships_remaining = 0;
                    not_initialized = 1;
                    five_init = 0;
                    continue;
                } else if (piece_rotations[i] < 1 || piece_rotations[i] > 4) {
                    error = min(error, 301);
                    // bookkeeping
                    clear_board(&players[p].board, height, width);
                    players[p].ships_remaining = 0;
                    not_initialized = 1;
                    five_init = 0;
                    continue;
                }
                
                // because my indices are 0-6 and 0-3 respectively
                piece_types[i]--; 
                piece_rotations[i]--;

                // no error yet -> copy piece type into piece -> have the 4x4 tetris piece on hand
                int piece[4][4];
                for (int x = 0; x < 4; x++) {
                    for (int y = 0; y < 4; y++) {
                        piece[x][y] = pieces[piece_types[i]][piece_rotations[i]][x][y];
                    }
                }

                // fix coordinates b/c of how the piece works
                anchor(piece, &piece_rows[i], &piece_columns[i]);

                // place piece on board
                int outcome = can_place_ship(players[p], piece, piece_rows[i], piece_columns[i], width, height);

                if (outcome == 1) {
                    place_ship(&players[p], piece, piece_rows[i], piece_columns[i]);
                } else { 
                    if (outcome == 302) {
                        error = min(error, 302);
                    } else if (outcome == 303) {
                        error = min(error, 303);
                    }
                    // bookkeeping
                    clear_board(&players[p].board, height, width);
                    players[p].ships_remaining = 0;
                    not_initialized = 1;
                    five_init = 0;
                }
            }

            // success! this player has initialized 5 ships on their own board!!
            if (five_init) {
                send(players[p].socket, "A", 1, 0);
                    printf("-------------------\n");
                    printf("Player %d:\n", p+1);
                    print_board(players[p].board, height, width);
                    printf("-------------------\n");
                break;
            } else { // not initialized -> print error first! -> back to the drawing board!!! 
                char error_str[6];
                sprintf(error_str, "E %d", error);
                printf("[Server] %s\n", error_str);
                send(players[p].socket, error_str, 6, 0);

                // bookkeeping
                clear_board(&players[p].board, height, width);
                players[p].ships_remaining = 0;
            }
        }
    }

    /* PLAY! */
    while (playing) {
        // process player's packet
        for (int p = 0; p < 2; p++) {
            memset(buffer, 0, sizeof(buffer)); // clear buffer
            num_bytes = read(players[p].socket, buffer, BUFFER_SIZE - 1);
            buffer[num_bytes] = '\0';
            packet_type = buffer[0];

            int enemy = (p == 0) ? 1 : 0; // other player's index!

            switch (packet_type) {
                case 'S': {
                    int row = -1, column = -1;
                    char result = ' ';
                    // check for errors
                    ptr = &buffer[2];
                    if (*ptr == '-') { ptr++; } // skip hyphen for negative
                    while (*ptr >= '0' && *ptr <= '9') { ptr++; } // skip first number
                    while (*ptr == ' ')                { ptr++; } // skip first space
                    if (*ptr == '-') { ptr++; } // skip hyphen for negative
                    while (*ptr >= '0' && *ptr <= '9') { ptr++; } // skip second number

                    if (sscanf(buffer, "%c %d %d", &packet_type, &row, &column) != 3 || *ptr != '\0') { // # of parameters; must end properly
                        printf("[Server] E 202\n");
                        send(players[p].socket, "E 202", 5, 0);
                        p--; // restart! the increment p++ won't do anything, back to same index
                        break;
                    } else if (row < 0 || column < 0 || row >= height || column >= width) { // out of bounds
                        printf("[Server] E 400\n");
                        send(players[p].socket, "E 400", 5, 0);
                        p--; 
                        break;
                    } 
                    
                    // check spot
                    if (players[enemy].board[row][column] == 'H' || players[enemy].board[row][column] == 'M') { // already guessed!
                        printf("[Server] E 401\n");
                        send(players[p].socket, "E 401", 5, 0);
                        p--;
                        break;
                    } else if (players[enemy].board[row][column] == '~') { // has to be ~ water so miss
                        result = 'M';
                        players[enemy].board[row][column] = 'M';
                    } else { // ship!!! b/c it's not water -> ship number
                        result = 'H';
                        if (is_ship_destroyed(players[enemy].board, row, column, width, height)) {
                            players[enemy].ships_remaining--;
                        }
                        players[enemy].board[row][column] = 'H'; // update it on the enemy's board
                    }
                    printf("Player %d:\n", p+1);
                    print_board(players[enemy].board, height, width);

                    players[p].num_guesses++;

                    char *shot = build_shot(players[p], players[enemy], result);
                    send(players[p].socket, shot, strlen(shot) + 1, 0);
                    free(shot);
                    
                    break;
                }
                case 'Q': {
                    char *query = build_query(players[p], players[enemy], height, width);
                    send(players[p].socket, query, strlen(query) + 1, 0);
                    free(query); // because it's dynamically allocated
                    p--; // restart! still that player's turn
                    break;
                }
                case 'F': // forfeit
                    printf("[Server] Player %d has forfeited.\n", p+1);
                    forfeit(&players[p], &players[enemy], height);
                    return EXIT_SUCCESS;
                default: // otherwise, invalid packet type!! (not shoot, query, forfeit)
                    printf("[Server] E 102\n");
                    send(players[p].socket, "E 102", 5, 0);
                    p--;
                    break;
            }

            // after each turn -> check game status -> halt packet
            if (players[enemy].ships_remaining == 0) { // game is over and current player has won
                send(players[p].socket, "H 1", 3, 0);

                read(players[enemy].socket, buffer, BUFFER_SIZE - 1); // force a read from enemy
                send(players[enemy].socket, "H 0", 3, 0);

                // end game!!
                /* free boards */
                free_board(players[0].board, height);
                free_board(players[1].board, height);
                /* shut down server */
                printf("[Server] Shutting down.\n");
                close(conn_p1);
                close(conn_p2);
                return EXIT_SUCCESS;
            } // else continue on with the game!
        }
    }
}

char **create_board(int rows, int cols) {
    /* allocate memory for each row */
    char **board = malloc(rows * sizeof(char *));
    
    /* allocate each row's columns */
    for (int i = 0; i < rows; i++) {
        board[i] = malloc(cols * sizeof(char));
    }

    /* initialize entire board to ~ to represent water by using memset on each row */
    for (int i = 0; i < rows; i++) {
        memset(board[i], '~', cols * sizeof(char));
    }

    return board;
}

void free_board(char **board, int rows) {
    for (int i = 0; i < rows; i++) { // free every row
        free(board[i]);
    }
    free(board);
}

void print_board(char **board, int height, int width) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            printf("%c ", board[i][j]);
        }
        printf("\n");
    }
}

void clear_board(char ***board, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        memset((*board)[i], '~', cols * sizeof(char));
    }
}

/*
    pre-condition: row, col is anchored such that it aligns the piece (black circle)
*/
int can_place_ship(Player player, int piece[4][4], int row, int col, int width, int height) {
    // iterate over each cell in the piece and check if there are any 1's we can offset the new row and column indices
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece[i][j] == 1) {
                // update new board coordinates
                int new_row = row + i;
                int new_col = col + j;

                // check if out of bounds
                if (new_row < 0 || new_col < 0 || new_row >= height || new_col >= width) {  
                    return 302;
                }
                // check if overlapping another ship
                if (player.board[new_row][new_col] != '~') {
                    return 303;
                }
            }
        }
    }
    return 1; // made it through without returning 0 -> it's possible!
}

/*
    pre-condition: row, col is anchored such that it aligns with the piece (black circle)
                   given ship must be valid (checked via can_place_ship())
*/
void place_ship(Player *player, int piece[4][4], int row, int col) {
    // iterate over each cell in the piece so that we can update the board accordingly
    player->ships_remaining++;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece[i][j] == 1) {
                int new_row = row + i;
                int new_col = col + j;
                char c = player->ships_remaining + '0';
                player->board[new_row][new_col] = c; // insert into board!!
            }
        }
    }
}

void anchor(int piece[4][4], int *row, int *col) {
    if ((piece[0][0] == 0) && (piece[1][0] == 0)) { // pieces[5][0] -> mirrored L needs y+2
        *row -= 2;
    } else if (piece[0][0] == 0) {
        *row -= 1;
    }
    // else, leave as is, anchor is [0][0] by default
}

char *build_query(Player player, Player enemy, int height, int width) {
    int size = (player.num_guesses * 6) + 5;
    char *str = malloc(size);

    snprintf(str, size, "G %d", enemy.ships_remaining);

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (enemy.board[i][j] == 'H' || enemy.board[i][j] == 'M') {
                char guess[50];
                snprintf(guess, size, " %c %d %d", enemy.board[i][j], i, j);
                int total_size = strlen(str) + strlen(guess) + 1;
                if (total_size > size) { // double size if too small to fit this guess
                    size *= 2;
                    str = realloc(str, size);
                }
                strcat(str, guess);
            } // otherwise don't do anything, just keep check next index
        }
    }

    return str;
}

char *build_shot(Player player, Player enemy, char result) {
    int size = 20;
    char *str = malloc(size);
    snprintf(str, size, "R %d %c", enemy.ships_remaining, result);
    return str;
}

int is_ship_destroyed(char **board, int row, int col, int width, int height) {
    char c = board[row][col];
    for (int i = 0; i < height; i++) { // search the board if there's any of this ship number left
        for (int j = 0; j < width; j++) {
            if ((i != row || j != col) && board[i][j] == c) { // is this index the only one left?
                return 0; // false, found it
            }
        }
    }
    return 1;
}

void forfeit(Player *player, Player *enemy, int height) {
    char buffer[BUFFER_SIZE] = {0};
    send(player->socket, "H 0", 3, 0); // current player lost! -> will shut it down

    read(enemy->socket, buffer, BUFFER_SIZE - 1); // force a read from the other player
    send(enemy->socket, "H 1", 3, 0); // doesn't matter what the input is; enemy won! -> shut down

    /* free boards (if created) */
    if (player->board != NULL) {
        free_board(player->board, height);
    }
    if (enemy->board != NULL) {
        free_board(enemy->board, height);
    }

    close(player->socket);
    close(enemy->socket);
}

int min(int a, int b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}