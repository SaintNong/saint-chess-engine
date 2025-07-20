#include "movepicker.h"

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "eval.h"
#include "search.h"

// Move ordering heuristics
int MvvLva[NB_PIECES][NB_PIECES];

// Fail high history
static Move killerMoves[MAX_SEARCH_DEPTH][2];
static int quietHistory[MAX_SEARCH_DEPTH][64][64];
static Move counterMoves[2][NB_PIECES][64]; // [side][from][to] (of last move)



static int stat_bonus(int depth) {
    // A copy of the stat bonus formula from Ethereal but originally from Stockfish.
    return depth > 13 ? 32 : 16 * depth * depth + 128 * MAX(depth - 1, 0);
}

// Moves are scored on:
    // All:
        // SEE
    // Quiets:
        // Killer Heuristic
        // History Heuristic
        // Counter moves
    // Captures:
        // MVV-LVA
static int scoreMove(Move move, Board *board, MovePicker *picker) {
    // Quiet moves
    if (!IsCapture(move)) {
        if (move == picker->firstKiller)
            return 2000000;
        else if (move == picker->secondKiller)
            return 1990000;
        else if (move == picker->counterMove)
            return 1980000;
        
        // Quiet moves which fail SEE are probably bad and, even worse than bad captures
        // They become worse and worse the more material they hang e.g. a queen move failing SEE would score -505
        if (!SEE(board, move, 0)) return -500 - board->squares[MoveFrom(move)];
        return quietHistory[picker->ply][MoveFrom(move)][MoveTo(move)];
    }
    // Captures sorted by SEE then MVV-LVA
    else {
        if (SEE(board, move, 0))
            // Good captures come before killers and after hash
            // Sorted by MVV-LVA
            return MvvLva[board->squares[MoveTo(move)]][board->squares[MoveFrom(move)]] + 5000000;
        else
            // Bad captures come after killers
            return MvvLva[board->squares[MoveTo(move)]][board->squares[MoveFrom(move)]] - 10000;
    }
}

// Indexed this way:
// MvvLva[victim][attacker];
void initMvvLva() {
    for (int attacker = PAWN; attacker < NB_PIECES; attacker++) {
        for (int victim = PAWN; victim < NB_PIECES; victim++) {
            MvvLva[victim][attacker] =
                    (100 * middleGameMaterial[victim] - middleGameMaterial[attacker]);
        }
    }
}

int getQuietHistory(Move move, int ply) {
    return quietHistory[ply][MoveFrom(move)][MoveTo(move)];
}

void updateCounterMoves(Board *board, Move move) {
    // Get the index which is determined by past board state
    int movedPiece = board->history[board->ply-1].movedPiece;
    int moveDestination = MoveTo(board->history[board->ply-1].move);

    // Update the counter
    counterMoves[board->side][movedPiece][moveDestination] = move;
}

void updateQuietHistory(Move move, int ply, int depth) {
    if (depth < 2) return;

    int *histEntry = &quietHistory[ply][MoveFrom(move)][MoveTo(move)];
    *histEntry += depth * depth;
}

void updateKillerMoves(Move move, int ply) {
    // Don't add to killers if the move is already there
    if (killerMoves[ply][0] != move) {
        killerMoves[ply][1] = killerMoves[ply][0];
        killerMoves[ply][0] = move;
    }
}

void clearKillerMoves() {
    // Called before every search, since killers from the previous search
    // aren't going to be applicable since the ply number for
    // corresponding positions has changed
    for (int ply = 0; ply < MAX_SEARCH_DEPTH; ply++) {
        killerMoves[ply][0] = NO_MOVE;
        killerMoves[ply][1] = NO_MOVE;
    }
}

void clearHistoryHeuristics() {
    for (int ply = 0; ply < MAX_SEARCH_DEPTH; ply++) {
        for (int from = 0; from < 64; from++) {
            for (int to = 0; to < 64; to++) {
                quietHistory[ply][from][to] = 0;
            }
        }
    }
}

// Checks if it's either the first killer at that ply or the second killer
int isKillerMove(Move move, int ply) {
    return (move == killerMoves[ply][0]) || (move == killerMoves[ply][1]);
}

// Scores all the moves in the list
static void scoreMoveList(MoveList *moves, int *scores, Board *board, MovePicker *picker) {
    for (int i = 0; i < moves->count; i++) {
        scores[i] = scoreMove(moves->list[i], board, picker);
    }
}

// Function to heapify a subtree with root at index i
void heapify(int *moveScores, Move *moveList, int n, int i) {
    int largest = i;  // Initialize largest as root
    int left = 2 * i + 1;  // left child
    int right = 2 * i + 2;  // right child

    // If left child is larger than root
    if (left < n && moveScores[left] > moveScores[largest])
        largest = left;

    // If right child is larger than largest so far
    if (right < n && moveScores[right] > moveScores[largest])
        largest = right;

    // If largest is not root
    if (largest != i) {
        // Swap scores
        int tempScore = moveScores[i];
        moveScores[i] = moveScores[largest];
        moveScores[largest] = tempScore;

        // Swap moves
        Move tempMove = moveList[i];
        moveList[i] = moveList[largest];
        moveList[largest] = tempMove;

        // Recursively heapify the affected subtree
        heapify(moveScores, moveList, n, largest);
    }
}

// Function to build a max-heap from the movelist
void buildMaxHeap(int *moveScores, Move *moveList, int n) {
    // Build heap (rearrange array)
    for (int i = n / 2 - 1; i >= 0; i--) {
        heapify(moveScores, moveList, n, i);
    }
}

// Initialises picker on hashMove if it exists, otherwise we start with
// generation
// TODO: Try noisy moves separately (this time without killing my sanity with
// 100 segfaults)
void initMovePicker(MovePicker *picker, Move hashMove, int ply, Board *board) {
    // Assign hashMove
    picker->hashMove = hashMove;
    picker->stage = hashMove == NO_MOVE ? STAGE_GENERATE : STAGE_HASH_MOVE;

    picker->heapBuilt = 0;

    // Assign killers
    picker->firstKiller = killerMoves[ply][0];
    picker->secondKiller = killerMoves[ply][1];

    // Assign counter move
    if (board->ply > 0) {
        // The move we're countering
        Move lastMove = board->history[board->ply-1].move;

        // There is no counter to a null move
        picker->counterMove = lastMove == NO_MOVE ? NO_MOVE
            : counterMoves[!board->side][board->history[board->ply-1].movedPiece][MoveTo(lastMove)];
    } else {
        // If this is the first move there is no counter move
        picker->counterMove = NO_MOVE;
    }
    
    // Assign ply for history ordering
    picker->ply = ply;
}

void initNoisyPicker(MovePicker *picker) {
    // Start with generation since there is no hash
    picker->stage = STAGE_GENERATE;

    // No killers, hashMove or counterMove in quiescence
    picker->hashMove = NO_MOVE;
    picker->firstKiller = NO_MOVE;
    picker->firstKiller = NO_MOVE;
    picker->counterMove = NO_MOVE;

    // Ply is not needed to score noisy moves
    picker->ply = 0;
}

Move pickMove(MovePicker *picker, Board *board, int *moveScore) {
    switch (picker->stage) {
    // Return hash move first if there is one
    // If this causes a cutoff, we skip move generation
    // which saves us a little time
    case STAGE_HASH_MOVE:
        picker->stage = STAGE_GENERATE;
        return picker->hashMove;

    // Generate the moves and score them
    case STAGE_GENERATE:
        generatePseudoLegalMoves(&picker->moveList, board);
        scoreMoveList(&picker->moveList, picker->moveScores, board, picker);

        picker->stage = STAGE_MAIN;

        // We fall through to the next stage after scoring and generation

case STAGE_MAIN:
    SKIP_MOVE:
        // Detect if we're already finished
        if (picker->moveList.count == 0) {
            picker->stage = STAGE_DONE;
            return NO_MOVE;
        }

        // Build the max-heap for the first time
        if (picker->heapBuilt == 0) {
            buildMaxHeap(picker->moveScores, picker->moveList.list, picker->moveList.count);
            picker->heapBuilt = 1; // Mark heap as built
        }

        // Get the best move (the root of the heap, i.e., index 0)
        Move bestMove = picker->moveList.list[0];
        *moveScore = picker->moveScores[0];

        // Replace the root with the last element
        picker->moveList.list[0] = picker->moveList.list[picker->moveList.count - 1];
        picker->moveScores[0] = picker->moveScores[picker->moveList.count - 1];

        // Pop the last element (decrease move list count)
        picker->moveList.count--;

        // Re-heapify the reduced heap
        heapify(picker->moveScores, picker->moveList.list, picker->moveList.count, 0);

        // Check if the move is the hash move, skip it if so
        if (bestMove == picker->hashMove) {
            goto SKIP_MOVE;
        }

        // Return the best move
        return bestMove;

        // Fallback when we run out of moves
        picker->stage = STAGE_DONE;
        return NO_MOVE;

    case STAGE_DONE:
        return NO_MOVE;

    default:
        puts("Something went wrong in the movepicker");
        return NO_MOVE;
    }
}
