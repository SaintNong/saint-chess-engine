#include "hashtable.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bitboards.h"
#include "board.h"
#include "move.h"
#include "search.h"

// Global variable :skull:
HashTable hashTable;

void updateHashAge() { hashTable.age++; }

void clearHashTable() {
    // Loop through the hash entries, setting all the values to empty
    for (int i = 0; i < hashTable.count; i++) {
        // Clear entry
        hashTable.entries[i].hashKey = 0ULL;
        hashTable.entries[i].bestMove = NO_MOVE;
        hashTable.entries[i].depth = 0;
        hashTable.entries[i].score = 0;
        hashTable.entries[i].flag = 0;
        hashTable.entries[i].age = 0;
    }
}

void freeHashTable() { free(hashTable.entries); }

double occupiedHashEntries() {
    int occupied = 0;
    for (int i = 0; i < hashTable.count; i++) {
        if (hashTable.entries[i].hashKey != 0ULL)
            occupied++;
    }
    return (double)occupied / (double)hashTable.count;
}

// Initialises hash table to certain size in MB
void initHashTable(int sizeMB) {
    // Calculate how many hash entries to match the size
    uint64_t size = sizeMB * 0x100000;
    hashTable.count = size / sizeof(HashEntry);
    hashTable.count -= 2; // for safety (inherited from VICE)

    // Free hash table
    free(hashTable.entries);

    // Allocate and clear the table
    hashTable.entries = (HashEntry *)malloc(hashTable.count * sizeof(HashEntry));

    // Check if allocation failed
    if (hashTable.entries == NULL) {
        puts("Hash allocation failed.");
        puts("Check if you have enough memory");
        exit(1);
    }

    clearHashTable();

    printf("Hash size set to %d MB\n", sizeMB);
    printf("Number of hash entries: %lu\n", hashTable.count);
}

void hashTableStore(U64 hash, Move bestMove, int depth, int score, int flag) {
    // Calculate hash index and retrieve corresponding bucket
    int index = hash % hashTable.count;
    HashEntry *entry = &hashTable.entries[index];

    entry->hashKey = hash;
    entry->bestMove = bestMove;
    entry->depth = depth;
    entry->score = score;
    entry->flag = flag;
    entry->age = hashTable.age;
}

int hashTableProbe(U64 hash, Move *hashMove, int *depth, int *score, int *flag) {
    // Calculate hash index and retrieve corresponding bucket
    int index = hash % hashTable.count;
    HashEntry *entry = &hashTable.entries[index];

    // Check the first entry
    if (entry->hashKey == hash) {
        // Copy data over
        *hashMove = entry->bestMove;
        *depth = entry->depth;
        *score = entry->score;
        *flag = entry->flag;

        return PROBE_SUCCESS;
    }

    return PROBE_FAIL;
}

// Back when the engine used hash table PV probing
// Credit: VICE once again

// Move probePVMove(U64 hash)
// {
//     // Calculate hash index
//     int index = hash % hashTable.count;
//     HashEntry *entry = &hashTable.entries[index];

//     // Retrieve best move if hash matches
//     if (entry->hashKey == hash)
//     {
//         return entry->bestMove;
//     }

// 	return NO_MOVE;
// }

// // Retrieves the PV line from the hash table
// int storePVLine(PV *line, Board *board, int depth)
// {
//     Move move = probePVMove(board->hash);
//     int count = 0;
//     int max;

//     // Continually retrieve moves from the hash table
//     while (move != NO_MOVE && count < depth)
//     {
//         if (moveExists(board, move))
//         {
//             makeMove(board, move);
//             line->moves[count++] = move;
//         }
//         else
//         {
//             break;
//         }

//         move = probePVMove(board->hash);
//     }
//     max = count;

//     // Undo all the moves
//     while (count > 0) {
//         count--;
//         undoMove(board, line->moves[count]);
//     }

//     return max;
// }
