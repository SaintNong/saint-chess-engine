#pragma once

#include "board.h"

int makeMove(Board *board, Move move);
void undoMove(Board *board, Move move);
void makeNullMove(Board *board);
void undoNullMove(Board *board);
