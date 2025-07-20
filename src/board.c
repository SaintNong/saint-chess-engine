#include "board.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "magicmoves.h"
#include "zobrist.h"

int SquareDistances[64][64];
void initDistances() {
    for (int from = 0; from < 64; from++) {
        for (int to = 0; to < 64; to++) {
            int horizontalDistance = abs(fileOf(from) - fileOf(2));
            int verticalDistance = abs(rankOf(from) - rankOf(2));
            SquareDistances[from][to] = MAX(horizontalDistance, verticalDistance);
        }
    }
}

int squareDistance(int from, int to) { return SquareDistances[from][to]; }

// Square helper functions
int squareFrom(int file, int rank) { return rank * 8 + file; }

int rankOf(int sq) { return sq / 8; }

int fileOf(int sq) { return sq % 8; }

bool fileRankInBoard(int file, int rank) {
    if (file < 0 || file >= 8) {
        return false;
    }
    if (rank < 0 || rank >= 8) {
        return false;
    }
    return true;
}

// Clears the board to an empty state
void clearBoard(Board *board) {
    // Clear board pieces
    memset(board->colors, 0ULL, sizeof(board->colors));
    memset(board->pieces, 0ULL, sizeof(board->pieces));

    for (int sq = 0; sq < 64; sq++) {
        board->squares[sq] = EMPTY;
    }

    // Clear board variables
    board->side = BOTH;
    board->hash = 0ULL;
    board->epSquare = NO_SQ;
    board->fiftyMove = 0;
    board->castlePerm = 0;
    board->ply = 0;

    // Clear history
    for (int i = 0; i < MAX_MOVES; i++) {
        Undo undo = board->history[i];
        undo.hash = 0ULL;
        undo.castlePerm = 0;
        undo.epSquare = NO_SQ;
        undo.fiftyMove = 0;
        undo.movedPiece = NO_PIECE;
        undo.capturedPiece = NO_PIECE;
    }
}

// Sets a piece on the board at the square
void setPiece(Board *board, int color, int piece, int sq) {
    assert(board->squares[sq] == EMPTY);      // Square is empty
    assert(piece >= PAWN && piece <= KING);   // Valid piece
    assert(color == WHITE || color == BLACK); // Valid color
    assert(sq >= A1 && sq <= H8);             // Valid square

    // Set piece on pieces bitboard, colors[color] and colors[BOTH]
    setBit(&board->pieces[piece], sq);
    setBit(&board->colors[color], sq);
    setBit(&board->colors[BOTH], sq);

    // Set piece on mailbox board
    board->squares[sq] = piece;

    // Update board hash
    board->hash ^= PieceKeys[toPiece(piece, color)][sq];
}

// Clears the piece from the board on the square specified
void clearPiece(Board *board, int color, int sq) {
    int piece = board->squares[sq];

    assert(piece >= PAWN && piece <= KING);   // Valid piece
    assert(color == WHITE || color == BLACK); // Valid color
    assert(sq >= A1 && sq <= H8);             // Valid square

    // Clear piece on mailbox board
    board->squares[sq] = EMPTY;

    // Clear piece from bitboards
    clearBit(&board->pieces[piece], sq);
    clearBit(&board->colors[color], sq);
    clearBit(&board->colors[BOTH], sq);

    // Update board hash
    board->hash ^= PieceKeys[toPiece(piece, color)][sq];
}

// Moves piece from one square to on board
void movePiece(Board *board, int from, int to, int color) {
    // Clear the from square
    int piece = board->squares[from];

    assert(board->squares[to] == EMPTY);      // to sq is empty
    assert(piece >= PAWN && piece <= KING);   // Valid piece
    assert(color == WHITE || color == BLACK); // Valid color
    assert(from >= A1 && from <= H8);         // Valid from square
    assert(to >= A1 && to <= H8);             // Valid to square

    board->squares[from] = EMPTY;

    // Clear piece from bitboards
    clearBit(&board->pieces[piece], from);
    clearBit(&board->colors[color], from);
    clearBit(&board->colors[BOTH], from);

    // Set piece on pieces bitboard, colors[color] and colors[BOTH]
    setBit(&board->pieces[piece], to);
    setBit(&board->colors[color], to);
    setBit(&board->colors[BOTH], to);

    // Set on mailbox board
    board->squares[to] = piece;

    // Update board hash
    board->hash ^= PieceKeys[toPiece(piece, color)][from];
    board->hash ^= PieceKeys[toPiece(piece, color)][to];
}

// If we're in a pawn endgame we don't try null move
int isPawnEndgame(Board *board, int side) {
    U64 kings = board->pieces[KING];
    U64 pawns = board->pieces[PAWN];
    U64 ourPieces = board->colors[side];

    // If kings | pawns is all of our pieces, then we are in a pawn endgame
    return (ourPieces & (kings | pawns)) == ourPieces;
}

int isSquareAttacked(Board *board, int color, int square) {
    int enemy = !color;
    U64 occ = board->colors[BOTH];

    U64 enemyPawns = board->colors[enemy] & board->pieces[PAWN];
    U64 enemyKnights = board->colors[enemy] & board->pieces[KNIGHT];
    U64 enemyBishops =
            board->colors[enemy] & (board->pieces[BISHOP] | board->pieces[QUEEN]);
    U64 enemyRooks =
            board->colors[enemy] & (board->pieces[ROOK] | board->pieces[QUEEN]);
    U64 enemyKings = board->colors[enemy] & board->pieces[KING];

    return (pawnAttacks(color, square) & enemyPawns) ||
         (knightAttacks(square) & enemyKnights) ||
         (enemyBishops && (Bmagic(square, occ) & enemyBishops)) ||
         (enemyRooks && (Rmagic(square, occ) & enemyRooks)) ||
         (kingAttacks(square) & enemyKings);
}

// Detects draws on the board
int isDraw(Board *board) {
    /*
    3 Types of draws:
            1. Fifty move
            2. Three fold repetition
            3. Insufficient material
    */

    // Type 1. Fifty move
    if (board->fiftyMove >= 100)
        return 1;

    // Type 2. Three fold
    // Detected by going through the board history looking for duplicate hash
    for (int i = board->ply - 1; i >= 0; i--) {
        // If the next position contains a move which is irreversible, there is no
        // need to check further
        if (i == board->ply - board->fiftyMove - 1)
            break;

        if (board->hash == board->history[i].hash)
            return 1;
    }

    // Type 3. Insufficient material
    // I will do later

    return 0;
}

// All attackers of a certain square
U64 allAttackersToSquare(Board *board, U64 occupied, int sq) {
    return (pawnAttacks(WHITE, sq) & board->colors[BLACK] & board->pieces[PAWN]) |
         (pawnAttacks(BLACK, sq) & board->colors[WHITE] & board->pieces[PAWN]) |
         (knightAttacks(sq) & board->pieces[KNIGHT]) |
         (Bmagic(sq, occupied) &
                    (board->pieces[BISHOP] | board->pieces[QUEEN])) |
         (Rmagic(sq, occupied) & (board->pieces[ROOK] | board->pieces[QUEEN])) |
         (kingAttacks(sq) & board->pieces[KING]);
}

// Wrapper for allAttackersToSquare() for use in double check detection
U64 attackersToKingSquare(Board *board) {
    int kingsq = getlsb(board->colors[board->side] & board->pieces[KING]);
    U64 occupied = board->colors[BOTH];
    return allAttackersToSquare(board, occupied, kingsq) &
         board->colors[!board->side];
}

// Static Exchange Evaluation
const int SEEPieceValues[NB_PIECES] = {100, 320, 320, 500, 950, 100000};

// Returns true if the capture ends positively materialwise, false otherwise
int SEE(Board *board, Move move, int threshold) {
    int from, exchangeSquare, nextPiece, sideToCapture, evaluation;
    U64 occupied, attackers, attackersOnSide;

    // Extract move info
    from = MoveFrom(move);
    exchangeSquare = MoveTo(move);

    // Next piece to be captured
    nextPiece = board->squares[from];

    // Set evaluation if the move made was a capture
    if (IsCapture(move))
        evaluation = board->squares[exchangeSquare];
    else
        evaluation = 0;
    evaluation -= threshold;
    if (evaluation < 0)
        return false;

    // Worst case
    evaluation -= SEEPieceValues[board->squares[from]];
    if (evaluation >= 0)
        return true;

    // Opponent to move next
    sideToCapture = !board->side;

    // Occupancy after the move is made
    occupied = board->colors[BOTH];
    clearBit(&occupied, from);

    // Get all attackers to the square we're exchanging on
    attackers = allAttackersToSquare(board, occupied, exchangeSquare);

    // Get sliders for xrays
    U64 bishops = board->pieces[BISHOP] | board->pieces[QUEEN];
    U64 rooks = board->pieces[ROOK] | board->pieces[QUEEN];

    while (true) {
        // Get attackers
        attackersOnSide = attackers & board->colors[sideToCapture];

        if (attackersOnSide == 0ULL)
            break; // No more attackers left

        // Get least valuable attacker of the square
        for (nextPiece = PAWN; nextPiece <= QUEEN; nextPiece++)
            if (attackersOnSide & board->pieces[nextPiece])
                break;

        // Remove the attacker from occupied
        clearBit(&occupied, getlsb(attackersOnSide & board->pieces[nextPiece]));

        // Update slider attacks
        // Diagonal moves may reveal bishop attacks
        if (nextPiece == PAWN || nextPiece == BISHOP || nextPiece == QUEEN)
            attackers |= Bmagic(exchangeSquare, occupied) & bishops;
        // Vertical or horizontal moves may reveal bishop or queen attacks
        if (nextPiece == ROOK || nextPiece == QUEEN)
            attackers |= Rmagic(exchangeSquare, occupied) & rooks;

        // Remove pieces already used
        attackers &= occupied;

        // Swap sides
        sideToCapture = !sideToCapture;

        // Negamax evaluation
        evaluation = -evaluation - 1 - SEEPieceValues[nextPiece];
        // If its positive then we won the exchange
        if (evaluation >= 0) {
            // If the last piece was a king and there are still more captures
            // then we actually lost the exchange because the next move would be
            // illegal
            if (nextPiece == KING && (attackers & board->colors[sideToCapture]))
                sideToCapture = !sideToCapture;

            break;
        }
    }
    // The last to move after the loop loses
    return board->side != sideToCapture;
}

// Displays a representation of the board on the terminal
void printBoard(Board *board) {
    char asciiPieces[12] = "PNBRQKpnbrqk";
    int sq;
    int isEmpty;

    // print pieces
    for (int rank = 7; rank >= 0; rank--) {
        printf("%d ", rank + 1);

        for (int file = 0; file < 8; file++) {
            sq = squareFrom(file, rank);

            isEmpty = 1;
            for (int piece = PAWN; piece < NB_PIECES; piece++) {
                if (testBit(board->pieces[piece], sq)) {
                    isEmpty = 0;
                    if (testBit(board->colors[WHITE], sq)) {
                        printf("%c ", asciiPieces[toPiece(piece, WHITE)]);
                    } else if (testBit(board->colors[BLACK], sq)) {
                        printf("%c ", asciiPieces[toPiece(piece, BLACK)]);
                    } else {
                        puts("that wasn't supposed to happen");
                        return;
                    }
                }
            }

            if (isEmpty) {
                printf(". ");
            }
        }
        printf("\n");
    }
    printf("  a b c d e f g h\n");

    // print side to move
    printf("Side: %s\n", (board->side == WHITE) ? "White" : "Black");

    // print en passant square
    if (board->epSquare != NO_SQ) {
        printf("EP square: %c%c\n", 'a' + (board->epSquare % 8),
           '1' + (board->epSquare / 8));
    }

    // print castling rights
    printf("Castle rights: %s%s%s%s\n",
         (board->castlePerm & CASTLE_WK) ? "K" : "",
         (board->castlePerm & CASTLE_WQ) ? "Q" : "",
         (board->castlePerm & CASTLE_BK) ? "k" : "",
         (board->castlePerm & CASTLE_BQ) ? "q" : "");

    // print half moves
    printf("Half moves: %d\n", board->ply);
}

// Sets a provided board to the provided FEN
void parseFen(Board *board, char *fen) {
    int sq;
    int color;
    int piece;

    clearBoard(board);

    // Place pieces
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file < 8; file++) {
            sq = squareFrom(file, rank);

            // if the character in the fen is a '/', the next rank is reached
            if (*fen == '/') {
                fen++;
            }

            // if the character is a number, offset the file by that number
            if (*fen >= '1' && *fen <= '8') {
                int offset = *fen - '0';

                // adjust by -1 to account for the loop increment
                file += offset - 1;
            }

            // if the character is alphabetic, it is a piece
            if (isalpha(*fen)) {
                if (isupper(*fen)) {
                    color = WHITE;
                } else {
                    color = BLACK;
                }

                switch (tolower(*fen)) {
                case 'p':
                    piece = PAWN;
                    break;
                case 'n':
                    piece = KNIGHT;
                    break;
                case 'b':
                    piece = BISHOP;
                    break;
                case 'r':
                    piece = ROOK;
                    break;
                case 'q':
                    piece = QUEEN;
                    break;
                case 'k':
                    piece = KING;
                    break;
                default:
                    printf("FEN parsing error while placing pieces\n");
                    exit(1);
                    break;
                }

                setPiece(board, color, piece, sq);
            }

            fen++;
        }
    }

    // Set side to move
    fen++;
    if (*fen == 'w') {
        board->side = WHITE;
    } else if (*fen == 'b') {
        board->side = BLACK;
    } else {
        puts("FEN parsing error while setting side to move");
        exit(1);
    }
    fen++;
    fen++;

    // Set castle rights
    board->castlePerm = 0;
    while (*fen != ' ') {
        switch (*fen) {
        case 'K':
            board->castlePerm |= CASTLE_WK;
            break;
        case 'Q':
            board->castlePerm |= CASTLE_WQ;
            break;
        case 'k':
            board->castlePerm |= CASTLE_BK;
            break;
        case 'q':
            board->castlePerm |= CASTLE_BQ;
            break;
        case '-':
            break;
        default:
            puts("FEN parsing error in castling rights");
            exit(1);
            break;
        }
        fen++;
    }
    fen++;

    // set en passant square
    if (*fen != '-') {
        int file = fen[0] - 'a';
        int rank = fen[1] - '1';
        board->epSquare = squareFrom(file, rank);
    }
    fen += 2;

    // Set fifty move and half move counter
    board->fiftyMove = strtol(fen, &fen, 10);
    fen++;

    // We ignore the move counter because this is root to our chess engine anyways
    int fullMoves = strtol(fen, &fen, 10);
    board->ply = 0;

    // Reset the Zobrist hash
    board->hash = generateHash(board);
    assert(board->hash == generateHash(board));
}
