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
    char **ship_board;      // own board to place your 5 ships
    char **guessing_board;  // blank board to track your guesses
    int num_guesses;
    int ships_remaining;    // number of PLAYER's own ships remaining
} Player;

/* function prototypes: */
char **create_board(int rows, int cols);
void free_board(char **board, int rows);
void print_board(char **board, int height, int width);
int can_place_ship(Player player, int piece[4][4], int row, int col, int width, int height);
void place_ship(Player *player, int piece[4][4], int row, int col);
void anchor(int piece[4][4], int *row, int *col);
char *build_query(Player player, int height, int width);
char *build_shot(Player player, char result);
int is_ship_destroyed(char **board, int row, int col, int width, int height);
char *build_board_str(char **board, int height, int width);
void forfeit(Player *player, Player *enemy);

// defines the different shapes/rotations ships can take on
int pieces[7][4][4][4] = { // expecting 7 types of shapes, 4 different rotations for each, each represented in a 4x4 grid
    { // shape 1 -> square!
        {
            {1, 1},
            {1, 1}
        },
        {
            {1, 1},
            {1, 1}
        },
        {
            {1, 1},
            {1, 1}
        },
        {
            {1, 1},
            {1, 1}
        }
    },

    { // shape 2 -> long 4
        {
            {1},
            {1},
            {1},
            {1}
        },
        {
            {1, 1, 1, 1}
        },
        {
            {1},
            {1},
            {1},
            {1}
        },
        {
            {1, 1, 1, 1}
        }
    },

    { // shape 3 -> s
        {
            {0, 1, 1},
            {1, 1, 0}
        },
        {
            {1},
            {1, 1},
            {0, 1}
        },
        {
            {0, 1, 1},
            {1, 1, 0}
        },
        {
            {1},
            {1, 1},
            {0, 1}
        }
    },

    { // shape 4 -> L
        {
            {1},
            {1},
            {1, 1}
        },
        {
            {1, 1, 1},
            {1}
        },
        {
            {1, 1},
            {0, 1},
            {0, 1}
        },
        {
            {0, 0, 1},
            {1, 1, 1}
        }
    },

    { // shape 5 -> z
        {
            {1, 1},
            {0, 1, 1}
        },
        {
            {0, 1},
            {1, 1},
            {1}
        },
        {
            {1, 1},
            {0, 1, 1}
        },
        {
            {0, 1},
            {1, 1},
            {1}
        }
    },

    { // shape 6 -> mirrored L
        {
            {0, 1},
            {0, 1},
            {1, 1}
        },
        {
            {1},
            {1, 1, 1}
        },
        {
            {1, 1},
            {1},
            {1}
        },
        {
            {1, 1, 1},
            {0, 0, 1}
        }
    },

    { // shape 7 -> T
        {
            {1, 1, 1},
            {0, 1, 0}
        },
        {
            {0, 1},
            {1, 1},
            {0, 1}
        },
        {
            {0, 1, 0},
            {1, 1, 1}
        },
        {
            {1},
            {1, 1},
            {1}
        }
    }
};

int main() {
    /* initialize variables for 2 players -> 2 different ports */
    int listen_fd_p1, listen_fd_p2;
    int conn_p1, conn_p2;
    struct sockaddr_in address1, address2;
    int opt = 1;
    int addrlen = sizeof(address1); // only need one
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
    if ( (conn_p1 = accept(listen_fd_p1, (struct sockaddr *)&address1, (socklen_t *)&addrlen)) < 0 ) {
        perror("[Server] player1's accept() failed.");
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 1 connected.\n");
    
    /* accept connection from player 2 */
    printf("[Server] Connecting player 2...\n");
    if ( (conn_p2 = accept(listen_fd_p2, (struct sockaddr *)&address2, (socklen_t *)&addrlen)) < 0 ) {
        perror("[Server] player2's accept() failed.");
        exit(EXIT_FAILURE);
    }
    printf("[Server] Player 2 connected.\n");

    // close listen_fd because we only need conn_fd now
    close(listen_fd_p1);
    close(listen_fd_p2);

    /* battleship! */

    /* part 1 */
    /* BEGIN */

    // variables
    char packet_type = ' ';
    int width = 0, height = 0, num_bytes = 0, num_scanned = 0;
    char msg[BUFFER_SIZE];
    int playing = 1; // true!

    // create the 2 players
    Player players[2] = {
        {.socket = conn_p1, .num_guesses = 0, .ships_remaining = 5},
        {.socket = conn_p2, .num_guesses = 0, .ships_remaining = 5}
    };

    // player 1 declares board size first
    while (playing) {
        memset(buffer, 0, sizeof(buffer)); // clear buffer
        num_bytes = read(conn_p1, buffer, BUFFER_SIZE - 1);
        buffer[num_bytes] = '\0'; // add null character at end 

        printf("[Server] Received: %s\n", buffer);

        num_scanned = sscanf(buffer, "%c %d %d", &packet_type, &width, &height);  // read "B 11 11"

        if (packet_type == 'F') {
            printf("[Server] Player 1 has forfeited.\n");
            forfeit(&players[0], &players[1]);
            //printf("hi forfeit was done");
            return EXIT_SUCCESS;
        } else if (packet_type != 'B') { // not a Begin packet!!
            printf("[Server] E 100\n");
            send(conn_p1, "E 100", 5, 0);
            continue;
        } else if (num_scanned != 3 || width < 10 || height < 10) {
            printf("[Server] E 200\n");
            send(conn_p1, "E 200", 5, 0);
            continue;
        }

        // otherwise, success!
        send(conn_p1, "A", 1, 0);
        break;
    }

    // player 2 sends only "B"
    playing = 1;
    while (playing) {
        memset(buffer, 0, sizeof(buffer)); // clear buffer
        num_bytes = read(conn_p2, buffer, BUFFER_SIZE - 1);
        buffer[num_bytes] = '\0';
        packet_type = buffer[0];
        printf("[Server] Received: %s\n", buffer);

        if (packet_type == 'F') {
            printf("[Server] Player 2 has forfeited.\n");
            forfeit(&players[1], &players[0]);
            return EXIT_FAILURE;
        } else if (packet_type != 'B') { // not a Begin packet!!
            printf("[Server] E 100\n");
            send(conn_p2, "E 100", 5, 0);
            break;
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
    players[0].ship_board = create_board(height, width);
    players[0].guessing_board = create_board(height, width);
    players[1].ship_board = create_board(height, width);
    players[1].guessing_board = create_board(height, width);

    /* INITIALIZE -> set 5 pieces for own board */

    // player 1
    for (int p = 0; p < 2; p++) {
        int not_initialized = 1; // the 5 pieces have been initialized on player's own board
        while (not_initialized) {
            not_initialized = 0; // assume everything is fine for now
            memset(buffer, 0, sizeof(buffer)); // clear buffer
            num_bytes = read(players[p].socket, buffer, BUFFER_SIZE - 1);
            buffer[num_bytes] = '\0';

            packet_type = buffer[0];

            if (packet_type != 'I') { // not an Initialize packet!!
                printf("[Server] E 101\n");
                send(players[p].socket, "E 101", 5, 0);
                not_initialized = 1;
                continue;
            }

            char *ptr = &buffer[2]; // pointer to traverse through the buffer
            int piece_type = -1, piece_rotation = -1, piece_column = -1, piece_row = -1;

            for (int i = 0; i < 5; i++) { // initialize 5 pieces
                num_scanned = sscanf(ptr, "%d %d %d %d", &piece_type, &piece_rotation, &piece_column, &piece_row);

                // check for error
                if (num_scanned != 4) {
                    printf("[Server] E 201\n");
                    send(players[p].socket, "E 201", 5, 0);
                    not_initialized = 1;
                    break;
                } else if (piece_type < 0 || piece_type > 6) {
                    printf("[Server] E 300\n");
                    send(players[p].socket, "E 300", 5, 0);
                    not_initialized = 1;
                    break;
                }
                if (piece_rotation < 0 || piece_rotation > 3) {
                    printf("[Server] E 301\n");
                    send(players[p].socket, "E 301", 5, 0);
                    not_initialized = 1;
                    break;
                }

                // no error -> copy piece type into piece -> have the 4x4 tetris piece on hand
                int piece[4][4];
                for (int x = 0; x < 4; x++) {
                    for (int y = 0; y < 4; y++) {
                        piece[x][y] = pieces[piece_type][piece_rotation][x][y];
                    }
                }

                // fix coordinates b/c of how the piece works
                anchor(piece, &piece_row, &piece_column);

                // place piece on board
                if (can_place_ship(players[p], piece, piece_row, piece_column, width, height)) {
                    place_ship(&players[p], piece, piece_column, piece_row);
                } else { 
                    // already printed error message from can_place_ship
                    not_initialized = 1; 
                    break;
                }
                
                // update / move the pointer to the next number!
                for (int j = 0; j < 4; j++) { // skip 4 numbers + spaces
                    while (*ptr >= '0' && *ptr <= '9') { // skip number (no matter how big)
                        ptr++;
                    }
                    while (*ptr == ' ') { // skip space
                        ptr++;
                    }
                }
                // pointer should now be pointing to the next number
            }
        }

        if (not_initialized) {
            continue;
        } else { // success! player has initialized 5 ships on their own board!!
            send(players[p].socket, "A", 1, 0);
            break;
        }
    }

    // print the initialized ship boards in the server
    print_board(players[0].ship_board, height, width);
    print_board(players[1].ship_board, height, width);
    // print the initialized ship boards in the client
    char *p1_board = build_board_str(players[0].ship_board, height, width);
    send(players[0].socket, p1_board, sizeof(p1_board), 0);
    free(p1_board);
    char *p2_board = build_board_str(players[0].ship_board, height, width);
    send(players[1].socket, p2_board, sizeof(p2_board), 0);
    free(p2_board);

    /* play game! */
    while (playing) {
        // process player's packet
        for (int p = 0; p < 2; p++) {
            memset(buffer, 0, sizeof(buffer)); // clear buffer
            num_bytes = read(players[p].socket, buffer, BUFFER_SIZE - 1);
            buffer[num_bytes] = '\0';
            packet_type = buffer[0];

            // alias for enemy
            Player enemy;
            if (p == 0) {
                enemy = players[1];
            } else {
                enemy = players[0];
            }

            switch (packet_type) {
                case 'S':
                    int row = -1, column = -1;
                    char result = ' ';

                    // check for errors
                    if (sscanf(buffer, "%c %d %d", &packet_type, &row, &column) != 3) {
                        printf("[Server] E 202\n");
                        send(players[p].socket, "E 202", 5, 0);
                        p--;
                        break;
                    } else if (row < 0 || column < 0 || row >= height || column >= width) {
                        printf("[Server] E 400\n");
                        send(players[p].socket, "E 400", 5, 0);
                        p--; // restart! the increment p++ won't do anything, back to same index
                        break;
                    } 
                    
                    // check spot
                    if (players[p].guessing_board[row][column] == 'H' || players[p].guessing_board[row][column] == 'M') { // already guessed!
                        printf("[Server] E 401\n");
                        send(players[p].socket, "E 401", 5, 0);
                        p--;
                        break;
                    } else if (enemy.ship_board[row][column] != '~') { // ship!!! b/c it's not water
                        result = 'H';
                        players[p].guessing_board[row][column] = 'H';
                        if (is_ship_destroyed(enemy.ship_board, row, column, width, height)) {
                            enemy.ships_remaining--;
                        }
                        enemy.ship_board[row][column] = 'H'; // update it on the enemy's board as well
                    } else { // has to be ~ water so miss
                        result = 'M';
                        players[p].guessing_board[row][column] = 'M';
                        enemy.ship_board[row][column] = 'M';
                    }

                    players[p].num_guesses++;

                    char *shot = build_shot(players[p], result);
                    send(players[p].socket, shot, sizeof(shot), 0);
                    free(shot);
                    
                    break;
                case 'Q':
                    char *query = build_query(players[p], height, width);
                    send(players[p].socket, query, sizeof(query), 0);
                    free(query); // because it's dynamically allocated
                    break;
                case 'F': // forfeit
                    playing = 0; // stop playing
                    break;
                default:
                    printf("[Server] E 102\n");
                    send(players[p].socket, "E 102", 5, 0);
                    p--;
                    break;
            }

            // after each turn -> check game status -> halt packet
            if (enemy.ships_remaining == 0) { // game is over and current player has won
                send(players[p].socket, "H 1", 3, 0);
                send(enemy.socket, "H 0", 3, 0);
                break;
            } else if (playing == 0) { // current player has forfeited and therefore lost
                send(players[p].socket, "H 0", 3, 0);
                send(enemy.socket, "H 1", 3, 0);
                break;
            } // else continue on with the game!
        }
    }

    /* free boards */
    free_board(players[0].ship_board, height);
    free_board(players[0].guessing_board, height);
    free_board(players[1].ship_board, height);
    free_board(players[1].guessing_board, height);

    /* shut down server */
    printf("[Server] Shutting down.\n");
    // close(listen_fd_p1);
    // close(listen_fd_p2);
    close(conn_p1);
    close(conn_p2);
    return EXIT_SUCCESS;
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
        memset(board[i], '~', sizeof(char *));
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
            printf("%d ", board[i][j]);
        }
        printf("\n");
    }
}

/*
    pre-condition: x, y must be anchored beforehand depending on the shape type and rotation
                   assume x, y is correct because it will be parsed in as piece_col, piece_row
    returns 1 if possible, 0 if impossible (out-of-bounds, error 302; ship overlap, error 303)
*/
int can_place_ship(Player player, int piece[4][4], int row, int col, int width, int height) {
    // iterate over each cell in the piece and check if there are any 1's we can offset the x, y
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece[i][j] == 1) {
                // update new board coordinates
                int pos_x = row + i;
                int pos_y = col + j;
                // check if out of bounds
                if (pos_x < 0 || pos_y < 0 || pos_x >= width || pos_y >= height) {
                    printf("[Server] E 302\n");
                    send(player.socket, "E 302", 5, 0);
                    return 0;
                }
                // check if overlapping another ship
                if (player.ship_board[row][col] >= '1' && player.ship_board[row][col] <= '5') {
                    printf("[Server] E 303\n");
                    send(player.socket, "E 303", 5, 0);
                    return 0;
                }
            }
        }
    }
    return 1; // made it through without returning 0 -> it's possible!
}

/*
    pre-condition: x, y must be anchored beforehand depending on the shape type and rotation
                   assuming piece is valid; conditional to catch errors should be done in the main
    updates the actual board
*/
void place_ship(Player *player, int piece[4][4], int row, int col) {
    // iterate over each cell in the piece so that we can update the board accordingly
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece[i][j] == 1) {
                int pos_x = row + i;
                int pos_y = col + j;

                player->ships_remaining++;

                char c = player->ships_remaining + '0';
                player->ship_board[pos_x][pos_y] = c; // insert into board!!
            }
        }
    }
}

void anchor(int piece[4][4], int *row, int *col) {
    if ((piece[0][0] == 0) && (piece[0][1] == 0)) { // pieces[5][0] -> mirrored L needs y+2
        col += 2;
    } else if (piece[0][0] == 0) {
        col += 1;
    }
    // else, leave as is
}

char *build_query(Player player, int height, int width) {
    int size = player.num_guesses * 3;
    char *str = malloc(size);

    snprintf(str, size, "G %d", player.ships_remaining);

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (player.guessing_board[i][j] == 'H' || player.guessing_board[i][j] == 'M') {
                char guess[20];
                snprintf(guess, size, " %c %d %d", player.guessing_board[i][j], j, i);
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

char *build_shot(Player player, char result) {
    int size = 20;
    char *str = malloc(size);

    snprintf(str, size, "R %d %c", player.ships_remaining, result);

    return str;
}

int is_ship_destroyed(char **board, int row, int col, int width, int height) {
    char c = board[row][col];
    for (int i = 0; i < height; i++) { // search the board if there's any of this ship number left
        for (int j = 0; j < width; j++) {
            if (i != row && j != col && board[i][j] == c) { // is this index the only one left?
                return 0; // false, found it
            }
        }
    }
    return 1;
}

char *build_board_str(char **board, int height, int width) {
    char *str = malloc((height * (width * 2)) + height + 1);
    char s = 0; // index for str
    
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            str[s++] = board[i][j];
            if (j == (width - 1)) { // last one in the column
                str[s++] = '\n';
            } else {
                str[s++] = ' ';
            }
        }
    }
    return str;
}

void forfeit(Player *player, Player *enemy) {
    char buffer[BUFFER_SIZE] = {0};
    send(player->socket, "H 0", 3, 0); // current player lost! -> will shut it down

    read(enemy->socket, buffer, BUFFER_SIZE - 1); // force a read from the other player
    send(enemy->socket, "H 1", 3, 0); // doesn't matter what the input is; enemy won! -> shut down

    close(player->socket);
    close(enemy->socket);
}