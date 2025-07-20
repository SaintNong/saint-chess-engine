SRC = $(wildcard *.c)
FLAGS = -O3 -flto -march=native -ffast-math
LIBS = -lm
NO_DEBUG = -D'NDEBUG=1'
EXE = engine

default:
	make run

noflags:
	gcc $(SRC) $(LIBS) -o $(EXE)

debug:
	gcc $(SRC) $(FLAGS) $(LIBS) -o $(EXE)

dist:
	gcc $(SRC) $(FLAGS) $(LIBS) $(NO_DEBUG) -o $(EXE)

run:
	make dist
	./$(EXE)

clean:
	rm $(EXE)
