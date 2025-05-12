#!/bin/bash

# Determine the directory of the script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Change to the exe directory
cd "$SCRIPT_DIR" || { echo "Failed to change directory to $SCRIPT_DIR"; exit 1; }

# Define the common input for the tools
INPUT="./ldecod_s_base.mytest-m64 -i BuckBunny.264 -o BuckBunny.yuv"

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
