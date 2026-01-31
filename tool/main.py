import tkinter as tk
import sys
import os

# Add the project root directory to path so we can import 'tool.app'
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from tool.app.gui import FOPDTApp

if __name__ == "__main__":
    root = tk.Tk()
    app = FOPDTApp(root)
    root.mainloop()
