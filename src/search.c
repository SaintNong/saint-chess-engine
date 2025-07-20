#include "search.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "bitboards.h"
#include "board.h"
#include "eval.h"
#include "hashtable.h"
#include "makemove.h"
#include "move.h"
#include "movegen.h"
#include "movepicker.h"
#include "timeman.h"

// Global variables :skull:
SearchInfo searchInfo;
int LMRDepths[MAX_SEARCH_DEPTH][MAX_LEGAL_MOVES];

// Precalculates the LMR depth table
// This is used later in search to decide how much to reduce by
// Credit: Ethereal and Weiss and basically every engine in the top 10
void initLMRDepths() {
    for (int depth = 0; depth < MAX_SEARCH_DEPTH; depth++) {
        for (int moveIndex = 0; moveIndex < MAX_LEGAL_MOVES; moveIndex++) {
            // Arbitrary values I chose by logging them and seeing what looks right
            int reduction = log(depth) * log(moveIndex) / 4;
            if (reduction < 0)
                reduction = 0;
            LMRDepths[depth][moveIndex] = reduction;

            // if (depth <= 18 && moveIndex <= 25)
            //     printf("D: %d MC: %d R: %d\n", depth, moveIndex, reduction);
        }
    }
}

// Stolen from VICE becauase I don't know how to write IO code in C (yet)
static inline void readInput() {
    int bytes;
    char input[256] = "";
    char *endc;

    if (inputWaiting()) {
        searchInfo.stopped = true;
        do {
            bytes = read(fileno(stdin), input, 256);
        } while (bytes < 0);
        endc = strchr(input, '\n');
        if (endc)
            *endc = 0;

        if (strlen(input) > 0) {
            if (!strncmp(input, "quit", 4)) {
                searchInfo.quit = true;
            }
        }
        return;
    }
}

static inline void checkTimeUp() {
    if (getTime() > searchInfo.endTime && searchInfo.timeSet) {
        searchInfo.stopped = true;
    }

    readInput(&searchInfo);
}

static int isRepetition(Board *board) {
    int ply = board->ply;
    int fiftyMove = board->fiftyMove;

    /*
    To detect repetitions, we go through all the board hashes in history, looking
    for a duplicate We start from the last capture/pawn move since those can't be
    undone
    */
    for (int i = ply - fiftyMove; i < ply - 1; i++) {
        if (board->hash == board->history[i].hash) {
            return 1;
        }
    }

    return 0;
}

int moveBestCaseScore(Move move, Board *board) {
    if (board->squares[MoveTo(move)] != EMPTY)
        return middleGameMaterial[board->squares[MoveTo(move)]];
    else
        return 0;
}

static int quiesce(Board *board, int alpha, int beta) {
    searchInfo.nodes++;

    /*
    During quiescence, we actually have the choice not to play a move at all when
    considering moves. This "stand pat" score is considered first here if it
    causes a cutoff
    */
    int evaluation = evaluate(board);
    if (evaluation >= beta)
        return beta;

    // Delta pruning
    // In nodes that are so bad that even if we win a FULL queen and still don't
    // improve alpha, we might as well just not search it. It's highly unlikely
    // anything here was good anyways.
    const int largeMaterialSwing = middleGameMaterial[QUEEN];
    if (evaluation + largeMaterialSwing < alpha)
        return evaluation;

    alpha = MAX(evaluation, alpha);

    // The search has stopped, we must leave
    if (searchInfo.stopped == true)
        return 0;

    // Start searching
    int bestScore = evaluation;
    int score;
    Move bestMove = NO_MOVE;
    int moveScore;

    MovePicker picker;
    initNoisyPicker(&picker);

    Move move;
    while ((move = pickMove(&picker, board, &moveScore)) != NO_MOVE) {

        // After a non-capture, there are no more noisy moves to check due to move sorting so we can break.
        // If the move score is less than zero, that signifies the rest of the moves failed SEE so we can
        // safely break then too.
        if (!IsCapture(move) || moveScore < 0)
            break;

        // Delta pruning
        // If a move is so bad that even if the value of the piece captured and
        // relatively large margin is not enough to raise alpha, we can prune it
        if ((evaluation + moveBestCaseScore(move, board) + DELTA_PRUNING_MARGIN) < alpha)
            continue;


        // Skip illegals
        if (makeMove(board, move) == 0) {
            undoMove(board, move);
            continue;
        }

        // Next iteration
        score = -quiesce(board, -beta, -alpha);
        undoMove(board, move);

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;

            if (score > alpha) {
                alpha = score;

                // Move failed high, opponent will avoid it
                if (alpha >= beta) {
                    break;
                }
            }
        }
    }

    return bestScore;
}

// Principal variation search
static int search(Board *board, int alpha, int beta, int depth, PV *pv, int ply, int pvNode, int doNull) {
    // This is a PV node if we're not doing a Null Window search
    // int pvNode = (beta - alpha > 1);

    // This is a root node if the ply is 0
    bool rootNode = (ply == 0);

    PV childPV;
    childPV.count = 0;

    // Check extension before quiescence
    int inCheck = isSquareAttacked(
            board, board->side,
            getlsb(board->pieces[KING] & board->colors[board->side]));
    if (inCheck)
        depth++;

    // Drop to quiescence when depth runs out
    if (depth <= 0) {
        childPV.count = 0;
        return quiesce(board, alpha, beta);
    }

    /*
    Hash return conditions:
    1. This is not a root node.
    2. This is not a PV node.
    3. Entry depth is >= our current.
    4. One of the following:
            a. The bound is exact or
            b. The bound is an upper bound but we scored below it or
            c. The bound is an lower bound but we scored above it
    */
    Move hashMove = NO_MOVE;
    int hashDepth, hashScore, hashFlag;
    // 1. Root node
    if (!rootNode) {
        searchInfo.hashAttempt++;
        if (hashTableProbe(board->hash, &hashMove, &hashDepth, &hashScore, &hashFlag) == PROBE_SUCCESS) {
            // 2 + 3. Not PV node and enough depth
            if (!pvNode && hashDepth >= depth) {
                // 4. Exact or produces a cutoff
                if (hashFlag == BOUND_EXACT ||
                        (hashFlag == BOUND_LOWER && hashScore >= beta) ||
                        (hashFlag == BOUND_UPPER && hashScore <= alpha)) {
                    searchInfo.hashHit++;
                    return hashScore;
                }
            }
        }
    }

    searchInfo.nodes++;

    // The search has stopped, we must leave
    if (searchInfo.stopped == true)
        return 0;

    if (!rootNode) {
        // Draw detection (not working for some reason lol)
        if (isDraw(board)) {
            return 0;
        }

        if (ply >= MAX_SEARCH_DEPTH - 1)
            return evaluate(board);

        // Mate distance pruning
        alpha = MAX(alpha, -MATE + ply);
        beta = MIN(beta, MATE - ply - 1);
        if (alpha >= beta) {
            return alpha;
        }
    }

    if ((searchInfo.nodes & 4095) == 0) {
        checkTimeUp();
    }

    // Evaluation used for pruning later
    int eval = evaluate(board);

    // Internal iterative deepening
    // If we don't have a hash move in a PV node, we do a tiny search and then
    // probe the hash table again to greatly improve our move ordering for this node
    // Speeds up search in programs with bad move ordering (like this one)
    if (pvNode && depth >= 8 && hashMove == NO_MOVE) {
        -search(board, alpha, beta, depth - 7, &childPV, ply + 1, IS_PV, doNull);
        hashTableProbe(board->hash, &hashMove, &hashDepth, &hashScore, &hashFlag);
    }

    // Adaptive null move pruning
    if (!pvNode && !inCheck && eval >= beta && !isPawnEndgame(board, board->side) && depth >= 4 && doNull) {


        int reduction = 4;

        makeNullMove(board);
        int score = -search(board, -alpha - 1, -alpha, depth - reduction, &childPV, ply + 1, NOT_PV, false);
        undoNullMove(board);

        if (score >= beta)
            return beta;
    }

    // Now we start searching
    int bestScore = -MATE + ply;
    Move bestMove = NO_MOVE;

    // Score of current move
    int score;

    // Used at the end for TT storing to see if alpha was raised
    int hashBound = BOUND_UPPER;
    int movesPlayed = 0;
    int moveScore;
    int moveIsQuiet;
    int skipQuiets = false;

    // Start going through the moves in the position
    MovePicker picker;
    initMovePicker(&picker, hashMove, ply, board);

    Move move;
    while ((move = pickMove(&picker, board, &moveScore)) != NO_MOVE) {
        moveIsQuiet = !IsCapture(move) && !IsPromotion(move);

        // Skip quiets if the flag is checked
        if (skipQuiets && moveIsQuiet)
            continue;

        // // SEE Pruning
        // if (depth <= 7
        //     && !SEE(board, move, -150 * depth))
        //     continue;

        // Check move legality
        if (makeMove(board, move) == 0) {
            undoMove(board, move);
            continue;
        }
        // Update count of legal moves played
        movesPlayed++;

            


        /*
        Principal Variation Search
        Principal variation search works by doing null window searches on moves that
        aren't important. The only moves which need a full window are PV moves in PV
        nodes, and the rest can be searched with a null window to save time. Of
        course, if one of these null windows fail high, the PV move was not the best
        move, and this new PV move will have to be searched at a full window. Since
        we have hash table ordering, the first move of a PV node should be the best
        most of the time, and thus this method speeds up the search by a fair
        amount.
        */

        // Do a full window search if its the first move played
        // In a cut node, this "full" window will still be a null window
        // In a PV node, this full window will actually be full
        if (movesPlayed == 1 && pvNode) {
            // Inherits the node type
            score = -search(board, -beta, -alpha, depth - 1, &childPV, ply + 1, pvNode, doNull);
        }
        // Prune the frick out of the rest of the moves because they're probably not
        // good
        else {
            /*
            Late move reductions:
            Going off the assmption that our move ordering is quite good, the best
            moves should be near the front of the list, and the bad ones at the back.
            Late move reductions aim to search those bad moves at the end of the list
            with a reduced depth (depending on which depth we're on and how far back
            in the move list it was). This saves us a lot of time, and by a lot I mean
            a LOT (2+ extra plies in the same time!). However, this may mean that some
            quiet move tactics which are harder to find will be missed by our engine,
            but it's worth the sacrifice. Of course, when ever one of these searches
            fail high, we must do a full depth null window search.
            */
            int reduction = 0;
            if (!inCheck && depth > 2 && moveIsQuiet && !isKillerMove(move, ply)) {
                reduction = LMRDepths[depth][movesPlayed];



                if (reduction < 0) reduction = 0;
            }


            // Null window search with late move reduction depth
            score = -search(board, -alpha - 1, -alpha, depth - 1 - reduction, &childPV, ply + 1, NOT_PV, doNull);
            // Failed high so null window search with full depth
            if (reduction > 0 && score > alpha)
                score = -search(board, -alpha - 1, -alpha, depth - 1, &childPV, ply + 1, NOT_PV, doNull);

            // Failed high, must be new pv
            // Re-search with full window
            if (score > alpha && score < beta)
                score = -search(board, -beta, -alpha, depth - 1, &childPV, ply + 1, IS_PV, doNull);
        }

        // Undo the move
        undoMove(board, move);

        // The search has stopped, we must leave
        if (searchInfo.stopped == true)
            return 0;

        // New best move was found!
        if (score > bestScore) {
            bestScore = score;

            // If the score beta alpha
            // alpha must be updated
            if (score > alpha) {
                alpha = score;
                bestMove = move;
                hashBound = BOUND_EXACT;

                // If alpha was beat in a PV node a new PV was found
                if (pvNode) {
                    pv->moves[0] = move;
                    memcpy(pv->moves + 1, childPV.moves, childPV.count * sizeof(Move));
                    pv->count = childPV.count + 1;
                }

                // Fail high cut-off
                // The move was too good, the opponent will avoid it!
                if (alpha >= beta) {
                    // The bound for this score is lower
                    hashBound = BOUND_LOWER;

                    // Move ordering heuristics from VICE
                    if (movesPlayed == 1)
                        searchInfo.fhf++;
                    searchInfo.fh++;

                    // Quiet move heuristics
                    if (!IsCapture(move)) {
                        // Killers
                        updateKillerMoves(move, ply);

                        // Counter moves
                        updateCounterMoves(board, move);

                        // History
                        updateQuietHistory(move, ply, depth);
                    }

                    break;
                }
            }
        }
    }

    // Fail low history detection
    // We keep track of nodes in which 

    // If no moves were made, this is either checkmate or stalemate
    if (movesPlayed == 0) {
        if (inCheck)
            return -MATE + ply;
        else
            return 0;
    }

    // The search has stopped, we must leave
    if (searchInfo.stopped == true)
        return 0;

    // Store the results of this search in the hash table
    hashTableStore(board->hash, bestMove, depth, bestScore, hashBound);

    return bestScore;
}

// My cursed implementation of aspiration windows
// https://www.chessprogramming.org/Aspiration_Windows
int aspirationWindow(Board *board, int score, int depth, PV *pv) {
    int alpha, beta;

    // Start window at the smallest size
    int alphaIndex = 0, betaIndex = 0;

    // Repeat until the search doesn't fall out of window
    while (score <= alpha || score >= beta) {
        alpha = score - ASPIRATION_SIZES[alphaIndex];
        beta = score + ASPIRATION_SIZES[betaIndex];

        // Search with current window
        score = search(board, alpha, beta, depth, pv, 0, IS_PV, true);

        // Success
        if (score > alpha && score < beta)
            break;

        // Update windows
        if (score <= alpha && alphaIndex < ASPIRATION_MAX)
            alphaIndex++;
        if (score >= beta && betaIndex < ASPIRATION_MAX)
            betaIndex++;
    }
    // printf("Alpha: %d\n", alphaIndex);
    // printf("Beta: %d\n", betaIndex);

    return score;
}

// Thank you VICE
void clearForSearch() {
    // Search debugging statistics
    searchInfo.fh = 0;
    searchInfo.fhf = 0;
    searchInfo.hashAttempt = 0;
    searchInfo.hashHit = 0;

    // Clear search heuristics
    clearKillerMoves();
    clearHistoryHeuristics();
    

    // Update hash ages
    updateHashAge();
}

// Iterative deepening with aspiration windows
void beginSearch(Board *board, SearchInfo *info) {
    clearForSearch();

    searchInfo = *info;

    int depthToSearch = searchInfo.depthToSearch;

    Move bestMove = NO_MOVE;
    PV pv;
    int score;
    int timesFoundMate = 0;

    // Begin iteratively deepening
    for (int currentDepth = 1; currentDepth <= depthToSearch; currentDepth++) {
        // At the first few depths we use a normal full window search, then
        // the score is decently stable and we can use aspiration windows on deeper
        // depths for faster searching
        // if (currentDepth < 5)
            score = search(board, -INF, INF, currentDepth, &pv, 0, IS_PV, true);
        // else
        //     score = aspirationWindow(board, score, currentDepth, &pv);

        // Exit iterative deepening loop if we have run out of time or the user has quit
        if (searchInfo.stopped || searchInfo.quit)
            break;

        // Retrieve PV
        bestMove = pv.moves[0];

        int timeElapsed = getTime() - searchInfo.startTime + 1;

        // UCI printing
        if (abs(score) > MATE - 100) {
            int pliesToMate = (MATE - abs(score));
            int mateInMoves = (pliesToMate + 1) / 2;

            printf("info depth %d score mate %d nodes %li time %d pv", currentDepth,
                                                                       score > 0 ? mateInMoves : -mateInMoves,
                                                                       searchInfo.nodes,
                                                                       timeElapsed);

            // timesFoundMate++;
            // if (timesFoundMate > 4) searchInfo.stopped = true;
        } else {
            printf("info depth %d score cp %d nodes %li time %d pv", currentDepth,
                                                                     score,
                                                                     searchInfo.nodes,
                                                                     timeElapsed);
        }

        // Print pv uci
        for (int i = 0; i < pv.count; i++) {
            printf(" ");
            printMove(pv.moves[i], 0);
        }
        printf("\n");
    }

    if (!searchInfo.quit) {
        printf("bestmove ");
        printMove(bestMove, 1);

        // Debug statistics
        printf("Ordering: %.2f %%\n", (searchInfo.fhf / searchInfo.fh) * 100);
        printf("Hash Table hit rate: %.2f %%\n",
           (searchInfo.hashHit / searchInfo.hashAttempt) * 100);

        double occupied = occupiedHashEntries();
        printf("Hash Table occupancy: %.2f %%\n", (occupied) * 100);

    } else {
        // Free hash table then exit
        freeHashTable();
        exit(0);
    }
}
