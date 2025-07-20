
with open("rev.txt", "r") as f:
    uInput = f.read().splitlines()



print("\n".join(uInput[::-1]))


