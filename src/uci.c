#include "uci.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench.h"
#include "bitboards.h"
#include "board.h"
#include "engine.h"
#include "magicmoves.h"
#include "makemove.h"
#include "move.h"
#include "movegen.h"
#include "movepicker.h"
#include "search.h"
#include "timeman.h"
#include "zobrist.h"

Move stringToMove(const char *string, Board *board) {
    // Get from and to squares from string
    int from = stringToSquare(string);
    int to = stringToSquare(string + 2);

    int pieceMoved = board->squares[from];
    int pieceCaptured = board->squares[to];
    int flag = QUIET_FLAG;

    // Promotion
    if (string[4] == 'q')
        flag |= QUEEN_PROMO_FLAG;
    else if (string[4] == 'r')
        flag |= ROOK_PROMO_FLAG;
    else if (string[4] == 'n')
        flag |= KNIGHT_PROMO_FLAG;
    else if (string[4] == 'b')
        flag |= BISHOP_PROMO_FLAG;
    // Capture
    else if (pieceCaptured != EMPTY)
        flag |= CAPTURE_FLAG;

    // Castling
    if (pieceMoved == KING) {
        if (from == E1) {
            if (to == G1)
                flag = CASTLE_FLAG;
            else if (to == C1)
                flag = CASTLE_FLAG;
        } else if (from == E8) {
            if (to == G8)
                flag = CASTLE_FLAG;
            else if (to == C8)
                flag = CASTLE_FLAG;
        }
    }

    // En passant
    if (pieceMoved == PAWN && string[0] != string[2] && pieceCaptured == EMPTY)
        flag = EP_FLAG;

    return ConstructMove(from, to, flag);
}

void uciPosition(Board *board, char *input) {
    // Set the position to either START_FEN or a provided FEN
    // Then plays the further moves specified on that position
    char *token = strtok(input, " ");

    if (strcmp(token, "position") == 0) {
        token = strtok(NULL, " ");
    }

    if (strcmp(token, "startpos") == 0) {
        parseFen(board, START_FEN);
        token = strtok(NULL, " "); // to 'moves' or NULL
    } else if (strcmp(token, "fen") == 0) {
        char fen[100];
        fen[0] = '\0';
        token = strtok(NULL, " "); // to actual FEN part
        while (token != NULL && strcmp(token, "moves") != 0) {
            strcat(fen, token);
            strcat(fen, " ");
            token = strtok(NULL, " ");
        }
        parseFen(board, fen);
    }

    // Parse moves
    if (token != NULL && strcmp(token, "moves") == 0) {
        token = strtok(NULL, " ");
        while (token != NULL) {
            if (makeMove(board, stringToMove(token, board)) == 0) {
                // Move was illegal, or my code was wrong..
                printf("Move parsing error at: %s\n", token);
                exit(1);
            }
            // Advance to next move
            token = strtok(NULL, " ");
        }
    }
}

void uciGo(Board *board, char *input) {
    // Sets up a search after parsing a 'go' command

    int depth = MAX_SEARCH_DEPTH - 1, movestogo = 30, movetime = -1;
    int time = -1, inc = 0;
    char *ptr = NULL;

    if ((ptr = strstr(input, "infinite")))
        ;

    if ((ptr = strstr(input, "binc")) && board->side == BLACK) {
        inc = atoi(ptr + 5);
    }

    if ((ptr = strstr(input, "winc")) && board->side == WHITE) {
        inc = atoi(ptr + 5);
    }

    if ((ptr = strstr(input, "wtime")) && board->side == WHITE) {
        time = atoi(ptr + 6);
    }

    if ((ptr = strstr(input, "btime")) && board->side == BLACK) {
        time = atoi(ptr + 6);
    }

    if ((ptr = strstr(input, "movestogo"))) {
        movestogo = atoi(ptr + 10);
    }

    if ((ptr = strstr(input, "movetime"))) {
        movetime = atoi(ptr + 9);
    }

    if ((ptr = strstr(input, "depth"))) {
        depth = atoi(ptr + 6);
    }

    SearchInfo info;
    info.startTime = getTime();
    info.endTime = info.startTime + timeToThink(time, inc, movestogo, movetime);
    info.depthToSearch = depth;
    info.fh = 0;
    info.fhf = 0;
    info.nodes = 0;

    info.quit = false;
    info.stopped = false;
    if (time == -1)
        info.timeSet = false;
    else
        info.timeSet = true;

    puts("Starting Search:");
    if (info.timeSet) {
        printf("Time allocated for this move: %dms\n",
           info.endTime - info.startTime);
    }

    // Start iterative deepening
    beginSearch(board, &info);
}

// Begins perft from current position at specified depth
void uciPerft(Board *board, char *input) {
    int depth = 0;
    char *token = strtok(input, " ");
    token = strtok(NULL, " ");

    if (token != NULL) {
        depth = atoi(token);
        if (depth > 0) {
            bench(board, depth);
        } else {
            puts("Error: Invalid depth for perft");
            puts("Now I'm gonna crash just to spite you.");
            exit(1);
        }
    } else {
        puts("Usage: perft [depth]");
    }
}

void uciLoop(Board *board) {
    /*
    Universal Chess Interface (UCI)

    UCI Commands implemented
        - uci => Prints some info about the engine and uciok
        - isready => Prints readyok and initialises engine internal state
        - ucinewgame => Resets board to initial state
        - position [fen | startpos] moves ... => Sets the position
        - go => Searches position (WIP)

    Custom commands
        - print => prints an ascii representation of the board to the terminal
        - perft [depth] => does a perft of that depth from the current board state
                           and benchmarks the speed
    */

    char input[4000];

    // Magic words that somehow fix everything??
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    parseFen(board, START_FEN);

    while (true) {
        memset(input, 0, sizeof(input));
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin))
            continue;

        // Strip newline character
        input[strcspn(input, "\n")] = 0;


        /* UCI commands */
        if (strcmp(input, "uci") == 0) {
            printf("id name %s %s\n", NAME, VERSION);
            printf("id author %s\n", AUTHOR);
            puts("uciok");

        } else if (strcmp(input, "isready") == 0) {
            puts("readyok");

        } else if (strcmp(input, "ucinewgame") == 0) {
            parseFen(board, START_FEN);

        } else if (strncmp(input, "position", 8) == 0) {
            uciPosition(board, input);

        } else if (strncmp(input, "go", 2) == 0) {
            uciGo(board, input);

        } else if (strcmp(input, "quit") == 0) {
            break;
        }

        /* Custom commands */
        else if (strncmp(input, "perft", 5) == 0) {
            uciPerft(board, input);
        } else if (strcmp(input, "print") == 0) {
            printBoard(board);
        }

        /* No commands hit, so unknown */
        else {
            printf("Unknown command: '%s'\n", input);
        }
    }
}
