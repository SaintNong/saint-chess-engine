#pragma once

#include "board.h"
#include "move.h"
#include "movegen.h"

// Constants used in the search
#define INF 25000
#define MATE 24500
#define MAX_SEARCH_DEPTH 256

#define DELTA_PRUNING_MARGIN 200

// Min and max
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

// Readability 100
#define IS_PV 1
#define NOT_PV 0

// Sizes in centipawns for aspiration window re-searches
#define ASPIRATION_MAX 10
static int ASPIRATION_SIZES[ASPIRATION_MAX] = {
    20,  40,  55,  75, 130, 210, 350, 700, 1200, INF
};

// PV line definition
typedef struct {
    Move moves[MAX_SEARCH_DEPTH];
    int count;
} PV;

// Global search information
typedef struct {
    int startTime;
    int endTime;
    int depthToSearch;

    long nodes;

    bool quit;
    bool stopped;
    bool timeSet;

    float fh;
    float fhf;

    float hashAttempt;
    float hashHit;

} SearchInfo;

void beginSearch(Board *board, SearchInfo *info);
void initLMRDepths();
