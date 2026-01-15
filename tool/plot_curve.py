import matplotlib.pyplot as plt
import os
import glob
import re

def process_file(filename):
    print(f"Processing {filename}...")
    
    # Extract sensor name from filename (e.g., plot_CPU0_TEMP.txt -> CPU0_TEMP)
    # Assuming format is plot_{SENSOR_NAME}.txt
    match = re.search(r'plot_(.+)\.txt', filename)
    if match:
        sensor_name = match.group(1)
    else:
        sensor_name = os.path.splitext(filename)[0] # Fallback
        
    output_filename = f"{sensor_name.lower()}_plot.png"

    n_list = []
    pwm_list = []
    temp_list = []

    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
            # Skip header if it exists and looks like text
            start_idx = 0
            if lines and lines[0].strip().startswith('n'):
                start_idx = 1
                
            for line in lines[start_idx:]:
                parts = line.split()
                if len(parts) >= 3:
                    n_list.append(float(parts[0]))
                    # Convert PWM from 0-255 to 0-100
                    pwm_list.append(float(parts[1]) / 255.0 * 100.0)
                    temp_list.append(float(parts[2]))

        # Find where PWM rises (assuming simple step up)
        if not pwm_list:
            print(f"  No data found in {filename}.")
            return

        initial_pwm = pwm_list[0]
        rise_index = -1
        for i, val in enumerate(pwm_list):
            if val > initial_pwm:
                rise_index = i
                break
        
        if rise_index != -1:
            print(f"  PWM rise detected at index {rise_index} (n={n_list[rise_index]})")
            start_idx_slice = max(0, rise_index - 50)
            end_idx_slice = min(len(n_list), rise_index + 200)
            
            n_list = n_list[start_idx_slice:end_idx_slice]
            pwm_list = pwm_list[start_idx_slice:end_idx_slice]
            temp_list = temp_list[start_idx_slice:end_idx_slice]
        else:
            print("  No PWM rise detected, plotting all data.")

        # Create the plot
        fig, ax1 = plt.subplots(figsize=(12, 6))

        color = 'tab:red'
        ax1.set_xlabel('Time step')
        ax1.set_ylabel('Temperature (°C)', color=color)
        ax1.plot(n_list, temp_list, color=color, linewidth=1.5, label=sensor_name)
        ax1.tick_params(axis='y', labelcolor=color)
        ax1.grid(True, linestyle='--', alpha=0.7)

        # Instantiate a second axes that shares the same x-axis
        ax2 = ax1.twinx()  
        color = 'tab:blue'
        ax2.set_ylabel('Duty (%)', color=color)  # we already handled the x-label with ax1
        ax2.plot(n_list, pwm_list, color=color, linewidth=1.5, linestyle='-', label='Duty')
        ax2.tick_params(axis='y', labelcolor=color)
        ax2.set_ylim(0, 100) # Set range to 0-100

        plt.title(f'{sensor_name} Temperature vs Duty')
        
        # Combine legends from both axes
        lines1, labels1 = ax1.get_legend_handles_labels()
        lines2, labels2 = ax2.get_legend_handles_labels()
        # Place legend below the plot
        ax1.legend(lines1 + lines2, labels1 + labels2, loc='upper center', bbox_to_anchor=(0.5, -0.15), ncol=2, frameon=False)

        # Adjust layout to make room for the legend
        plt.subplots_adjust(bottom=0.2)
        
        plt.savefig(output_filename)
        print(f"  Successfully saved plot to {output_filename}")
        plt.close(fig) # Close the figure to free memory

    except Exception as e:
        print(f"  An error occurred processing {filename}: {e}")

# Find all matching files
files = glob.glob('plot_*.txt')

if not files:
    print("No 'plot_*.txt' files found in the current directory.")
else:
    print(f"Found {len(files)} file(s).")
    for f in files:
        process_file(f)
