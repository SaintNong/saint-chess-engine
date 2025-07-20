import tkinter as tk

class PSTEditor:
    def __init__(self, master):
        self.master = master
        master.title("Piece-Square Table Editor")

        self.pst = [[0 for _ in range(64)] for _ in range(2)]  # Initialize the PST for two colors
        self.entries = [[None for _ in range(64)] for _ in range(2)]  # Entries for user input

        self.create_board()

        self.print_button = tk.Button(master, text="Print PST", command=self.print_pst)
        self.print_button.pack()

    def create_board(self):
        for color in range(2):
            frame = tk.Frame(self.master)
            frame.pack()

            color_label = tk.Label(frame, text="WHITE" if color == 0 else "BLACK")
            color_label.grid(row=0, column=0, columnspan=8)

            for i in range(64):
                row, col = divmod(i, 8)
                entry = tk.Entry(frame, width=5)
                entry.grid(row=row + 1, column=col)
                entry.insert(0, str(self.pst[color][i]))
                self.entries[color][i] = entry

    def print_pst(self):
        for color in range(2):
            print("// " + ("WHITE" if color == 0 else "BLACK"))
            for i in range(64):
                if i % 8 == 0 and i > 0:
                    print("")
                self.pst[color][i] = int(self.entries[color][i].get())
                print(f"{self.pst[color][i]:3}, ", end="")
            print("\n")


root = tk.Tk()
app = PSTEditor(root)
root.mainloop()


