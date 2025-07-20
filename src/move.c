#include "move.h"

#include <stdio.h>
#include <string.h>

#include "zobrist.h"
#include "eval.h"

// Converts a square index to string coordinates
void squareToStr(int square, char *coords) {
    int file = square % 8;
    int rank = square / 8;
    coords[0] = 'a' + file;
    coords[1] = '1' + rank;
    coords[2] = '\0';
}

int stringToSquare(const char *string) {
    return squareFrom(string[0] - 'a', string[1] - '1');
}

int coordinatesToSquare(const char *coords) {
    // Ensure the input is valid
    if (strlen(coords) != 2)
        return NO_SQ;

    // Convert file and rank from string to index
    int file = coords[0] - 'a'; // Convert 'a'-'h' to 0-7
    int rank = coords[1] - '1'; // Convert '1'-'8' to 0-7

    // Calculate index based on the enum order
    int index = 8 * rank + file; // This will correspond to the enum order

    // Check if index is out of bounds of the board
    if (index < 0 || index >= 64)
        return NO_SQ;

    return index;
}


void moveToString(Move move, char *string) {
    int from = MoveFrom(move);
    int to = MoveTo(move);

    // Put squares in the string
    squareToStr(from, &string[0]);
    squareToStr(to, &string[2]);
    if (IsPromotion(move)) {
        switch (MovePromotedPiece(move)) {
        case KNIGHT:
            string[4] = 'n';
            break;
        case BISHOP:
            string[4] = 'b';
            break;
        case ROOK:
            string[4] = 'r';
            break;
        case QUEEN:
            string[4] = 'q';
            break;
        }
        string[5] = '\0';
    }
}

// Prints move in uci format
// e.g. b7b8q
void printMove(Move move, int includeNewLine) {
    char moveStr[6];
    moveToString(move, moveStr);

    if (includeNewLine)
        printf("%s\n", moveStr);
    else
        printf("%s", moveStr);
}