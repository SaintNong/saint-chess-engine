# Saint chess engine
A hobby chess engine I built in highschool (before I knew how to use git haha) in the holidays between year 11 and 12.
I've learned a lot since then, but it's always fun to look back on a project I worked so hard on.

# Features
- Magic bitboard move generation
- Negamax with fail-soft alpha beta
- Principal variation search
- Hash table (always replace scheme)
- Hash move ordering
- Static Exchange Evaluation
- SEE pruning in quiscence search
- History Heuristic
- Killer Heuristic
- Null move pruning
- Late move reductions
- Mate distance pruning
- Delta pruning
- Aspiration windows
- Futility pruning
- Check extension
- Tapered evaluation (manually tuned)
- Passed and isolated pawn evaluation

# **Please note that this project is very buggy**
I have a new and better engine based built from scratch, loosely based on this one. This one will have SPRT testing and assertions placed everywhere from the start, to hopefully make sure each feature is robust before moving to the next one.
Also the tuner is very broken so ignore that please.

# Credits
see credits.txt
