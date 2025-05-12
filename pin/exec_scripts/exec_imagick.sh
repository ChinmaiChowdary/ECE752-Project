#!/bin/bash

# Determine the directory of the script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Change to the exe directory
cd "$SCRIPT_DIR" || { echo "Failed to change directory to $SCRIPT_DIR"; exit 1; }

# Define the common input for the tools
#INPUT="./imagick_s_base.mytest-m64 -limit disk 0 ../data/refspeed/input/refspeed_input.tga -resize 817% -rotate -2.76 -shave 540x375 -alpha remove -auto-level -contrast-stretch 1x1% -colorspace Lab -channel R -equalize +channel +channel -colorspace sRGB -define histogram:unique-colors=false -adaptive-blur 0x5 -despeckle -auto-gamma -adaptive-sharpen 55 -enhance -brightness-contrast 10x10 -resize 30% train_output.tga"
INPUT="./imagick_s_base.mytest-m64 -limit disk 0 train_input.tga -resize 320x240 -shear 31 -edge 140 -negate -flop -resize 900x900 -edge 10 train_output.tga"

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
