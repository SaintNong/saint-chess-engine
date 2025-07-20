#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef unsigned long long U64;
#define C64(constantU64) constantU64##ULL

// LSB and MSB
int poplsb(U64 *bitboard);
int popmsb(U64 *bitboard);
int getlsb(U64 bitboard);
int getmsb(U64 bitboard);

// Bitboard operations
int popCount(U64 bitboard);
void setBit(U64 *bitboard, int sq);
void clearBit(U64 *bitboard, int sq);
bool testBit(U64 bitboard, int sq);

// Helper functions for IO
void printBitboard(U64 bitboard);
int squareFrom(int file, int rank);

// Initialise lookup tables
void initAttackMasks();

// Wrappers functions to 3 lookup tables
U64 knightAttacks(int sq);
U64 kingAttacks(int sq);
U64 pawnAttacks(int color, int sq);
