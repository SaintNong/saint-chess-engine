#pragma once

#include <stdlib.h>
#ifdef WIN32
#include "windows.h"
#else
#include "string.h"
#include "sys/select.h"
#include "sys/time.h"
#include "unistd.h"
#endif

int getTime();
int timeToThink(int time, int inc, int movestogo, int movetime);
int inputWaiting();
