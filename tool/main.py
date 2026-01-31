import tkinter as tk
import sys
import os

# Handle import paths for both direct execution and PyInstaller bundling
script_dir = os.path.dirname(os.path.abspath(__file__))
if os.path.basename(script_dir) == 'tool':
    # If running from inside 'tool' directory, add parent to path to support 'from tool.app'
    sys.path.append(os.path.dirname(script_dir))
sys.path.append(script_dir) # Support 'from app'

try:
    from tool.app.gui import FOPDTApp
except ImportError:
    from app.gui import FOPDTApp

if __name__ == "__main__":
    root = tk.Tk()
    app = FOPDTApp(root)
    root.mainloop()
