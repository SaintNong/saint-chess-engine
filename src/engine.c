#include "engine.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench.h"
#include "bitboards.h"
#include "board.h"
#include "eval.h"
#include "hashtable.h"
#include "magicmoves.h"
#include "makemove.h"
#include "move.h"
#include "movegen.h"
#include "movepicker.h"
#include "search.h"
#include "timeman.h"
#include "uci.h"
#include "zobrist.h"

void welcomeMessage() {
    printf("%s by %s\n", NAME, AUTHOR);
    puts("<3");
    puts("please dont segfault uwu");
}

void initialise() {
    initAttackMasks();
    initZobristKeys();
    initMvvLva();
    initLMRDepths();
    initDistances();
    initPawnMasks();

    // Default hash is 256 MB
    // TODO: Set custom hash size from UCI
    initHashTable(256);

    // Credit to Pradu Kannan for excellent magic bitboard implementation
    initmagicmoves();
}

int main() {
    welcomeMessage();
    initialise();

    // Debug flag which decides whether to run the UCI loop or debug code
    bool DEBUG = false;

    // Create the board which will be used thoughout the whole program
    Board board;

    if (!DEBUG) {
        uciLoop(&board);

    } else {
        // debug code here
        parseFen(&board, "8/r3k3/3ppp2/p7/3PPP2/5P2/4K3/3R4 w - - 0 1");
        printBoard(&board);
        printf("Eval: %d\n", evaluate(&board));

    }

    // Free hash table before leaving
    freeHashTable();
    return 0;
}
