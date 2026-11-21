import argparse
import csv
import glob
from pathlib import Path

import matplotlib.pyplot as plt


def find_latest_csv():
    files = glob.glob("H100 Test 1 copy.csv")
    if not files:
        raise FileNotFoundError("No testfire_data_*.csv file found in the current directory")
    return max(files, key=lambda name: Path(name).stat().st_mtime)


def load_csv(filename):
    elapsed_seconds = []
    thrust_values = []
    pressure_values = []

    with open(filename, "r", newline="", encoding="utf-8") as csv_file:
        reader = csv.reader(csv_file)

        for line_number, row in enumerate(reader, start=1):
            if len(row) < 3:
                continue

            try:
                timestamp = float(row[0].strip())
                thrust = float(row[1].strip())
                pressure = float(row[2].strip())
            except ValueError:
                print(f"Skipping invalid row {line_number}: {row}")
                continue

            elapsed_seconds.append(timestamp)
            thrust_values.append(thrust)
            pressure_values.append(pressure)

    if not elapsed_seconds:
        raise ValueError("No valid sensor rows found")

    start_time = elapsed_seconds[0]
    elapsed_seconds = [timestamp - start_time for timestamp in elapsed_seconds]

    return elapsed_seconds, thrust_values, pressure_values


def main():
    parser = argparse.ArgumentParser(description="Graph thrust test stand CSV data")
    parser.add_argument("csv_file", nargs="?", help="CSV file to graph")
    args = parser.parse_args()

    csv_file = args.csv_file or find_latest_csv()
    time_seconds, thrust, pressure = load_csv(csv_file)

    fig, (thrust_axis, pressure_axis) = plt.subplots(
        2,
        1,
        figsize=(12, 8),
        sharex=True,
    )

    thrust_axis.plot(time_seconds, thrust, label="Thrust", color="tab:red")
    thrust_axis.set_ylabel("Thrust")
    thrust_axis.grid(True, alpha=0.3)
    thrust_axis.legend()

    pressure_axis.plot(time_seconds, pressure, label="Pressure", color="tab:blue")
    pressure_axis.set_xlabel("Time after logging started (seconds)")
    pressure_axis.set_ylabel("Pressure (PSI)")
    pressure_axis.grid(True, alpha=0.3)
    pressure_axis.legend()

    fig.suptitle(f"Thrust Test Stand: {csv_file}")
    fig.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
