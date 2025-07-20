import chess
import chess.pgn
import json
import copy
import random

import multiprocessing
from multiprocessing import Pool
import numpy as np
import time

# helper function so i can see what i'm doing
def print_bitboard(bitboard):
    string = '{:064b}'.format(bitboard).replace("1", "X").replace("0", ".")

    idx = 0
    for x in range(8):
        for y in range(8):
            print(string[idx], end=" ")
            idx += 1
        print("")
    print("")

# Stolen from stackoverflow so I can split my weights up
# because python doesn't do arrays (I mean, what is a list?)
def split_array(arr, size):
     arrs = []
     while len(arr) > size:
         pice = arr[:size]
         arrs.append(pice)
         arr   = arr[size:]
     arrs.append(arr)
     return arrs

def print_array(arr):
    # indent before array
    print("    ", end="")

    # stringify the array
    arr = [str(item) for item in arr]

    # array
    print(", ".join(arr), end="")

    # final comma
    print(",")

def flatten(S):
    if S == []:
        return S
    if isinstance(S[0], list):uation
        return flatten(S[0]) + flatten(S[1:])
    return S[:1] + flatten(S[1:])

# For multiprocessing
def process_chunk(chunk, K, tuner_inst):
    board = chess.Board()
    total_error = 0.0
    for position in chunk:
        FEN = position[0]
        result = position[1]

        # Assuming get_static_evaluation and map_evaluation_to_sigmoid are independent methods
        evaluation = tuner_inst.get_static_evaluation(FEN, board)
        probability_eval = tuner_inst.map_evaluation_to_sigmoid(evaluation, K)

        error = result - probability_eval
        total_error += pow(error, 2)
    return total_error

piece_names = [
    "Pawn", "Knight", "Bishop", "Rook", "Queen", "King"
]

# Texel's tuning method implemented by a dumb child
class Tuner:
    raw_data = []
    data = []
    data_size = 0
    board = chess.Board()
    
    # Weights used in evaluation function
    # To be loaded before tuning them
    mat_mg = []
    mat_eg = []
    pst_mg = []
    pst_eg = []
    mob_bonus = []
    king_atk_bonus = []


    bishop_pair_bonus = 0
    stm_bonus = 0

    # All the weights in one 1d list
    weights = []

    def __init__(self, training_file, weights_file, output_file, number_of_positions):
        self.weights_file = weights_file
        self.training_file = training_file
        self.out_file = output_file

        self.load_training_data(training_file, number_of_positions)
        self.load_weights(weights_file)
        self.update_weights(self.weights)



    def load_training_data(self, file_path, max_positions):
        # Load file full of FEN strings and win/loss/draw results and process them
        print(f"Using training data file '{file_path}'")
        with open(file_path, "r") as file:
            self.raw_data = file.read().splitlines()
        
        # Shuffle to avoid overfitting the same set of positions
        random.shuffle(self.raw_data)

        # Process positions into FEN string and game result
        positions_parsed = 0
        for position in self.raw_data:
            fen, result = position.rsplit(' ', 1)
            positions_parsed += 1
            if positions_parsed >= max_positions:
                break

            # Make the game result into a float
            # 0.5 means draw, 1.0 means win and 0.0 means loss
            result = float(result[1:-1])

            self.data.append([fen, result])

        self.data_size = len(self.data)
        
        print(f"Loaded {len(self.data)} training positions")

    def load_weights(self, file_path):
        # Loads weights from a json file
        print(f"Using starting weights file '{file_path}'")
        with open(file_path, "r") as file:
            weights = json.loads(file.read())
        
        # Put them into the class variables
        self.weights += weights["mat_mg"]
        self.weights += weights["mat_eg"]
        self.mat_mg = self.weights[:6]
        self.mat_eg = self.weights[6:12]

        self.weights += flatten(weights["pst_mg"])
        self.weights += flatten(weights["pst_eg"])

        self.weights += flatten(weights["mob_bonus"])
        self.weights += flatten(weights["king_atk_bonus"])

        self.weights.append(weights["bishop_pair_bonus"])
        self.weights.append(weights["stm_bonus"])

        print("Weights loaded successfully")

    def update_weights(self, new_weights):
        # Propagates changes from a 1d weight array to the eval parameters
        self.mat_mg = new_weights[:6]
        self.mat_eg = new_weights[6:12]

        self.pst_mg = split_array(new_weights[12:396], 64)
        self.pst_eg = split_array(new_weights[396:780], 64)

        self.mob_bonus = split_array(new_weights[780:792], 6)
        self.king_atk_bonus = split_array(new_weights[792:804], 6)

        self.bishop_pair_bonus = new_weights[804]
        self.stm_bonus = new_weights[805]

        self.weights = new_weights

    def save_weights(self):
        print(f"Saving weights to '{self.out_file}'")
        # Save the tuned weights to json
        with open(self.out_file, "w") as f:
            # Weights format
            weights = {
                "mat_mg": self.mat_mg,
                "mat_eg": self.mat_eg,
                "pst_mg": flatten(self.pst_mg),
                "pst_eg": flatten(self.pst_eg),
                "mob_bonus": flatten(self.mob_bonus),
                "king_atk_bonus": flatten(self.king_atk_bonus),
                "bishop_pair_bonus": self.bishop_pair_bonus,
                "stm_bonus": self.stm_bonus,
            }
            # Dump to json
            json.dump(weights, f)

        
        
        print("Weights saved successfully")


    def get_tapered_score(self, score_mg, score_eg, phase):
        START_PHASE = 256
        phase = min(phase, START_PHASE)

        # Tapers score depending on how much material is on the board
        # btw '>> 8' ~ 'divide by 256'
        return ((score_mg * phase) + (score_eg * (START_PHASE - phase))) >> 8
    
    def get_static_evaluation(self, FEN, board):
        # Engine constants
        PHASE_VALUES = [0, 10, 10, 22, 44, 0]
        middlegame = 0
        endgame = 1

        # A port of my chess engine's evaluation function to python

        # Set our board to the FEN
        board.set_fen(FEN)

        score = 0

        # Quick calculation for piece imbalances e.g. bishop pair
        if chess.popcount(board.bishops & board.occupied_co[chess.WHITE]) > 1:
            score += self.bishop_pair_bonus
        if chess.popcount(board.bishops & board.occupied_co[chess.BLACK]) > 1:
            score -= self.bishop_pair_bonus

        # Side to move bonus
        if board.turn == chess.WHITE:
            score += self.stm_bonus
        else:
            score -= self.stm_bonus

        # Initialise the two phase evaluations for middlegame and endgame and phase variable
        score_mg = 0
        score_eg = 0
        phase = 0

        # Get king attacks bitboard for use later
        white_king = board.king(chess.WHITE)
        white_king_atk = board.attacks_mask(white_king)

        black_king = board.king(chess.BLACK)
        black_king_atk = board.attacks_mask(black_king)
        

        # Loop through all the squares
        for square in range(64):
            if board.piece_at(square):
                piece_obj = board.piece_at(square)
                piece = piece_obj.piece_type - 1

                # Mobility calculation
                if piece >= chess.BISHOP:
                    sideBB = board.occupied_co[piece_obj.color]
                    attacks = board.attacks_mask(square) & ~sideBB
                    mobility = chess.popcount(attacks)
                else:
                    sideBB = 0
                    attacks = 0
                    mobility = 0
                
                # Calculations for white or black
                if piece_obj.color == chess.WHITE:
                    # Material
                    score_mg += self.mat_mg[piece]
                    score_eg += self.mat_eg[piece]

                    # Piece square tables
                    score_mg += self.pst_mg[piece][chess.square_mirror(square)]
                    score_eg += self.pst_eg[piece][chess.square_mirror(square)]

                    # Mobility
                    score_mg += self.mob_bonus[middlegame][piece] * mobility
                    score_eg += self.mob_bonus[endgame][piece] * mobility

                    # King attacks
                    king_attacks = chess.popcount(attacks & black_king_atk)
                    score_mg += self.king_atk_bonus[middlegame][piece] * king_attacks
                    score_eg += self.king_atk_bonus[endgame][piece] * king_attacks

                else:
                    # Material
                    score_mg -= self.mat_mg[piece]
                    score_eg -= self.mat_eg[piece]

                    # Piece square tables
                    score_mg -= self.pst_mg[piece][square]
                    score_eg -= self.pst_eg[piece][square]

                    # Mobility
                    score_mg -= self.mob_bonus[middlegame][piece] * mobility
                    score_eg -= self.mob_bonus[endgame][piece] * mobility

                    # King attacks
                    king_attacks = chess.popcount(attacks & white_king_atk)
                    score_mg -= self.king_atk_bonus[middlegame][piece] * king_attacks
                    score_eg -= self.king_atk_bonus[endgame][piece] * king_attacks

                phase += PHASE_VALUES[piece]

        # Tapered eval according to phase
        score += self.get_tapered_score(score_mg, score_eg, phase)

        # Negamax dictates that score should be returned from the side to move's perspective
        score = int(score)
        if board.turn == chess.WHITE:
            return score
        else:
            return -score

    def map_evaluation_to_sigmoid(self, eval, K):
        # K is a constant which is calcualated to minimise E (error)
        # By that I mean I just guessed a bunch of times until the mean squared error looked small

        return 1 / (1 + pow(10, -K * eval / 400))

    def calculate_mean_squared_error_parallel(self, K):
        # Determine the number of processes and chunk size
        num_processes = multiprocessing.cpu_count()
        chunk_size = len(self.data) // num_processes

        # Split the dataset into chunks
        chunks = [self.data[i:i + chunk_size] for i in range(0, len(self.data), chunk_size)]

        # Process chunks in parallel
        with Pool(num_processes) as pool:
            errors = pool.starmap(process_chunk, [(chunk, K, self) for chunk in chunks])

        # Aggregate the results
        total_error = np.sum(errors)

        # Return the mean
        return total_error / self.data_size


    def calculate_mean_squared_error(self, K):
        # Obtains the mean squared evaluation error over the dataset
        # In theory, the evaluation function should be a good predictor of the result of
        # the game. To know how 'bad' our current evaluation function is then, we can map
        # the evaluation function to a sigmoid, representing the chance of victory at any
        # point.
        # This new probability evaluation can be applied to every entry in the dataset.
        # Since we have the results of games in our dataset already, we can calculate how
        # far off our evaluation function was to predicting the actual result of the game.
        # We measure this as the "mean squared error".

        total_error = 0.0
        

        # Loop through all the positions in our dataset
        for position in self.data:
            FEN = position[0]
            result = position[1]

            # Calculate the evaluation
            evaluation = self.get_static_evaluation(FEN, self.board)

            # Map the evaluation to a sigmoid
            probability_eval = self.map_evaluation_to_sigmoid(evaluation, K)

            # Add error squared to the total error
            error = result - probability_eval
            total_error += pow(error, 2)
            # And we're done! Wasn't that simple?

        # Return the mean
        return total_error / self.data_size



    def parse_pgn_to_fens(self, pgn_fp, output_fp, n_positions_to_parse):
        # Parses a PGN file to obtain valid training data of a requested size
        # Positions in the training data must also be quiet so the evaluation is accurate
        data = []

        # Open pgn file to parse it to python-chess
        count = 1
        positions_parsed = 0
        pgn = open(pgn_fp, "r")
        while True:
            # Update the user so they don't turn off the program because they think it'll take forever
            if count % 10 == 0:
                print(f"Parsing game {count}, Positions parsed: {positions_parsed}")
            # Extract each game
            game = chess.pgn.read_game(pgn)
            board = game.board()

            # Extract game result to be attached to FEN later
            result = game.headers['Result']
            if result == "1/2-1/2":
                result = "[0.5]"
            elif result == "1-0":
                result = "[1.0]"
            elif result == "0-1":
                result = "[0.0]"
            else:
                raise ValueError(f"Result value is not in proper format {result}")

            # Go through the game
            move_number = 0
            for move in game.mainline_moves():
                board.push(move)
                move_number += 1

                # If there aren't any captures in the position, and the > 5 full moves have been played
                # then this position can go in our training data
                # Positions are also too risky to put in our data if the side to move is in check, or its checkmate
                # as those will not be evaluated in the evaluation function of in the real engine
                if board.is_check():
                    continue
                elif board.is_checkmate() or board.is_stalemate():
                    continue
                
                # if more than 5 full moves have passed
                if move_number > 10:
                    # Find if there are captures
                    if sum(1 for capture in board.generate_pseudo_legal_captures()) == 0:
                        # This is suitable for our dataset!
                        fen = board.fen()

                        line = f"{fen} {result}"

                        data.append(line)
                        positions_parsed += 1
            count += 1

            # End of file reached
            # or we have enough positions
            if game == None or positions_parsed >= n_positions_to_parse:
                pgn.close()
                break


        # Write to file
        print("Parsing PGN done")
        print("Writing to file")
        with open(output_fp, "w") as out_file:
            # Convert to one massive string
            string_data = "\n".join(data)

            # Write it to the file and we're done!
            out_file.write(string_data)

        return data

    def tune(self, K):
        # Local optimisation of all weights
        # Translated from chess programming wiki's algorithm from peter osterlund

        # These are the best weights from which to improve on
        best_weights = self.weights
        best_E = self.calculate_mean_squared_error(K)
        improved = True
        step_size = 1
        epoch = 0

        while improved:
            improved = False
            epoch += 1

            # For every weight
            for i in range(len(best_weights)):
                new_weights = copy.deepcopy(best_weights)

                # Adjust weight upwards and see whether the error is higher
                new_weights[i] += step_size
                self.update_weights(new_weights)

                # Calculate Error
                E = self.calculate_mean_squared_error_parallel(K)

                # Print information so the user doesn't get bored
                print(f"(Tuner) Epoch {epoch} Weight {i} Error {E} Best Error {best_E}")

                # We beat the best error
                if E < best_E:
                    # Save the error and weights
                    best_E = E
                    best_weights = copy.deepcopy(new_weights)
                    improved = True

                    print("Improvement found")
                    self.save_weights()
                
                else:
                    # Try subtract 1 instead
                    new_weights[i] -= step_size * 2
                    self.update_weights(new_weights)

                    # Calculate Error
                    E = self.calculate_mean_squared_error(K)

                    # Print information so the user doesn't get bored
                    print(f"(Tuner) Epoch {epoch} Weight {i} Error {E} Best Error {best_E}")

                    # We beat the best error
                    if E < best_E:
                        # Save the error and weights
                        best_E = E
                        best_weights = copy.deepcopy(new_weights)
                        improved = True

                        print("Improvement found")
                        self.save_weights()
                
            print("Session iteration completed")
            self.save_weights()
        
        print("Writing final weights")
        self.save_weights()

    def print_weights(self):
        # Dumps weights to C header file code
        # Midgame material
        print("// Tuned evaluation parameters from Texel Tuner")
        print("static const int middleGameMaterial[NB_PIECES] = {")
        print_array(self.mat_mg)
        print("};")
        print("")

        # Endgame material
        print("static const int endGameMaterial[NB_PIECES] = {")
        print_array(self.mat_eg)
        print("};")
        print("")

        # Midgame piece square tables
        print("static const int middleGamePSQT[NB_PIECES][64] = {")
        for i, table in enumerate(self.pst_mg):
            # Split each piece square table into sizes 8
            parts = split_array(table, 8)

            # Print piece comment at the start
            print(f"    //{piece_names[i]}s")

            # Print each part
            for part in parts:
                print_array(part)
        print("};")
        print("")
            

        # Endgame piece square tables
        print("static const int endGamePSQT[NB_PIECES][64] = {")
        for i, table in enumerate(self.pst_eg):
            # Split each piece square table into sizes 8
            parts = split_array(table, 8)

            # Print piece comment at the start
            print(f"    //{piece_names[i]}s")

            # Print each part
            for part in parts:
                print_array(part)
        print("};")
        print("")
            
        
        # Mobility bonus
        print("static const int mobilityBonus[2][NB_PIECES] = {")
        print_array(self.mob_bonus[0])
        print_array(self.mob_bonus[1])
        print("};")
        print("")

        # King attack bonus
        print("static const int kingAttackBonus[2][NB_PIECES] = {")
        print_array(self.king_atk_bonus[0])
        print_array(self.king_atk_bonus[1])
        print("};")
        print("")

        # Constants
        print(f"#define BISHOP_PAIR_BONUS {self.bishop_pair_bonus}")
        print(f"#define STM_BONUS {self.stm_bonus}")


        


if __name__ == "__main__":
    # Make da tuner
    tuner = Tuner(training_file="data.txt", weights_file="new_weights.json", output_file="new_weights.json", number_of_positions=32_000)

    # Use da tuner
    K = 1.13
    # print(tuner.map_evaluation_to_sigmoid(500, K))
    # tuner.tune(K)

    tuner.print_weights()

    # # Time the original function
    # start_time = time.time()
    # mse = tuner.calculate_mean_squared_error(K)
    # end_time = time.time()
    # print(f"Mean Squared Error: {mse}")
    # print(f"Time taken by calculate_mean_squared_error: {end_time - start_time} seconds")

    # # Time the parallel function
    # start_time = time.time()
    # mse_parallel = tuner.calculate_mean_squared_error_parallel(K)
    # end_time = time.time()
    # print(f"Mean Squared Error (Parallel): {mse_parallel}")
    # print(f"Time taken by calculate_mean_squared_error_parallel: {end_time - start_time} seconds")



# Training data was generated using this function call
# tuner.parse_pgn_to_fens("ccrl2024jan.pgn", "data.txt", 200_000)


