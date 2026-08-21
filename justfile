# link — Flipper FAP, USB MIDI clock for a Game Boy

set dotenv-load := false

export PATH := env_var("HOME") + "/.local/bin:" + env_var("PATH")

default:
    @just --list --justfile {{source_file()}}

# Download/update official Flipper SDK (OFW)
sdk:
    ufbt update

# Build dist/link.fap
build:
    ufbt

# Build, upload, and launch (quit qFlipper first)
launch:
    ufbt launch

# Compile only, print SDK target
check:
    ufbt
