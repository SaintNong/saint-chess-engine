#include "eval.h"

#include <stdio.h>
#include <math.h>

#include "board.h"
#include "magicmoves.h"

U64 fileMasks[8];
U64 rankMasks[8];
U64 adjacentFileMasks[8];
U64 passedPawnMasks[2][64];

U64 fillUp(int startRank) {
    U64 mask = 0ULL;
    for (int rank = startRank + 1; rank < 8; rank++)
    {
        mask |= rankMasks[rank];
    }

    return mask;
}

U64 fillDown(int startRank) {
    U64 mask = 0ULL;
    for (int rank = startRank - 1; rank >= 0; rank--)
    {
        mask |= rankMasks[rank];
    }

    return mask;
}


void initPawnMasks() {
    // Clear Masks
    for (int i = 0; i < 8; i++) {
        fileMasks[i] = 0ULL;
        rankMasks[i] = 0ULL;
        adjacentFileMasks[i] = 0ULL;
    }
    for (int i = 0; i < 64; i++) {
        passedPawnMasks[middlegame][i] = 0ULL;
        passedPawnMasks[endgame][i] = 0ULL;
    }

    // File and rank masks
    for (int square = 0; square < 64; square++) {
        int file = fileOf(square);
        int rank = rankOf(square);

        // File
        setBit(&fileMasks[file], square);
        
        // Rank
        setBit(&rankMasks[rank], square);

    }

    // Passed pawn masks
    for (int square = 0; square < 64; square++) {
        int file = fileOf(square);
        int rank = rankOf(square);

        // Mask of the adjacent files
        U64 adjacentMask = fileMasks[file];
        if (file > 1)
            adjacentMask |= fileMasks[file - 1];
        if (file < 7)
            adjacentMask |= fileMasks[file + 1];
        adjacentFileMasks[file] = adjacentMask;
        
        // White
        U64 whiteMask = fillUp(rank) & adjacentMask;
        passedPawnMasks[WHITE][square] = whiteMask;

        // Black
        U64 blackMask = fillDown(rank) & adjacentMask;
        passedPawnMasks[BLACK][square] = blackMask;
    }
}


int getGamePhase(Board *board) {
    // Gets game phase, a value from 0 to 256 representing how
    // close to the endgame we are
    int phase = popCount(board->pieces[KNIGHT]) * KNIGHT_PHASE +
                popCount(board->pieces[BISHOP]) * BISHOP_PHASE +
                popCount(board->pieces[ROOK])   * ROOK_PHASE   +
                popCount(board->pieces[QUEEN])  * QUEEN_PHASE;

    return MIN(phase, START_PHASE);
}

int getTaperedScore(int MGScore, int EGScore, int phase) {
    // Taper a middlegame score and endgame score for the same term
    return ((MGScore * phase) + (EGScore * (START_PHASE - phase))) >> 8; // divide by 256
}

int evaluateMaterialPSQT(Board *board, int phase) {
    // Tapered PSQT evaluation and material evaluation
    int EGScore = 0;
    int MGScore = 0;
    U64 pieces;
    int square;
    int score;

    // // Get black and white king attacks for calculation later
    // U64 kings = board->pieces[KING];
    // U64 blackKingBB = kings & board->colors[BLACK];
    // U64 blackKingAttacks = kingAttacks(getlsb(blackKingBB));
    // blackKingAttacks |= blackKingBB | blackKingAttacks >> 8;

    // U64 whiteKingBB = kings & board->colors[WHITE];
    // U64 whiteKingAttacks = kingAttacks(getlsb(whiteKingBB));
    // whiteKingAttacks |= whiteKingBB | whiteKingAttacks << 8;


    // Go through all the pieces
    for (int piece = PAWN; piece <= KING; piece++) {
        pieces = board->pieces[piece];

        // Loop through the bitboard
        while (pieces) {
            square = poplsb(&pieces);
            if (testBit(board->colors[WHITE], square)) {
                // White piece
                MGScore += middleGamePSQT[piece][MIRROR_SQ(square)];
                MGScore += middleGameMaterial[piece];

                EGScore += endGamePSQT[piece][MIRROR_SQ(square)];
                EGScore += endGameMaterial[piece];

                // // Mobility
                // U64 mobility;
                // int kingAttackCount;
                // switch (piece) {
                //     case KNIGHT:
                //         mobility = knightAttacks(square) & ~board->colors[WHITE];
                //     case BISHOP:
                //         mobility = Bmagic(square, board->colors[BOTH]) & ~board->colors[WHITE];
                //         break;
                //     case ROOK:
                //         mobility = Rmagic(square, board->colors[BOTH]) & ~board->colors[WHITE];
                //         break;
                //     case QUEEN:
                //         mobility = Qmagic(square, board->colors[BOTH]) & ~board->colors[WHITE];
                //         break;
                //     case KING:
                //         mobility = Qmagic(square, board->colors[BOTH]) & ~board->colors[WHITE];
                //         break;
                //     default:
                //         mobility = 0;
                //         break;
                // }
                // // Mobility
                // MGScore += mobilityBonus[middlegame][piece] * popCount(mobility);
                // EGScore += mobilityBonus[endgame][piece] * popCount(mobility);

                // // King attacks
                // kingAttackCount = popCount(mobility & blackKingAttacks);
                // MGScore += kingAttackBonus[middlegame][piece] * kingAttackCount;
                // EGScore += kingAttackBonus[endgame][piece] * kingAttackCount;
            } else {
                // Black piece
                MGScore -= middleGamePSQT[piece][square];
                MGScore -= middleGameMaterial[piece];

                EGScore -= endGamePSQT[piece][square];
                EGScore -= endGameMaterial[piece];

                // // Mobility
                // U64 mobility;
                // int kingAttackCount;
                // switch (piece) {
                //     case KNIGHT:
                //         mobility = knightAttacks(square) & ~board->colors[BLACK];
                //         break;
                //     case BISHOP:
                //         mobility = Bmagic(square, board->colors[BOTH]) & ~board->colors[BLACK];
                //         break;
                //     case ROOK:
                //         mobility = Rmagic(square, board->colors[BOTH]) & ~board->colors[BLACK];
                //         break;
                //     case QUEEN:
                //         mobility = Qmagic(square, board->colors[BOTH]) & ~board->colors[BLACK];
                //         break;
                //     case KING:
                //         mobility = Qmagic(square, board->colors[BOTH]) & ~board->colors[BLACK];
                //         break;
                //     default:
                //         mobility = 0;
                //         break;
                // }
                // // Mobility
                // MGScore -= mobilityBonus[middlegame][piece] * popCount(mobility);
                // EGScore -= mobilityBonus[endgame][piece] * popCount(mobility);

                // // King attacks
                // kingAttackCount = popCount(mobility & whiteKingAttacks);
                // MGScore -= kingAttackBonus[middlegame][piece] * kingAttackCount;
                // EGScore -= kingAttackBonus[endgame][piece] * kingAttackCount;
            }
        }
    }
    // Tapered eval
    // Credit: MadChess (Erik Madsen) for phase values

    score = getTaperedScore(MGScore, EGScore, phase);
    // printf("MGScore: %d\n", MGScore);
    // printf("EGScore: %d\n", EGScore);
    // printf("Phase: %d\n", phase);
    // printf("score: %d\n", score);


    // In negamax, we return evaluations relative to the side to move.
    // Since this function is from white's perspective, we must negate score
    // if we're playing from black's POV.
    if (board->side == WHITE)
        return score;
    return -score;
}

int evaluateImbalances(Board *board) {
    int score;
    int us = board->side;
    int them = !board->side;

    // Bishop pair bonus
    U64 bishops = board->pieces[BISHOP];
    if (popCount(bishops & board->colors[us]) > 1)
        score += BISHOP_PAIR_BONUS;
    if (popCount(bishops & board->colors[them]) > 1)
        score -= BISHOP_PAIR_BONUS;



    return score;
}

int evaluateKings(Board *board, int phase, int side) {
    // King safety evaluation
    // Inspired by CPW
    int MGScore = 0;

    // King castling bonus
    if (side == WHITE && (board->castlePerm & CASTLE_WK || board->castlePerm & CASTLE_WQ))
        MGScore += CASTLE_BONUS;
    else if (side == BLACK && (board->castlePerm & CASTLE_BK || board->castlePerm & CASTLE_BQ))
        MGScore += CASTLE_BONUS;

    // Taper the eval since King safety is not applicable in endgame
    int score = getTaperedScore(MGScore, 0, phase);


    return score;
}

int evaluatePawns(Board *board, int phase, int side) {
    // Evaluates pawn structure
    int score;
    int MGScore;
    int EGScore;
    int square;
    int rank, file;

    // Loop through all our pawns
    U64 ourPawns = board->pieces[PAWN] & board->colors[side];
    U64 ourPawnsSaved = ourPawns;
    U64 enemyPawns = board->pieces[PAWN] & board->colors[!side];
    while (ourPawns) {
        square = poplsb(&ourPawns);
        file = fileOf(square);
        rank = rankOf(square);
        if (side == BLACK) rank = 7 - rank;

        // Passed pawn
        if ((passedPawnMasks[side][square] & enemyPawns) == 0) {
            MGScore += passedPawnBonus[middlegame][rank];
            EGScore += passedPawnBonus[endgame][rank];
        }

        // Isolated pawn
        if (adjacentFileMasks[file] & ourPawnsSaved & ~fileMasks[file] == 0ULL) {
            MGScore -= ISOLATED_PAWN_PENALTY;
            EGScore -= ISOLATED_PAWN_PENALTY;
        }

        // Doubled pawn
        if (popCount(fileMasks[file] & ourPawnsSaved) > 1) {
            MGScore -= DOUBLED_PAWN_PENALTY_MG;
            EGScore -= DOUBLED_PAWN_PENALTY_EG;
        }
    }

    score = getTaperedScore(MGScore, EGScore, phase);

    return score;
}

// Calculates the evaluation of the board from the side to move's perspective
int evaluate(Board *board) {
    int score;
    int phase = getGamePhase(board);
    score += evaluateMaterialPSQT(board, phase);
    score += evaluateImbalances(board);

    // score += evaluateKings(board, phase, board->side) - evaluateKings(board, phase, !board->side);
    score += evaluatePawns(board, phase, board->side) - evaluatePawns(board, phase, !board->side);



    /*
    If it's our turn to move, it's likely we are able to find a move which
    improves the evaluation thus causing an odd-even fluctuation in the
    evaluation. To mitigate this, I chose to add a small bonus to the side to move
    (which is us). Of course, this proposition is not valid if we're in zugzwang,
    however that is rare and can be counteracted if we just search deeper
    */
   score += STM_BONUS;

    return score;
}
