#pragma once

#include "board.h"
#include "move.h"
#include "movegen.h"

enum { STAGE_HASH_MOVE, STAGE_GENERATE, STAGE_MAIN, STAGE_DONE };

typedef struct {
    MoveList moveList;
    int moveScores[MAX_LEGAL_MOVES];
    Move hashMove, firstKiller, secondKiller, counterMove;
    int stage;
    int ply;
    int heapBuilt;
} MovePicker;

#define HISTORY_DIVISOR 16384

// Move ordering heuristics
int getQuietHistory(Move move, int ply);
void updateQuietHistory(Move move, int ply, int depth);
void updateKillerMoves(Move move, int ply);
void updateCounterMoves(Board *board, Move move);
int isKillerMove(Move move, int ply);

void clearKillerMoves();
void clearHistoryHeuristics();

// Move picker
void initMovePicker(MovePicker *picker, Move hashMove, int ply, Board *board);
void initNoisyPicker(MovePicker *picker);
Move pickMove(MovePicker *picker, Board *board, int *moveScore);
void initMvvLva();
