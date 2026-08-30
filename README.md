# Arktop

Arktop is a lightweight terminal process monitor for macOS
cleaner version of `top`

version 0.1 is out -> a little out of scapes but better versions will arrive soon 
hopefully will be able to make it clean

maybe make a even smaller version of arktop that just shows the ram and cpu processes only 
maybe the swap ram too but that's all

It is written in **C++** and uses macOS system APIs to display process
and system resource information.

## Features

-   View running processes
-   Per-process CPU usage
-   Per-process memory usage
-   System-wide CPU usage
-   System-wide RAM usage
-   System-wide swap usage
-   CPU, RAM, and swap usage bars
-   Sort processes by:
    -   CPU
    -   Memory
    -   PID
    -   Name
-   Move through processes
-   Send `SIGTERM` to a selected process
-   Terminal-based interface

## Requirements

Arktop currently targets **macOS**.

You need:

-   macOS
-   Xcode Command Line Tools
-   `clang++`
-   A terminal with Unicode/block-character support

Check that `clang++` is installed:

``` bash
clang++ --version
```

If it is not installed, install Apple's Command Line Tools:

``` bash
xcode-select --install
```

## Project Structure

The project is currently organized like this:

``` text
arktop/
├── src/
│   ├── main.cpp
│   ├── MacProcessProvider.cpp
│   ├── MacProcessProvider.hpp
│   ├── Terminal.cpp
│   └── Terminal.hpp
└── README.md
```

## Build

From the root of the project:

``` bash
clang++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Wpedantic \
  src/main.cpp \
  src/MacProcessProvider.cpp \
  src/Terminal.cpp \
  -o arktop-v0.1 
```

replace arktop(-v0.1) this version type with any version name you want

This creates the executable:

``` text
./arktop-v0.1
```

## Run

After building:

``` bash
./arktop
```

You should see something similar to:

``` text
 ARKTOP   Processes: 410   |   Sort: CPU
        CPU [██████████████░░░░░░░░░]  62.4%
        RAM [████████████████░░░░░░░]  78.1%  12.5 GB / 16.0 GB
        SWP [██░░░░░░░░░░░░░░░░░░░░░]   4.2%   0.7 GB / 16.0 GB

q:Quit   j/k:Move   k:Kill   c:CPU   m:Memory   p:PID   n:Name   r:Refresh

PID      NAME                             CPU%     MEM%       MEMORY STATE
-------- -------------------------------- -------- -------- ------------ --------
64560    Browser Helper (Renderer)          0.2%     4.23%     346.5 MB RUN
...
```

## Controls

  Key   Action
  ----- -----------------------
  `q`   Quit Arktop
  `j`   Move selection down
  `k`   Kill selected process
  `c`   Sort by CPU
  `m`   Sort by memory
  `p`   Sort by PID
  `n`   Sort by process name
  `r`   Refresh

### Important

The `k` key sends `SIGTERM` to the selected process.

Be careful when using it on system processes. Some processes may require
elevated privileges or may automatically restart.

## Development

For development, compile with warnings enabled:

``` bash
clang++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Wpedantic \
  src/main.cpp \
  src/MacProcessProvider.cpp \
  src/Terminal.cpp \
  -o arktop
```

Then run:

``` bash
./arktop
```

A useful development loop is:

``` bash
clang++ -std=c++17 -Wall -Wextra -Wpedantic \
  src/main.cpp \
  src/MacProcessProvider.cpp \
  src/Terminal.cpp \
  -o arktop && ./arktop
```

## Current Architecture

Arktop is currently split into three main pieces:

### `main.cpp`

Responsible for:

-   Main application loop
-   Rendering
-   Sorting
-   Keyboard controls
-   CPU/RAM/swap dashboard
-   Process selection

### `MacProcessProvider.cpp/.hpp`

Responsible for retrieving process information from macOS.

### `Terminal.cpp/.hpp`

Responsible for terminal setup, cursor movement, keyboard input, screen
handling, and restoring the terminal when Arktop exits.

## Future Improvements

Possible next steps for Arktop:

-   Separate process collection from the UI refresh loop
-   Reduce CPU usage of Arktop itself
-   Add scrolling through large process lists
-   Add arrow-key navigation
-   Add `F9`/function-key process controls
-   Add process detail view
-   Add per-core CPU usage
-   Add CPU temperature
-   Add system load average
-   Add uptime
-   Add network usage
-   Add disk usage
-   Add configurable refresh rate
-   Add colors based on CPU/memory usage
-   Add a proper CMake build system
-   Add process search/filtering
-   Add a configuration file

## License

This project is currently a personal project. Add a license here if you
decide to publish Arktop.



