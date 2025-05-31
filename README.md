# Launch Pad Control

A web-based control system for a model rocket launch pad, with support for either Arduino Nano RP2040 Connect or Arduino Nano ESP32.

## Features

- Web interface with buttons for launch, abort, open clamps, and close clamps
- Automatic countdown and launch sequence
- Status display for each action
- Buzzer and LED feedback

## Setup

1. Clone this repository to your local machine
2. Edit the HTML file in `data/index.html` to customize the UI
3. Run the `build.py` script
4. Change your environment to either `esp32` for ESP32 or `rp2040` for RP2040
5. Upload the sketch to your board
6. Connect to the board's network and navigate to `192.168.4.1`

## Usage

Use the Launch Pad Controller to:

- Initiate a launch sequence
- Abort a launch sequence
- Open the launch clamps
- Close the launch clamps
