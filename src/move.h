#pragma once

typedef unsigned short Move;


/*
    16-bit move representation breakdown

    0000 0000 0000 0000 |
    0000 0000 0011 1111 | Move origin square      (6 bits => 64)
    0000 1111 1100 0000 | Move destination square (6 bits => 64)
    1111 0000 0000 0000 | Flags                   (4 bits => 16)
*/

// Flags are blatantly stolen from Berserk <3
#define NO_MOVE 0

#define QUIET_FLAG 0b0000
#define CASTLE_FLAG 0b0001
#define CAPTURE_FLAG 0b0100
#define EP_FLAG 0b0110
#define PROMO_FLAG 0b1000
#define KNIGHT_PROMO_FLAG 0b1000
#define BISHOP_PROMO_FLAG 0b1001
#define ROOK_PROMO_FLAG 0b1010
#define QUEEN_PROMO_FLAG 0b1011

#define ConstructMove(from, to, flags) (from) | ((to) << 6) | ((flags) << 12)

#define MoveFrom(move) ((move) & 0x3f)
#define MoveTo(move) (((move) & 0xfc0) >> 6)
#define MoveFlags(move) (((move) & 0xf000) >> 12)

#define IsCapture(move) (!!(MoveFlags(move) & CAPTURE_FLAG))
#define IsEnpass(move) ((MoveFlags(move) == EP_FLAG))
#define IsCastling(move) ((MoveFlags(move) == CASTLE_FLAG))
#define IsPromotion(move) (!!(MoveFlags(move) & PROMO_FLAG))
#define MovePromotedPiece(move) ((MoveFlags(move) & 0b0011) + 1)

// Move functions
void printMove(Move move, int includeNewLine);
void moveToString(Move move, char *string);
int stringToSquare(const char *string);
