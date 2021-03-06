# pd-externals
This package is a collection of various Puredata externals for signal processing.

Each class is accompanied with a help file. Open this file in Puredata for usage information and details.

- **bleprect~** bandlimited rectangle oscillator with hard sync
- **blepsaw~** bandlimited sawtooth oscillator with hard sync
- **bleptri~** bandlimited triangle oscillator with hard sync
- **bbd~** digital model of the analog bucket brigade delay (BBD)
- **limit~** limiter
- **robot~** robotic sound effect
- **lfos~** array of LFOs with fixed relative phase offsets
- **sincos** combined computation of sine and cosine (faster)
- **tri~** primitive triangle oscillator
- **miditranspose** transposition of MIDI note events
- **midiselect** MIDI event selection by channel and type
- **delayA~** allpass delay line
- **nlcubic~** cubic non-linearity
- **dcremove~** DC offset remover
- **opl3~** emulation of the Yamaha OPL3 with MIDI driver

## Usage instructions

Check out the source repository with submodules, and add the directory to the search paths of Puredata (under Edit → Preferences → Path).

Build the software by entering these commands.
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

## License information

The source code is Boost licensed, with exception of externals which are adaptations of existing open source software.

In cases noted below, the code retains the license of the project it originates from.

- **src/dafx/bbd~.cc**, **src/stk/** are MIT licensed
- **src/fons/**, **src/blepvco/**, **src/swh/** are GPLv2 or later
- **src/chip/opl3/driver/** is LGPLv2
- **src/chip/opl3/nukedopl/** is GPLv2 or later
