# Super Mario Bros - Terminal Edition

A classic Mario-style platformer game written in C++ that runs in your terminal!

## Features

- Classic platformer gameplay with Mario
- Jump on platforms, collect coins, and stomp Goombas
- Smooth scrolling levels with parallax backgrounds
- Score tracking and lives system
- Colorful ASCII graphics using ANSI colors

## Requirements

- C++17 compatible compiler (g++ recommended)
- POSIX-compatible terminal (Linux, macOS, WSL)
- Terminal with ANSI color support

## Building

```bash
# Build the game
make

# Or compile directly
g++ -std=c++17 -Wall -O2 -o mario mario.cpp
```

## Running

```bash
# Using make
make run

# Or run directly
./mario
```

## Controls

| Key | Action |
|-----|--------|
| `A` / `←` | Move left |
| `D` / `→` | Move right |
| `W` / `↑` / `Space` | Jump |
| `Q` | Quit game |

## Gameplay

- **Objective**: Reach the flag at the end of the level
- **Coins**: Collect coins for 200 points each
- **Question Blocks**: Hit from below to get coins (100 points)
- **Bricks**: Can be broken by hitting from below (10 points)
- **Goombas**: Stomp them from above for 100 points, or they'll hurt you!
- **Pipes**: Obstacles to jump over
- **Flag**: Reach it to win (+1000 points)

## Tips

- Time your jumps carefully over gaps
- Stomp enemies by landing on top of them
- Don't fall into pits - you'll lose a life!
- You start with 3 lives

## Screenshots

```
   SCORE: 1200     COINS: 5     LIVES: 3     WORLD 1-1

         (__)                    (__)
        (____)                  (____)

                    [?][?]
                     o
    @>              ====
    A     ====
   ========================   &&   ===========================
   ########################   ww   ###########################
```

## License

This is a fan-made tribute game for educational purposes.
