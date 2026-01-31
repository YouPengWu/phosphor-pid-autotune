import tkinter as tk
import math
from tkinter import filedialog, messagebox
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import (
    FigureCanvasTkAgg,
    NavigationToolbar2Tk,
)
import sys
import os
import traceback
import glob

# Import from local package
from .data_io import parse_data_file
from .plotting import get_model_curve
from .fopdt_algo import identify_two_point, identify_lsm, identify_nelder_mead
from .pid_algo import calculate_pid

# Hack for PyInstaller + Matplotlib + Pillow issue
try:
    import PIL.ImageTk
    import PIL._tkinter_finder
except ImportError:
    pass


class FOPDTApp:
    def __init__(self, root):
        self.root = root
        self.root.title("FOPDT Analysis Tool")
        self.root.geometry("1800x1200")

        # --- Layout ---
        # Main Frame
        main_frame = tk.Frame(root)
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Left Panel (Controls & Info)
        left_panel = tk.Frame(
            main_frame, width=400, bg="#f0f0f0"
        )  # Widen panel slightly
        left_panel.pack_propagate(False)  # Prevent resizing based on content
        left_panel.pack(side=tk.LEFT, fill=tk.Y, padx=5, pady=5)

        # Right Panel (Plot)
        right_panel = tk.Frame(main_frame, bg="white")
        right_panel.pack(
            side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=5, pady=5
        )

        # --- Bottom Bar for Plot (Save Button) ---
        plot_bottom_bar = tk.Frame(right_panel, bg="white")
        plot_bottom_bar.pack(side=tk.BOTTOM, fill=tk.X, pady=5)

        self.btn_save_plot = tk.Button(
            plot_bottom_bar,
            text="Save Plot",
            command=self.save_plot,
            font=("Arial", 12, "bold"),
        )
        self.btn_save_plot.pack(side=tk.RIGHT, padx=10)

        # --- 1. Load Data File ---
        tk.Label(
            left_panel,
            text="1. Load Data",
            font=("Arial", 12, "bold"),
            bg="#f0f0f0",
        ).pack(anchor=tk.W, pady=(10, 5))

        # Container for Load Section
        frame_load_sec = tk.Frame(left_panel, bg="#f0f0f0")
        frame_load_sec.pack(fill=tk.X, padx=10, anchor=tk.W)

        self.loaded_files = []  # Store full paths

        # Buttons Row
        frame_btns = tk.Frame(frame_load_sec, bg="#f0f0f0")
        frame_btns.pack(fill=tk.X, pady=2)

        # Single Menu Button mechanism
        self.btn_load = tk.Button(
            frame_btns,
            text="Load Data  ▼",
            command=self.show_load_menu,
            height=2,
            width=30,
        )
        self.btn_load.pack(pady=5, fill=tk.X)

        self.load_menu = tk.Menu(self.root, tearoff=0)
        self.load_menu.add_command(
            label="Select Files...", command=self.load_files_click
        )
        self.load_menu.add_command(
            label="Select Folder...", command=self.load_folder_click
        )

        # File Listbox
        frame_list = tk.Frame(frame_load_sec, bg="white")
        frame_list.pack(fill=tk.BOTH, expand=True, pady=5)

        scrollbar = tk.Scrollbar(frame_list)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        self.lst_files = tk.Listbox(
            frame_list, height=6, bg="white", yscrollcommand=scrollbar.set
        )
        self.lst_files.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.config(command=self.lst_files.yview)

        self.lst_files.bind("<<ListboxSelect>>", self.on_file_select)

        # Window Size Input (Moved below list)
        frame_win = tk.Frame(frame_load_sec, bg="#f0f0f0")
        frame_win.pack(pady=5, anchor=tk.W)

        tk.Label(frame_win, text="Avg Window:", bg="#f0f0f0").pack(
            side=tk.LEFT
        )
        self.entry_window = tk.Entry(frame_win, width=5)
        self.entry_window.insert(0, "30")  # Default
        self.entry_window.pack(side=tk.LEFT, padx=5)
        self.entry_window.bind("<Return>", self.on_window_change)
        self.entry_window.bind("<KP_Enter>", self.on_window_change)

        # Hint Label
        self.lbl_win_hint = tk.Label(
            frame_load_sec,
            text="",
            bg="#f0f0f0",
            fg="gray",
            justify=tk.LEFT,
            font=("Arial", 9),
        )
        self.lbl_win_hint.pack(pady=(0, 5), anchor=tk.W, padx=5)

        self.current_filepath = None

        # Helper for Color Dots
        def create_dot_image(color, size=12):
            img = tk.PhotoImage(width=size, height=size)
            img.put(color, to=(0, 0, size, size))
            return img

        self.img_grey = create_dot_image("grey")
        self.img_blue = create_dot_image("blue")
        self.img_green = create_dot_image("green")
        self.img_red = create_dot_image("red")

        # --- 2. Plotting Options (Checkboxes) ---
        tk.Label(
            left_panel,
            text="2. Curve Visibility",
            font=("Arial", 12, "bold"),
            bg="#f0f0f0",
        ).pack(anchor=tk.W, pady=(20, 5))

        self.show_actual = tk.BooleanVar(value=True)
        self.show_632 = tk.BooleanVar(value=True)
        self.show_lsm = tk.BooleanVar(value=True)
        self.show_opt = tk.BooleanVar(value=True)

        frame_chk = tk.Frame(left_panel, bg="#f0f0f0")
        frame_chk.pack(fill=tk.X, padx=10)

        # Actual - Grey
        cb_actual = tk.Checkbutton(
            frame_chk,
            text=" Actual Data",
            variable=self.show_actual,
            command=self.refresh_plot,
            bg="#f0f0f0",
            font=("Arial", 12),
            image=self.img_grey,
            compound=tk.LEFT,
            selectcolor="#f0f0f0",
        )
        cb_actual.pack(anchor=tk.W, pady=2)

        # Nelder-Mead - Red (Moved up)
        cb_opt = tk.Checkbutton(
            frame_chk,
            text=" Nelder-Mead (Opt)",
            variable=self.show_opt,
            command=self.refresh_plot,
            bg="#f0f0f0",
            font=("Arial", 12),
            image=self.img_red,
            compound=tk.LEFT,
            selectcolor="#f0f0f0",
        )
        cb_opt.pack(anchor=tk.W, pady=2)

        # LSM - Green
        cb_lsm = tk.Checkbutton(
            frame_chk,
            text=" LSM Method",
            variable=self.show_lsm,
            command=self.refresh_plot,
            bg="#f0f0f0",
            font=("Arial", 12),
            image=self.img_green,
            compound=tk.LEFT,
            selectcolor="#f0f0f0",
        )
        cb_lsm.pack(anchor=tk.W, pady=2)

        # Two Point - Blue (Moved down)
        cb_632 = tk.Checkbutton(
            frame_chk,
            text=" Two Point (63.2%)",
            variable=self.show_632,
            command=self.refresh_plot,
            bg="#f0f0f0",
            font=("Arial", 12),
            image=self.img_blue,
            compound=tk.LEFT,
            selectcolor="#f0f0f0",
        )
        cb_632.pack(anchor=tk.W, pady=2)

        # --- 3. Tuning Method Selection ---
        tk.Label(
            left_panel,
            text="3. Tuning Method",
            font=("Arial", 12, "bold"),
            bg="#f0f0f0",
        ).pack(anchor=tk.W, pady=(20, 5))

        frame_method = tk.Frame(left_panel, bg="#f0f0f0")
        frame_method.pack(fill=tk.X, padx=10)

        self.tuning_method_var = tk.StringVar(value="Nelder-Mead")
        self.opt_method = tk.OptionMenu(
            frame_method,
            self.tuning_method_var,
            "Nelder-Mead",
            "LSM",
            "Two Point",
            command=self.on_method_change,
        )
        self.opt_method.config(
            compound=tk.LEFT, image=self.img_red
        )  # Default Red
        self.opt_method.pack(anchor=tk.W, fill=tk.X)

        # Update Menu Items with Images
        menu = self.opt_method["menu"]
        # Index 0: Nelder-Mead (Red)
        menu.entryconfig(0, image=self.img_red, compound=tk.LEFT)
        # Index 1: LSM (Green)
        menu.entryconfig(1, image=self.img_green, compound=tk.LEFT)
        # Index 2: Two Point (Blue)
        menu.entryconfig(2, image=self.img_blue, compound=tk.LEFT)

        # --- 4. FOPDT Parameters (Read-Only) ---
        tk.Label(
            left_panel,
            text="4. FOPDT Parameters",
            font=("Arial", 12, "bold"),
            bg="#f0f0f0",
        ).pack(anchor=tk.W, pady=(20, 5))

        frame_params = tk.Frame(left_panel, bg="#f0f0f0")
        frame_params.pack(fill=tk.X, padx=10)

        # K
        tk.Label(frame_params, text="K:", bg="#f0f0f0", width=4).grid(
            row=0, column=0
        )
        self.entry_k = tk.Entry(frame_params, width=8, state="readonly")
        self.entry_k.grid(row=0, column=1, padx=2, pady=2)

        # Tau
        tk.Label(frame_params, text="Tau:", bg="#f0f0f0", width=4).grid(
            row=0, column=2
        )
        self.entry_tau = tk.Entry(frame_params, width=8, state="readonly")
        self.entry_tau.grid(row=0, column=3, padx=2, pady=2)

        # Theta
        tk.Label(frame_params, text="Theta:", bg="#f0f0f0", width=5).grid(
            row=0, column=4
        )
        self.entry_theta = tk.Entry(frame_params, width=8, state="readonly")
        self.entry_theta.grid(row=0, column=5, padx=2, pady=2)

        # --- 5. Tuning Knob ---
        tk.Label(
            left_panel,
            text="5. IMC Design Parameter (ε)",
            font=("Arial", 12, "bold"),
            bg="#f0f0f0",
        ).pack(anchor=tk.W, pady=(20, 5))

        frame_slider = tk.Frame(left_panel, bg="#f0f0f0")
        frame_slider.pack(fill=tk.X, padx=10)

        # Epsilon Slider & Entry
        frame_slider_ctrl = tk.Frame(frame_slider, bg="#f0f0f0")
        frame_slider_ctrl.pack(fill=tk.X, anchor=tk.W)

        # Row 1: Epsilon Input AND Ratio Display
        frame_eps_row = tk.Frame(frame_slider_ctrl, bg="#f0f0f0")
        frame_eps_row.pack(fill=tk.X, pady=2)

        # Epsilon Label & Entry
        tk.Label(
            frame_eps_row,
            text="Epsilon (ε):",
            bg="#f0f0f0",
            width=12,
            anchor=tk.W,
        ).pack(side=tk.LEFT)
        self.entry_epsilon = tk.Entry(frame_eps_row, width=8)
        self.entry_epsilon.pack(side=tk.LEFT, padx=5)
        self.entry_epsilon.insert(0, "1.0")
        self.entry_epsilon.bind("<Return>", self.on_entry_change)
        self.entry_epsilon.bind("<KP_Enter>", self.on_entry_change)
        self.entry_epsilon.bind("<FocusOut>", self.on_entry_change)

        # Ratio Label & Value (Right Side)
        tk.Label(
            frame_eps_row,
            text="Ratio (ε/θ):",
            bg="#f0f0f0",
            width=12,
            anchor=tk.W,
        ).pack(side=tk.LEFT, padx=(20, 0))
        self.lbl_ratio_val = tk.Label(
            frame_eps_row, text="-", bg="#e0e0e0", width=8, relief=tk.SUNKEN
        )
        self.lbl_ratio_val.pack(side=tk.LEFT, padx=5)

        self.lbl_eps_display = tk.Label(
            frame_slider,
            text="ε = 1.000",
            font=("Arial", 11, "bold"),
            bg="#f0f0f0",
            fg="#333",
        )
        self.lbl_eps_display.pack(anchor=tk.CENTER, pady=(5, 0))

        self.scale_epsilon = tk.Scale(
            frame_slider,
            from_=0.0,
            to=3.0,
            resolution=0.01,
            orient=tk.HORIZONTAL,
            command=self.on_slider_change,
            showvalue=0,
        )
        self.scale_epsilon.set(1.5)
        self.scale_epsilon.pack(fill=tk.X)

        # Mode Buttons (Dynamic)
        self.frame_mode_btns = tk.Frame(frame_slider, bg="#f0f0f0")
        self.frame_mode_btns.pack(fill=tk.X, pady=(2, 5))

        # Performance
        self.btn_perf = tk.Button(
            self.frame_mode_btns,
            text="Performance",
            command=lambda: self.set_slider_mode("perf"),
            bg="#ffcccc",
            font=("Arial", 9),
        )
        self.btn_perf.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=2)

        # Normal
        self.btn_norm = tk.Button(
            self.frame_mode_btns,
            text="Normal",
            command=lambda: self.set_slider_mode("norm"),
            bg="#ccffcc",
            font=("Arial", 9),
        )
        self.btn_norm.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=2)

        # Quiet
        self.btn_quiet = tk.Button(
            self.frame_mode_btns,
            text="Quiet",
            command=lambda: self.set_slider_mode("quiet"),
            bg="#cceeff",
            font=("Arial", 9),
        )
        self.btn_quiet.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=2)

        # --- 6. PID Calculation Results ---
        tk.Label(
            left_panel,
            text="6. PID Coefficients",
            font=("Arial", 12, "bold"),
            bg="#f0f0f0",
        ).pack(anchor=tk.W, pady=(20, 5))

        frame_res = tk.Frame(left_panel, bg="#f0f0f0")
        frame_res.pack(fill=tk.X, padx=10)

        tk.Label(
            frame_res,
            text="Rivera 1986 IMC PID (Ratio > 0.8):",
            font=("Arial", 12, "bold"),
            bg="#f0f0f0",
        ).pack(anchor=tk.W, pady=(5, 0))
        self.lbl_pid_res = tk.Label(
            frame_res,
            text="Kp: -   Ki: -   Kd: -",
            bg="white",
            relief=tk.SUNKEN,
            font=("Arial", 14, "bold"),
        )
        self.lbl_pid_res.pack(pady=2, fill=tk.X)

        tk.Label(
            frame_res,
            text="Rivera 1986 Improved PI (Ratio > 1.7):",
            font=("Arial", 12, "bold"),
            bg="#f0f0f0",
        ).pack(anchor=tk.W, pady=(10, 0))
        self.lbl_pi_res = tk.Label(
            frame_res,
            text="Kp: -   Ki: -",
            bg="white",
            relief=tk.SUNKEN,
            font=("Arial", 14, "bold"),
        )
        self.lbl_pi_res.pack(pady=2, fill=tk.X)

        # --- 7. System Log ---
        tk.Label(
            left_panel,
            text="7. System Log",
            font=("Arial", 12, "bold"),
            bg="#f0f0f0",
        ).pack(anchor=tk.W, pady=(20, 5))

        self.info_text = tk.Text(
            left_panel, height=8, width=40, font=("Courier", 9)
        )
        self.info_text.pack(padx=5, pady=5, fill=tk.BOTH, expand=True)
        self.info_text.config(state=tk.DISABLED)

        # Author Credits
        self.lbl_author = tk.Label(
            left_panel,
            text="       YouPeng, Wu (twpeng50606@gmail.com)  2026",
            font=("Arial", 9, "italic"),
            bg="#f0f0f0",
            fg="#666",
        )
        self.lbl_author.pack(side=tk.BOTTOM, anchor=tk.W, pady=(0, 10))

        # Matplotlib Figure (OO Interface)
        self.fig = Figure(figsize=(10, 6), dpi=100)
        self.ax = self.fig.add_subplot(111)

        # Canvas
        self.canvas = FigureCanvasTkAgg(self.fig, master=right_panel)
        self.canvas_widget = self.canvas.get_tk_widget()
        self.canvas_widget.pack(fill=tk.BOTH, expand=True)

        # Toolbar
        # self.toolbar = NavigationToolbar2Tk(self.canvas, right_panel)
        # self.toolbar.update()
        # self.canvas_widget.pack(fill=tk.BOTH, expand=True)

        # Credits Label (Bottom Right)
        credits_text = (
            "【 數值方法及驗證團隊 】\n\n"
            "Thermal RD : SamJX_Wang@compal.com\n"
            "Thermal RD : MaxCY_Lin@compal.com\n"
            "BMC RD     : YouPeng_Wu@compal.com\n\n"
            "--------------------------------------------------\n"
            "* 如果這個工具有問題　去找上面前兩位 *"
        )
        # self.lbl_credits = tk.Label(right_panel, text=credits_text, bg="#f9f9f9", fg="black", justify=tk.LEFT, font=("Consolas", 11, "bold"), relief=tk.RIDGE, bd=2, padx=20, pady=20)
        # self.lbl_credits.pack(side=tk.BOTTOM, anchor=tk.E, padx=20, pady=20)

    def set_slider_mode(self, mode):
        # Directly set virtual slider positions
        if mode == "perf":
            target_val = 0.5
        elif mode == "norm":
            target_val = 1.5
        elif mode == "quiet":
            target_val = 2.5
        else:
            return

        self.scale_epsilon.set(target_val)
        self.update_slider_color(target_val)
        self.update_pid_tuning()

    def update_mode_buttons(self):
        # Update button text with calculated ratios
        if not hasattr(self, "current_params") or not self.current_params:
            self.btn_perf.config(text="Performance")
            self.btn_norm.config(text="Normal")
            self.btn_quiet.config(text="Quiet")
            return

        tau = self.current_params.get("tau", 0)
        theta = self.current_params.get("theta", 0)
        if theta < 0.001:
            theta = 0.001

        # Ratios (epsilon / theta)
        # Performance: Epsilon = 0.2*tau + 0.5*theta
        # Normal: Epsilon = 1.0*tau
        # Quiet: Epsilon = 2.0*tau

        # Calculate actual Epsilon for display
        eps_perf = 0.2 * tau + 0.5 * theta
        eps_norm = 1.0 * tau
        eps_quiet = 2.0 * tau

        self.btn_perf.config(
            text=f"Performance\nε = 0.2τ + 0.5θ\n(ε={eps_perf:.3f})"
        )
        self.btn_norm.config(text=f"Normal\nε = 1.0τ\n(ε={eps_norm:.3f})")
        self.btn_quiet.config(text=f"Quiet\nε = 2.0τ\n(ε={eps_quiet:.3f})")

    # load_file moved to end of class

    def log_info(self, text):
        self.info_text.config(state=tk.NORMAL)
        self.info_text.insert(tk.END, text + "\n")
        self.info_text.config(state=tk.DISABLED)
        # Force UI update
        self.root.update_idletasks()

    def clear_info(self):
        self.info_text.config(state=tk.NORMAL)
        self.info_text.delete(1.0, tk.END)
        self.info_text.config(state=tk.DISABLED)

    def refresh_plot(self):
        if not hasattr(self, "plot_data"):
            return

        try:
            self.ax.clear()

            times = self.plot_data["times"]
            temps = self.plot_data["temps"]
            sensor_name = self.plot_data["sensor_name"]
            t_start = self.plot_data["t_start"]
            t_end = self.plot_data["t_end"]
            step_time = self.plot_data["step_time"]
            y0 = self.plot_data["y0"]
            delta_pwm = self.plot_data["delta_pwm"]

            # Raw Data
            if self.show_actual.get():
                self.ax.scatter(
                    times,
                    temps,
                    color="grey",
                    s=15,
                    alpha=0.6,
                    label="Actual Data",
                    zorder=2,
                )

            # Optimization (Red Solid) - Moved Up
            if self.show_opt.get() and self.params_opt:
                mt, mv = get_model_curve(
                    self.params_opt, t_start, t_end, step_time, y0, delta_pwm
                )
                label_str = f"Nelder-Mead (k={self.params_opt['k']:.3f}, tau={self.params_opt['tau']:.1f}, theta={self.params_opt['theta']:.1f})"
                self.ax.plot(
                    mt, mv, "r-", linewidth=2, label=label_str, zorder=5
                )

            # LSM (Green Solid)
            if self.show_lsm.get() and self.params_lsm:
                mt, mv = get_model_curve(
                    self.params_lsm, t_start, t_end, step_time, y0, delta_pwm
                )
                label_str = f"LSM (k={self.params_lsm['k']:.3f}, tau={self.params_lsm['tau']:.1f}, theta={self.params_lsm['theta']:.1f})"
                self.ax.plot(
                    mt, mv, "g-", linewidth=2, label=label_str, zorder=4
                )

            # Two Point (Blue Dashed) - Moved Down
            if self.show_632.get() and self.params_632:
                mt, mv = get_model_curve(
                    self.params_632, t_start, t_end, step_time, y0, delta_pwm
                )
                label_str = f"TwoPoint (k={self.params_632['k']:.3f}, tau={self.params_632['tau']:.1f}, theta={self.params_632['theta']:.1f})"
                self.ax.plot(
                    mt, mv, "b-", linewidth=2, label=label_str, zorder=3
                )

            # Styling
            # Styling - IEEE Journal Style
            # Fonts - Improved Font Selection with Fallback
            import matplotlib.font_manager as font_manager

            # Helper to find best available serif font
            def get_best_serif_font():
                available_fonts = set(
                    [f.name for f in font_manager.fontManager.ttflist]
                )
                preferred = [
                    "Times New Roman",
                    "Times",
                    "DejaVu Serif",
                    "Liberation Serif",
                    "serif",
                ]
                for p in preferred:
                    if p in available_fonts:
                        return p
                return "serif"  # Fallback to generic family

            font_name = get_best_serif_font()

            # Use specific dict for size/weight, but rely on resolved name
            title_font = {
                "family": "serif",
                "fontname": font_name,
                "size": 22,
                "weight": "bold",
            }
            label_font = {"family": "serif", "fontname": font_name, "size": 18}

            self.ax.set_title(
                f"{sensor_name} Step Response Analysis",
                fontdict=title_font,
                pad=20,
            )
            self.ax.set_xlabel("Time (s)", fontdict=label_font)
            self.ax.set_ylabel("Temperature (°C)", fontdict=label_font)
            self.ax.grid(True, linestyle="--", alpha=0.6)

            # Legend
            # borderpad=1.0 makes it "wider" (more padding)
            legend_font = font_manager.FontProperties(
                family="serif", style="normal", size=16
            )
            # Try to set specific font if available
            try:
                legend_font.set_family(font_name)
            except:
                pass

            self.ax.legend(
                prop=legend_font,
                frameon=True,
                facecolor="white",
                edgecolor="white",
                framealpha=1,
                borderpad=1.0,
                labelspacing=0.8,
            )

            # Ticks
            self.ax.tick_params(axis="both", which="major", labelsize=14)
            for label in self.ax.get_xticklabels() + self.ax.get_yticklabels():
                label.set_fontname(font_name)

            self.canvas.draw()
        except Exception as e:
            traceback.print_exc()

    def process_and_plot(self, filepath, auto_adjust_window=False):
        try:
            self.clear_info()

            filename = os.path.basename(filepath)
            sensor_name = filename.replace("step_trigger_", "").replace(
                ".txt", ""
            )
            self.root.title(f"FOPDT Analysis Tool - {sensor_name}")

            # 1. Parse Data
            self.log_info(f"Loading data: {filename}...")

            # Read Window Size
            try:
                window_size = int(self.entry_window.get())
            except ValueError:
                window_size = 30
                self.entry_window.delete(0, tk.END)
                self.entry_window.insert(0, "30")
                self.log_info("Invalid Window Size, using default 30")

            (
                times,
                temps,
                pwms,
                step_time,
                y0,
                delta_pwm,
                start_idx,
                y_final,
            ) = parse_data_file(filepath, 0.5, window_size)

            if times is None:
                messagebox.showerror("Error", "Failed to parse data file.")
                return

            # Update Hint Label & Auto Adjust
            if len(times) > 1:
                total_duration = times[-1] - times[0]
                avg_interval = total_duration / (len(times) - 1)
                if avg_interval > 0:
                    rec_win = int(round(60.0 / avg_interval))
                    self.lbl_win_hint.config(
                        text=f"Avg Interval: {avg_interval:.4f}s\nFor 1 min avg, set Window ~ {rec_win}"
                    )

                    if auto_adjust_window:
                        self.log_info(
                            f"Auto-adjusting Avg Window to {rec_win} (1 min avg)..."
                        )
                        # Update UI
                        self.entry_window.delete(0, tk.END)
                        self.entry_window.insert(0, str(rec_win))
                        window_size = rec_win

                        # Re-parse with new window size
                        (
                            times,
                            temps,
                            pwms,
                            step_time,
                            y0,
                            delta_pwm,
                            start_idx,
                            y_final,
                        ) = parse_data_file(filepath, 0.5, window_size)
                else:
                    self.lbl_win_hint.config(text="Avg Interval: N/A")
            else:
                self.lbl_win_hint.config(text="Avg Interval: N/A")

            # Filter for plotting
            plot_times = times[start_idx:]
            plot_temps = temps[start_idx:]

            if not plot_times:
                messagebox.showerror(
                    "Error", "No data found after step trigger."
                )
                return

            # Store plotting data for refresh
            self.plot_data = {
                "times": plot_times,
                "temps": plot_temps,
                "sensor_name": sensor_name,
                "t_start": plot_times[0],
                "t_end": plot_times[-1],
                "step_time": step_time,
                "y0": y0,
                "delta_pwm": delta_pwm,
            }

            # Debug Info
            self.log_info(f"--- Debug Data ---")
            self.log_info(f"Total Points: {len(temps)}")
            if len(temps) >= 5:
                self.log_info(f"Last 5 Temps: {temps[-5:]}")
            self.log_info(
                f"Calculated y_final (Avg {window_size}): {y_final:.4f}"
            )
            self.log_info(f"y0 (Initial, Avg {window_size}): {y0:.4f}")

            # 2. Identify FOPDT Parameters (Calculated Internally)
            # Convert delta_pwm (raw 0-255) to delta_duty (0-100%) for standard K units (deg/%)
            delta_duty = delta_pwm * 100.0 / 255.0

            self.log_info(
                f"Running Identification (Avg Window={window_size})..."
            )
            self.log_info(
                f"y0={y0:.2f}, y_final={y_final:.2f}, delta_pwm={delta_pwm}, delta_duty={delta_duty:.2f}%"
            )

            # Two Point (63.2%)
            try:
                self.params_632 = identify_two_point(
                    plot_times, plot_temps, step_time, y0, y_final, delta_duty
                )
                self.log_info(
                    f"TwoPoint: k={self.params_632['k']:.4f}, tau={self.params_632['tau']:.4f}, theta={self.params_632['theta']:.4f}"
                )
            except Exception as e:
                self.log_info(f"TwoPoint Error: {e}")
                self.params_632 = None

            # LSM
            try:
                self.params_lsm = identify_lsm(
                    plot_times, plot_temps, step_time, y0, y_final, delta_duty
                )
                self.log_info(
                    f"LSM: k={self.params_lsm['k']:.4f}, tau={self.params_lsm['tau']:.4f}, theta={self.params_lsm['theta']:.4f}"
                )
            except Exception as e:
                self.log_info(f"LSM Error: {e}")
                self.params_lsm = None

            # Optimization (Nelder-Mead)
            try:
                # Use Two Point results as initial guess if available, else standard guess
                guess = (
                    self.params_632
                    if self.params_632
                    else {
                        "k": (y_final - y0) / delta_duty,
                        "tau": 10,
                        "theta": 1,
                    }
                )
                self.params_opt = identify_nelder_mead(
                    plot_times, plot_temps, step_time, y0, delta_duty, guess
                )
                self.log_info(
                    f"Nelder-Mead: k={self.params_opt['k']:.4f}, tau={self.params_opt['tau']:.4f}, theta={self.params_opt['theta']:.4f}"
                )
            except Exception as e:
                self.log_info(f"Nelder-Mead Error: {e}")
                self.params_opt = None

            # 3. Method Selection Trigger
            self.on_method_change()

            # 4. Refresh Plot
            self.refresh_plot()

            # Default Selection for Tuning
            if self.params_opt:
                self.tuning_method_var.set("Nelder-Mead")
                self.current_params = self.params_opt
            elif self.params_lsm:
                self.tuning_method_var.set("LSM")
                self.current_params = self.params_lsm
            elif self.params_632:
                self.tuning_method_var.set("Two Point")
                self.current_params = self.params_632

            # Force update UI with new params
            if self.current_params:
                self.display_parameters(self.current_params)
                self.update_slider_range(
                    self.current_params
                )  # Fixed range 0-3
                self.scale_epsilon.set(1.5)  # Default to Normal (Center)
                self.update_mode_buttons()
                self.update_slider_color(1.5)
                self.update_pid_tuning()

            self.on_method_change()

        except Exception as e:
            traceback.print_exc()
            messagebox.showerror("Execution Error", f"An error occurred:\n{e}")

        except Exception as e:
            traceback.print_exc()
            messagebox.showerror("Execution Error", f"An error occurred:\n{e}")

    def on_window_change(self, event=None):
        if self.current_filepath:
            self.log_info(
                f"Reloading with Window Size: {self.entry_window.get()}"
            )
            self.process_and_plot(self.current_filepath)

    def show_load_menu(self):
        try:
            x = self.btn_load.winfo_rootx()
            y = self.btn_load.winfo_rooty() + self.btn_load.winfo_height()
            self.load_menu.tk_popup(x, y)
        finally:
            self.load_menu.grab_release()

    def load_files_click(self):
        filepaths = filedialog.askopenfilenames(
            filetypes=[("Text Files", "*.txt"), ("All Files", "*.*")]
        )
        if filepaths:
            selected_files = list(filepaths)

            # Smart Folder Detection
            if len(selected_files) == 1:
                single_file = selected_files[0]
                folder = os.path.dirname(single_file)
                all_txts = sorted(glob.glob(os.path.join(folder, "*.txt")))

                if len(all_txts) > 1:
                    # Ask user if they want to load the whole folder
                    msg = f"This folder contains {len(all_txts)} text files.\nDo you want to load all of them?"
                    if messagebox.askyesno("Load Folder?", msg):
                        self.loaded_files = all_txts
                        self.log_info(
                            f"Smart Load: Loaded {len(all_txts)} files from folder."
                        )
                    else:
                        self.loaded_files = selected_files
                else:
                    self.loaded_files = selected_files
            else:
                self.loaded_files = selected_files

            self.update_file_list()
            # Auto-select first one to trigger plot
            if self.loaded_files:
                self.lst_files.selection_clear(0, tk.END)
                self.lst_files.selection_set(0)
                self.on_file_select(None)

    def load_folder_click(self):
        folder = filedialog.askdirectory()
        if folder:
            # Sort files naturally
            files = sorted(glob.glob(os.path.join(folder, "*.txt")))
            if files:
                self.loaded_files = files
                self.update_file_list()
                self.log_info(f"Loaded {len(files)} files from folder.")

                # Auto-select first one
                if self.loaded_files:
                    self.lst_files.selection_clear(0, tk.END)
                    self.lst_files.selection_set(0)
                    self.on_file_select(None)
            else:
                self.log_info(f"No .txt files found in {folder}")

    def update_file_list(self):
        self.lst_files.delete(0, tk.END)
        for f in self.loaded_files:
            self.lst_files.insert(tk.END, os.path.basename(f))

    def on_file_select(self, event):
        selection = self.lst_files.curselection()
        if selection:
            idx = selection[0]
            if 0 <= idx < len(self.loaded_files):
                filepath = self.loaded_files[idx]
                # Always load if different, or maybe even if same (to reload)?
                # Let's check against current to avoid redundant processing if accidental click
                if filepath != self.current_filepath:
                    self.current_filepath = filepath
                    # Auto adjust window for consistency with user request
                    self.process_and_plot(filepath, auto_adjust_window=True)

    def on_method_change(self, event=None):
        method = self.tuning_method_var.get()

        # Update OptionMenu Image
        if method == "Nelder-Mead":
            self.opt_method.config(image=self.img_red)
        elif method == "LSM":
            self.opt_method.config(image=self.img_green)
        elif method == "Two Point":
            self.opt_method.config(image=self.img_blue)

        params = None
        if method == "Nelder-Mead":
            params = getattr(self, "params_opt", None)
        elif method == "LSM":
            params = getattr(self, "params_lsm", None)
        elif method == "Two Point":
            params = getattr(self, "params_632", None)

        if params:
            self.current_params = params
            self.display_parameters(params)
            self.update_slider_range(params)
            self.update_mode_buttons()

            val = self.scale_epsilon.get()
            self.update_slider_color(val)
            self.update_pid_tuning()

    def update_slider_range(self, params):
        # Slider now represents a VIRTUAL SCALE from 0.0 to 3.0
        # Zone 1 [0.0 - 1.0]: Performance (Center 0.5)
        # Zone 2 [1.0 - 2.0]: Normal (Center 1.5)
        # Zone 3 [2.0 - 3.0]: Quiet (Center 2.5)
        self.scale_epsilon.config(from_=0.0, to=3.0, resolution=0.01)
        # We don't necessarily reset here if we are just switching methods,
        # but process_and_plot calls it. Let's keep it 1.5 (Normal) by default in that flow.

    def _get_epsilon_from_slider(self, s_val):
        """Piecewise linear mapping from 0-3 slider to actual Epsilon."""
        if not hasattr(self, "current_params") or not self.current_params:
            return 1.0

        tau = self.current_params.get("tau", 0)
        theta = self.current_params.get("theta", 0)
        if theta < 0.001:
            theta = 0.1

        e_perf = 0.2 * tau + 0.5 * theta
        e_norm = 1.0 * tau
        e_quiet = 2.0 * tau

        # Ensure values are monotonic
        if e_perf > e_norm:
            e_perf = e_norm * 0.5
        if e_quiet < e_norm:
            e_quiet = e_norm * 2.0

        if s_val <= 0.5:
            # Map [0, 0.5] -> [0, e_perf]
            return (s_val / 0.5) * e_perf
        elif s_val <= 1.5:
            # Map [0.5, 1.5] -> [e_perf, e_norm]
            return e_perf + ((s_val - 0.5) / 1.0) * (e_norm - e_perf)
        elif s_val <= 2.5:
            # Map [1.5, 2.5] -> [e_norm, e_quiet]
            return e_norm + ((s_val - 1.5) / 1.0) * (e_quiet - e_norm)
        else:
            # Map [2.5, 3.0] -> [e_quiet, e_quiet * 1.5]
            return e_quiet + ((s_val - 2.5) / 0.5) * (e_quiet * 0.5)

    def _get_slider_from_epsilon(self, e_val):
        """Piecewise linear mapping from Epsilon to 0-3 slider."""
        if not hasattr(self, "current_params") or not self.current_params:
            return 1.5

        tau = self.current_params.get("tau", 0)
        theta = self.current_params.get("theta", 0)
        if theta < 0.001:
            theta = 0.1

        e_perf = 0.2 * tau + 0.5 * theta
        e_norm = 1.0 * tau
        e_quiet = 2.0 * tau

        if e_perf > e_norm:
            e_perf = e_norm * 0.5
        if e_quiet < e_norm:
            e_quiet = e_norm * 2.0

        if e_val <= e_perf:
            return (e_val / e_perf) * 0.5 if e_perf > 0 else 0
        elif e_val <= e_norm:
            return 0.5 + ((e_val - e_perf) / (e_norm - e_perf)) * 1.0
        elif e_val <= e_quiet:
            return 1.5 + ((e_val - e_norm) / (e_quiet - e_norm)) * 1.0
        else:
            return 2.5 + ((e_val - e_quiet) / (e_quiet * 0.5)) * 0.5

    def display_parameters(self, params):
        # Update text fields
        self.entry_k.config(state=tk.NORMAL)
        self.entry_tau.config(state=tk.NORMAL)
        self.entry_theta.config(state=tk.NORMAL)

        self.entry_k.delete(0, tk.END)
        self.entry_k.insert(0, f"{params['k']:.4f}")
        self.entry_tau.delete(0, tk.END)
        self.entry_tau.insert(0, f"{params['tau']:.4f}")
        self.entry_theta.delete(0, tk.END)
        self.entry_theta.insert(0, f"{params['theta']:.4f}")

        self.entry_k.config(state="readonly")
        self.entry_tau.config(state="readonly")
        self.entry_theta.config(state="readonly")

    def interpolate_color(self, val, min_val, max_val, colors):
        """
        colors: list of (stop_ratio, rgb_tuple)
        e.g. [(0.0, (173,216,230)), (0.5, (0,255,0)), (1.0, (255,0,0))]
        """
        if max_val - min_val == 0:
            return "#000000"

        # Clamp val to visual bounds
        if val < min_val:
            val = min_val
        if val > max_val:
            val = max_val

        ratio = (float(val) - min_val) / (max_val - min_val)
        ratio = max(0.0, min(1.0, ratio))

        # Find the segment
        lower = colors[0]
        upper = colors[-1]

        for i in range(len(colors) - 1):
            if colors[i][0] <= ratio <= colors[i + 1][0]:
                lower = colors[i]
                upper = colors[i + 1]
                break

        # Interpolate within segment
        # Local ratio
        seg_range = upper[0] - lower[0]
        if seg_range == 0:
            local_ratio = 0
        else:
            local_ratio = (ratio - lower[0]) / seg_range

        start_rgb = lower[1]
        end_rgb = upper[1]

        r = int(start_rgb[0] + (end_rgb[0] - start_rgb[0]) * local_ratio)
        g = int(start_rgb[1] + (end_rgb[1] - start_rgb[1]) * local_ratio)
        b = int(start_rgb[2] + (end_rgb[2] - start_rgb[2]) * local_ratio)
        return f"#{r:02x}{g:02x}{b:02x}"

    def update_slider_color(self, s_val):
        # Rich Gradient with depth (0.0 to 3.0 scale)
        # Red Zone (Perf): Dark Red -> Bright Red -> Orange/Red
        # Green Zone (Norm): Yellow/Green -> Emerald Green -> Sea Green
        # Blue Zone (Quiet): Cyan -> Sapphire Blue -> Deep Blue
        stops = [
            (0.00, (180, 0, 0)),  # 0.0: Dark Red
            (0.16, (255, 50, 50)),  # 0.5 center: Bright Red
            (0.33, (255, 100, 0)),  # 1.0 boundary: Orange
            (0.34, (180, 180, 0)),  # 1.0 transition: Yellow-ish
            (0.50, (50, 255, 50)),  # 1.5 center: Emerald Green
            (0.66, (0, 150, 100)),  # 2.0 boundary: Sea Green
            (0.67, (0, 180, 255)),  # 2.0 transition: Sky Blue
            (0.83, (50, 50, 255)),  # 2.5 center: Sapphire Blue
            (1.00, (0, 0, 120)),  # 3.0: Deep Night Blue
        ]
        color = self.interpolate_color(s_val, 0.0, 3.0, stops)
        try:
            self.scale_epsilon.config(troughcolor=color)
        except:
            pass

    def on_slider_change(self, s_val):
        # Trigger sync from slider
        self.update_pid_tuning()
        try:
            self.update_slider_color(float(s_val))
        except:
            pass

    def on_entry_change(self, event=None):
        # Update slider when text box changes
        try:
            epsilon = float(self.entry_epsilon.get())
            s_val = self._get_slider_from_epsilon(epsilon)

            # Clamp s_val to [0, 3]
            if s_val < 0:
                s_val = 0
            if s_val > 3:
                s_val = 3

            # This triggers on_slider_change which updates PID and labels
            self.scale_epsilon.set(s_val)
            self.update_slider_color(s_val)
        except ValueError:
            pass

    def update_pid_tuning(self, event=None):
        # Called when params change, method changes, or slider moves
        try:
            s_val = self.scale_epsilon.get()
            epsilon = self._get_epsilon_from_slider(s_val)

            # Update labels to ensure they match current params mapping
            self.entry_epsilon.delete(0, tk.END)
            self.entry_epsilon.insert(0, f"{epsilon:.3f}")
            self.lbl_eps_display.config(text=f"ε = {epsilon:.3f}")

            self.update_pid_tuning_from_val(epsilon)
        except:
            pass

    def update_pid_tuning_from_val(self, epsilon):
        try:
            if not hasattr(self, "current_params") or not self.current_params:
                return

            if epsilon < 0.001:
                epsilon = 0.001

            k = self.current_params.get("k", 0)
            tau = self.current_params.get("tau", 0)
            theta = self.current_params.get("theta", 0)

            # --- Update Ratio Display ---
            if theta < 0.001:
                self.lbl_ratio_val.config(text="theta ~ 0")
            else:
                ratio_val = epsilon / theta
                self.lbl_ratio_val.config(text=f"{ratio_val:.2f}")

            # --- 1. PID Controller (IMC) ---
            # formula uses original params but we must use effective_theta/epsilon for stability

            # Avoid division by zero or extreme gains if theta is 0
            effective_theta = theta
            if effective_theta < 0.1:
                effective_theta = 0.1

            numerator = 2.0 * tau + effective_theta
            denominator = k * (2.0 * epsilon + effective_theta)

            if abs(denominator) < 1e-9:
                kc = 0
            else:
                kc = numerator / denominator

            tau_i = tau + effective_theta / 2.0
            tau_d = (tau * effective_theta) / (2.0 * tau + effective_theta)

            kp = kc
            ki = kc / tau_i if abs(tau_i) > 1e-9 else 0
            kd = kc * tau_d

            self.lbl_pid_res.config(
                text=f"Kp: {kp:.5f}   Ki: {ki:.5f}   Kd: {kd:.5f}", fg="blue"
            )

            # --- 2. Improved PI Controller (IMC Improved) ---
            # Kc = (2*tau + theta) / (2*k*epsilon)
            # TauI = tau + theta/2

            denom_pi = 2.0 * k * epsilon
            if abs(denom_pi) < 1e-9:
                kc_pi = 0
            else:
                kc_pi = (2.0 * tau + effective_theta) / denom_pi

            tau_i_pi = tau + effective_theta / 2.0

            kp_pi = kc_pi
            ki_pi = kc_pi / tau_i_pi if abs(tau_i_pi) > 1e-9 else 0

            self.lbl_pi_res.config(
                text=f"Kp: {kp_pi:.5f}   Ki: {ki_pi:.5f}", fg="green"
            )

        except Exception as e:
            traceback.print_exc()
            self.lbl_pid_res.config(text="Error Calc")

    def save_plot(self):
        try:
            filename = filedialog.asksaveasfilename(
                title="Save Plot As",
                defaultextension=".png",
                filetypes=[
                    ("PNG Image", "*.png"),
                    ("JPEG Image", "*.jpg"),
                    ("All Files", "*.*"),
                ],
            )
            if filename:
                self.fig.savefig(filename, dpi=300, bbox_inches="tight")
                messagebox.showinfo("Success", f"Plot saved to:\n{filename}")
        except Exception as e:
            traceback.print_exc()
            messagebox.showerror("Error", f"Failed to save plot:\n{e}")

            # Avoid division by zero or extreme gains if theta is 0
            # If theta is negligible (e.g. LSM often gives 0), basic IMC formula produces infinite gain.
            # We treat theta as at least 0.1s for tuning stability in this edge case.
            effective_theta = theta
            if effective_theta < 0.1:
                effective_theta = 0.1

            # Epsilon is now direct
            # But wait, original code used epsilon = effective_theta * ratio
            # Here we already have Epsilon from user.

            # --- 1. PID Controller (IMC) ---
            # formula uses original params but we must use effective_theta/epsilon for stability

            numerator = 2.0 * tau + effective_theta
            denominator = k * (2.0 * epsilon + effective_theta)

            if abs(denominator) < 1e-9:
                kc = 0
            else:
                kc = numerator / denominator

            tau_i = tau + effective_theta / 2.0
            tau_d = (tau * effective_theta) / (2.0 * tau + effective_theta)

            kp = kc
            ki = kc / tau_i if abs(tau_i) > 1e-9 else 0
            kd = kc * tau_d

            self.lbl_pid_res.config(
                text=f"Kp: {kp:.5f},  Ki: {ki:.5f},  Kd: {kd:.5f}", fg="blue"
            )

            # --- 2. Improved PI Controller (IMC Improved) ---
            # Kc = (2*tau + theta) / (2*k*epsilon)

            denom_pi = 2.0 * k * epsilon
            if abs(denom_pi) < 1e-9:
                kc_pi = 0
            else:
                kc_pi = (2.0 * tau + effective_theta) / denom_pi

            tau_i_pi = tau + effective_theta / 2.0

            kp_pi = kc_pi
            ki_pi = kc_pi / tau_i_pi if abs(tau_i_pi) > 1e-9 else 0

            self.lbl_pi_res.config(
                text=f"Kp: {kp_pi:.5f},  Ki: {ki_pi:.5f}", fg="green"
            )

            # Log to ensure it's working
            # print(f"Update Tuning: Ratio={ratio}, Kp={kp}")

        except Exception as e:
            traceback.print_exc()
            self.lbl_pid_res.config(text="Error Calc")


if __name__ == "__main__":
    root = tk.Tk()
    app = FOPDTApp(root)
    root.mainloop()
