a_to_h = "abcdefgh"
to_8 = [1, 2, 3, 4, 5, 6, 7, 8]
for rank in to_8:
    for file in a_to_h:
        print(file.upper() + str(rank) + ", ", end="")
    print()


