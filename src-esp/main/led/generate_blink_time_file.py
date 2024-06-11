import sys
import os

total_time_ms = 10_000
min_time_ms = 110
# This is basically halfing the time each time it blinks, thus is in a geometric series
# https://www.youtube.com/watch?v=zRKZ0-kOUZM
geometric_progression = 1 / 0.5

# Subtracting some time after the first blink
arithmetic_progression = 70

is_geometric = False


def main(output_file):
    current_time = 0
    current_blink = min_time_ms
    while current_time < total_time_ms:
        if is_geometric:
            current_blink = int(current_blink * geometric_progression)
        else:
            current_blink = current_blink + arithmetic_progression
        current_time += current_blink
        print(f"    {current_blink},")

    print(f"const uint32_t blink_time = {current_blink};")
    with open(output_file, "w") as f:
        f.write("#pragma once\n\n")
        f.write("//This is an auto generated file, look at generate_blink_time.py\n\n")
        f.write("#include <inttypes.h> \n\n")
        f.write(f"const uint32_t initial_blink_period = {current_blink};\n")
        f.write(f"const uint32_t decrease_blink_duration = {arithmetic_progression};\n")
        f.write(f"const uint32_t min_blink_period = {min_time_ms};\n")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python generate.py <output_header_file>")
        script_path = sys.argv[0]
        script_dir = os.path.dirname(script_path)
        output_file = os.path.join(script_dir, "blink_time.h")
    else:
        output_file = sys.argv[1]
    main(output_file)
