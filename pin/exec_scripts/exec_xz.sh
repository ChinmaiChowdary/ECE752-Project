#!/bin/bash

# Determine the directory of the script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Change to the exe directory
cd "$SCRIPT_DIR" || { echo "Failed to change directory to $SCRIPT_DIR"; exit 1; }

# Define the common input for the tools
INPUT="./xz_s_base.mytest-m64 ../data/all/input/input.combined.xz 40 a841f68f38572a49d86226b7ff5baeb31bd19dc637a922a972b2e6d1257a890f6a544ecab967c313e370478c74f760eb229d4eef8a8d2836d233d3e9dd1430bf 6356684 -1 8"

# Check if the command argument is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <pin-command>"
    echo "Example: $0 \"$PIN_ROOT/pin -t $PIN_TOOLS/SimpleExamples/obj-intel64/d_TLB.so\""
    exit 1
fi

# Assign the provided argument to a variable
PIN_COMMAND="$1"

# Run the provided command with the specified input
$PIN_COMMAND -- $INPUT
