/*
 * Super Mario Bros - Terminal Edition
 * A simple Mario platformer game in C++
 *
 * Controls:
 *   A/Left Arrow  - Move left
 *   D/Right Arrow - Move right
 *   W/Space/Up    - Jump
 *   Q             - Quit game
 */

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

// ==================== Constants ====================
const int SCREEN_WIDTH = 80;
const int SCREEN_HEIGHT = 24;
const float GRAVITY = 0.5f;
const float JUMP_FORCE = -2.0f;
const float MOVE_SPEED = 1.0f;
const int FRAME_DELAY_MS = 50;

// ==================== Colors (ANSI) ====================
namespace Color {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BRIGHT_RED = "\033[91m";
    const std::string BRIGHT_GREEN = "\033[92m";
    const std::string BRIGHT_YELLOW = "\033[93m";
    const std::string BRIGHT_BLUE = "\033[94m";
    const std::string BG_BLUE = "\033[44m";
    const std::string BG_GREEN = "\033[42m";
    const std::string BG_RED = "\033[41m";
    const std::string BG_YELLOW = "\033[43m";
}

// ==================== Utility Functions ====================
void clearScreen() {
    std::cout << "\033[2J\033[H";
}

void moveCursor(int x, int y) {
    std::cout << "\033[" << (y + 1) << ";" << (x + 1) << "H";
}

void hideCursor() {
    std::cout << "\033[?25l";
}

void showCursor() {
    std::cout << "\033[?25h";
}

// Non-blocking keyboard input
class Keyboard {
private:
    struct termios oldSettings;
    bool initialized = false;

public:
    void init() {
        tcgetattr(STDIN_FILENO, &oldSettings);
        struct termios newSettings = oldSettings;
        newSettings.c_lflag &= ~(ICANON | ECHO);
        newSettings.c_cc[VMIN] = 0;
        newSettings.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
        initialized = true;
    }

    void restore() {
        if (initialized) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
        }
    }

    int getKey() {
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            // Handle escape sequences (arrow keys)
            if (c == 27) {
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) > 0 && seq[0] == '[') {
                    if (read(STDIN_FILENO, &seq[1], 1) > 0) {
                        switch (seq[1]) {
                            case 'A': return 'w'; // Up
                            case 'B': return 's'; // Down
                            case 'C': return 'd'; // Right
                            case 'D': return 'a'; // Left
                        }
                    }
                }
                return 27; // ESC
            }
            return c;
        }
        return -1;
    }
};

// ==================== Game Objects ====================

struct Vector2 {
    float x, y;
    Vector2(float x = 0, float y = 0) : x(x), y(y) {}
};

struct Rectangle {
    float x, y, width, height;
    Rectangle(float x = 0, float y = 0, float w = 1, float h = 1)
        : x(x), y(y), width(w), height(h) {}

    bool intersects(const Rectangle& other) const {
        return x < other.x + other.width &&
               x + width > other.x &&
               y < other.y + other.height &&
               y + height > other.y;
    }
};

enum class EntityType {
    PLATFORM,
    COIN,
    GOOMBA,
    BRICK,
    QUESTION_BLOCK,
    PIPE,
    FLAG
};

struct Entity {
    Rectangle rect;
    EntityType type;
    bool active = true;
    float velX = 0;
    int animFrame = 0;

    Entity(float x, float y, float w, float h, EntityType t)
        : rect(x, y, w, h), type(t) {}
};

class Mario {
public:
    Vector2 pos;
    Vector2 vel;
    float width = 2;
    float height = 2;
    bool onGround = false;
    bool facingRight = true;
    int animFrame = 0;
    int lives = 3;
    bool big = false;
    bool invincible = false;
    int invincibleTimer = 0;

    Mario(float x, float y) : pos(x, y), vel(0, 0) {}

    Rectangle getRect() const {
        return Rectangle(pos.x, pos.y, width, height);
    }

    void jump() {
        if (onGround) {
            vel.y = JUMP_FORCE;
            onGround = false;
        }
    }

    void moveLeft() {
        // Set velocity directly for responsive controls
        vel.x = -MOVE_SPEED;
        facingRight = false;
    }

    void moveRight() {
        // Set velocity directly for responsive controls
        vel.x = MOVE_SPEED;
        facingRight = true;
    }

    void applyFriction() {
        // Gradual slowdown when no input
        vel.x *= 0.8f;
        if (std::abs(vel.x) < 0.1f) vel.x = 0;
    }

    void stop() {
        vel.x = 0;
    }

    void update() {
        // Apply gravity
        vel.y += GRAVITY;
        if (vel.y > 3.0f) vel.y = 3.0f; // Terminal velocity

        // Update position
        pos.x += vel.x;
        pos.y += vel.y;

        // Animation
        if (vel.x != 0) {
            animFrame = (animFrame + 1) % 4;
        }

        // Invincibility timer
        if (invincible) {
            invincibleTimer--;
            if (invincibleTimer <= 0) {
                invincible = false;
            }
        }
    }

    void takeDamage() {
        if (!invincible) {
            if (big) {
                big = false;
                height = 2;
                invincible = true;
                invincibleTimer = 60;
            } else {
                lives--;
                invincible = true;
                invincibleTimer = 60;
            }
        }
    }
};

// ==================== Level Class ====================

class Level {
public:
    std::vector<Entity> entities;
    float scrollX = 0;
    int levelWidth = 200;
    Vector2 startPos;
    Vector2 flagPos;

    Level() {
        buildLevel1();
    }

    void buildLevel1() {
        entities.clear();
        startPos = Vector2(5, 18);
        flagPos = Vector2(190, 10);

        // Ground
        for (int i = 0; i < 70; i++) {
            entities.emplace_back(i * 3, 21, 3, 3, EntityType::PLATFORM);
        }

        // Gap in ground
        // (gap from 70*3=210 to 75*3=225 - but let's make it shorter)

        // More ground after gap
        for (int i = 75; i < 100; i++) {
            entities.emplace_back(i * 3 - 15, 21, 3, 3, EntityType::PLATFORM);
        }

        // Floating platforms
        entities.emplace_back(20, 15, 4, 1, EntityType::PLATFORM);
        entities.emplace_back(30, 13, 4, 1, EntityType::PLATFORM);
        entities.emplace_back(45, 15, 6, 1, EntityType::PLATFORM);
        entities.emplace_back(60, 12, 4, 1, EntityType::PLATFORM);
        entities.emplace_back(80, 14, 5, 1, EntityType::PLATFORM);
        entities.emplace_back(100, 13, 4, 1, EntityType::PLATFORM);
        entities.emplace_back(120, 15, 6, 1, EntityType::PLATFORM);
        entities.emplace_back(140, 12, 4, 1, EntityType::PLATFORM);
        entities.emplace_back(160, 14, 5, 1, EntityType::PLATFORM);

        // Question blocks with coins
        entities.emplace_back(25, 12, 2, 2, EntityType::QUESTION_BLOCK);
        entities.emplace_back(50, 10, 2, 2, EntityType::QUESTION_BLOCK);
        entities.emplace_back(85, 11, 2, 2, EntityType::QUESTION_BLOCK);
        entities.emplace_back(110, 10, 2, 2, EntityType::QUESTION_BLOCK);
        entities.emplace_back(145, 11, 2, 2, EntityType::QUESTION_BLOCK);

        // Bricks
        entities.emplace_back(27, 12, 2, 2, EntityType::BRICK);
        entities.emplace_back(29, 12, 2, 2, EntityType::BRICK);
        entities.emplace_back(52, 10, 2, 2, EntityType::BRICK);
        entities.emplace_back(54, 10, 2, 2, EntityType::BRICK);
        entities.emplace_back(87, 11, 2, 2, EntityType::BRICK);

        // Coins
        entities.emplace_back(22, 13, 1, 1, EntityType::COIN);
        entities.emplace_back(35, 11, 1, 1, EntityType::COIN);
        entities.emplace_back(48, 13, 1, 1, EntityType::COIN);
        entities.emplace_back(65, 10, 1, 1, EntityType::COIN);
        entities.emplace_back(90, 12, 1, 1, EntityType::COIN);
        entities.emplace_back(105, 11, 1, 1, EntityType::COIN);
        entities.emplace_back(125, 13, 1, 1, EntityType::COIN);
        entities.emplace_back(150, 10, 1, 1, EntityType::COIN);
        entities.emplace_back(170, 12, 1, 1, EntityType::COIN);

        // Goombas
        entities.emplace_back(40, 19, 2, 2, EntityType::GOOMBA);
        entities.emplace_back(70, 19, 2, 2, EntityType::GOOMBA);
        entities.emplace_back(95, 19, 2, 2, EntityType::GOOMBA);
        entities.emplace_back(130, 19, 2, 2, EntityType::GOOMBA);
        entities.emplace_back(165, 19, 2, 2, EntityType::GOOMBA);

        // Set goomba movement
        for (auto& e : entities) {
            if (e.type == EntityType::GOOMBA) {
                e.velX = -0.3f;
            }
        }

        // Pipes
        entities.emplace_back(55, 17, 3, 4, EntityType::PIPE);
        entities.emplace_back(115, 15, 3, 6, EntityType::PIPE);
        entities.emplace_back(155, 17, 3, 4, EntityType::PIPE);

        // Flag at end
        entities.emplace_back(flagPos.x, flagPos.y, 1, 11, EntityType::FLAG);
    }
};

// ==================== Renderer ====================

class Renderer {
private:
    std::vector<std::string> screenBuffer;
    std::vector<std::string> colorBuffer;

public:
    Renderer() {
        screenBuffer.resize(SCREEN_HEIGHT, std::string(SCREEN_WIDTH, ' '));
        colorBuffer.resize(SCREEN_HEIGHT, std::string(SCREEN_WIDTH, ' '));
    }

    void clear() {
        for (auto& line : screenBuffer) {
            std::fill(line.begin(), line.end(), ' ');
        }
        for (auto& line : colorBuffer) {
            std::fill(line.begin(), line.end(), ' ');
        }
    }

    void setChar(int x, int y, char c, char color = 'w') {
        if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
            screenBuffer[y][x] = c;
            colorBuffer[y][x] = color;
        }
    }

    void drawString(int x, int y, const std::string& str, char color = 'w') {
        for (size_t i = 0; i < str.length(); i++) {
            setChar(x + i, y, str[i], color);
        }
    }

    void drawMario(const Mario& mario, float scrollX) {
        int screenX = static_cast<int>(mario.pos.x - scrollX);
        int screenY = static_cast<int>(mario.pos.y);

        char color = 'r'; // Red
        if (mario.invincible && mario.invincibleTimer % 4 < 2) {
            return; // Blinking effect
        }

        if (mario.big) {
            // Big Mario (3 tall)
            if (mario.facingRight) {
                setChar(screenX, screenY - 1, 'O', color);     // Head
                setChar(screenX, screenY, '|', color);         // Body
                setChar(screenX + 1, screenY, '>', color);     // Arm
                setChar(screenX, screenY + 1, '^', color);     // Legs
                setChar(screenX + 1, screenY + 1, '^', color);
            } else {
                setChar(screenX + 1, screenY - 1, 'O', color);
                setChar(screenX + 1, screenY, '|', color);
                setChar(screenX, screenY, '<', color);
                setChar(screenX, screenY + 1, '^', color);
                setChar(screenX + 1, screenY + 1, '^', color);
            }
        } else {
            // Small Mario
            if (mario.facingRight) {
                setChar(screenX, screenY, '@', color);         // Head/body
                setChar(screenX + 1, screenY, '>', color);     // Direction
                setChar(screenX, screenY + 1, 'A', color);     // Legs
            } else {
                setChar(screenX + 1, screenY, '@', color);
                setChar(screenX, screenY, '<', color);
                setChar(screenX + 1, screenY + 1, 'A', color);
            }
        }
    }

    void drawEntity(const Entity& entity, float scrollX) {
        int screenX = static_cast<int>(entity.rect.x - scrollX);
        int screenY = static_cast<int>(entity.rect.y);
        int w = static_cast<int>(entity.rect.width);
        int h = static_cast<int>(entity.rect.height);

        switch (entity.type) {
            case EntityType::PLATFORM:
                for (int dy = 0; dy < h; dy++) {
                    for (int dx = 0; dx < w; dx++) {
                        char c = (dy == 0) ? '=' : '#';
                        setChar(screenX + dx, screenY + dy, c, 'g');
                    }
                }
                break;

            case EntityType::COIN:
                setChar(screenX, screenY, 'o', 'y');
                break;

            case EntityType::GOOMBA:
                setChar(screenX, screenY, '&', 'm');
                setChar(screenX + 1, screenY, '&', 'm');
                setChar(screenX, screenY + 1, 'w', 'm');
                setChar(screenX + 1, screenY + 1, 'w', 'm');
                break;

            case EntityType::BRICK:
                for (int dy = 0; dy < h; dy++) {
                    for (int dx = 0; dx < w; dx++) {
                        setChar(screenX + dx, screenY + dy, '[', 'o');
                    }
                }
                break;

            case EntityType::QUESTION_BLOCK:
                setChar(screenX, screenY, '[', 'y');
                setChar(screenX + 1, screenY, '?', 'y');
                setChar(screenX, screenY + 1, '?', 'y');
                setChar(screenX + 1, screenY + 1, ']', 'y');
                break;

            case EntityType::PIPE:
                for (int dy = 0; dy < h; dy++) {
                    for (int dx = 0; dx < w; dx++) {
                        char c;
                        if (dy == 0) {
                            c = (dx == 0 || dx == w - 1) ? '|' : '_';
                        } else {
                            c = (dx == 0 || dx == w - 1) ? '|' : ' ';
                        }
                        setChar(screenX + dx, screenY + dy, c, 'G');
                    }
                }
                break;

            case EntityType::FLAG:
                for (int dy = 0; dy < h; dy++) {
                    setChar(screenX, screenY + dy, '|', 'w');
                    if (dy < 3) {
                        setChar(screenX + 1, screenY + dy, '>', 'G');
                        setChar(screenX + 2, screenY + dy, '>', 'G');
                    }
                }
                break;
        }
    }

    void drawHUD(int score, int coins, int lives, int level) {
        std::string scoreStr = "SCORE: " + std::to_string(score);
        std::string coinStr = "COINS: " + std::to_string(coins);
        std::string livesStr = "LIVES: " + std::to_string(lives);
        std::string levelStr = "WORLD 1-" + std::to_string(level);

        drawString(2, 1, scoreStr, 'w');
        drawString(25, 1, coinStr, 'y');
        drawString(45, 1, livesStr, 'r');
        drawString(65, 1, levelStr, 'c');
    }

    void drawBackground(float scrollX) {
        // Sky
        for (int y = 2; y < SCREEN_HEIGHT - 2; y++) {
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                setChar(x, y, '.', 'B');
            }
        }

        // Clouds (parallax)
        int cloudOffset = static_cast<int>(scrollX * 0.3f) % SCREEN_WIDTH;
        for (int i = 0; i < 5; i++) {
            int cx = (i * 20 - cloudOffset + SCREEN_WIDTH * 2) % (SCREEN_WIDTH + 20) - 10;
            drawString(cx, 3 + (i % 3), "(__)", 'w');
            drawString(cx - 1, 4 + (i % 3), "(____)", 'w');
        }

        // Hills (parallax)
        int hillOffset = static_cast<int>(scrollX * 0.5f) % 40;
        for (int i = 0; i < 4; i++) {
            int hx = i * 40 - hillOffset;
            drawString(hx, 19, "  /\\  ", 'G');
            drawString(hx, 20, " /  \\ ", 'G');
        }
    }

    void render() {
        moveCursor(0, 0);

        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            std::string line;
            char currentColor = ' ';

            for (int x = 0; x < SCREEN_WIDTH; x++) {
                char newColor = colorBuffer[y][x];
                if (newColor != currentColor) {
                    switch (newColor) {
                        case 'r': line += Color::BRIGHT_RED; break;
                        case 'g': line += Color::GREEN; break;
                        case 'G': line += Color::BRIGHT_GREEN; break;
                        case 'y': line += Color::BRIGHT_YELLOW; break;
                        case 'b': line += Color::BLUE; break;
                        case 'B': line += Color::BRIGHT_BLUE; break;
                        case 'm': line += Color::MAGENTA; break;
                        case 'c': line += Color::CYAN; break;
                        case 'o': line += Color::YELLOW; break;
                        default: line += Color::WHITE; break;
                    }
                    currentColor = newColor;
                }
                line += screenBuffer[y][x];
            }
            line += Color::RESET;
            std::cout << line << "\n";
        }
        std::cout.flush();
    }

    void drawGameOver(int finalScore) {
        clear();
        int cx = SCREEN_WIDTH / 2;
        int cy = SCREEN_HEIGHT / 2;

        drawString(cx - 5, cy - 3, "GAME OVER", 'r');
        drawString(cx - 10, cy, "Final Score: " + std::to_string(finalScore), 'y');
        drawString(cx - 12, cy + 3, "Press SPACE to restart, Q to quit", 'w');
    }

    void drawWin(int finalScore) {
        clear();
        int cx = SCREEN_WIDTH / 2;
        int cy = SCREEN_HEIGHT / 2;

        drawString(cx - 8, cy - 3, "CONGRATULATIONS!", 'G');
        drawString(cx - 6, cy - 1, "YOU WIN!", 'y');
        drawString(cx - 10, cy + 1, "Final Score: " + std::to_string(finalScore), 'y');
        drawString(cx - 12, cy + 4, "Press SPACE to play again, Q to quit", 'w');
    }

    void drawTitle() {
        clear();

        // Mario ASCII art title
        std::vector<std::string> title = {
            "  _____ _   _ _____ _____ ____    __  __          _____  _____ ____  ",
            " / ____| | | |  __ |  ___|  _ \\  |  \\/  |   /\\   |  __ \\|_   _/ __ \\ ",
            "| (___ | | | | |__)|  _| | |_) | | \\  / |  /  \\  | |__) | | || |  | |",
            " \\___ \\| | | |  ___| |___|  _ <  | |\\/| | / /\\ \\ |  _  /  | || |  | |",
            " ____) | |_| | |   |_____|_| \\_\\ | |  | |/ ____ \\| | \\ \\ _| || |__| |",
            "|_____/ \\___/|_|               |_|  |_/_/    \\_|_|  \\_|_____|\\____/ "
        };

        int startY = 4;
        for (size_t i = 0; i < title.size(); i++) {
            drawString(5, startY + i, title[i], 'r');
        }

        drawString(25, 12, "TERMINAL EDITION", 'y');

        // Mario sprite
        drawString(35, 15, "   @>   ", 'r');
        drawString(35, 16, "   A    ", 'r');

        drawString(20, 19, "Press SPACE or ENTER to start", 'w');
        drawString(25, 21, "Press Q to quit", 'w');
    }
};

// ==================== Game Class ====================

class Game {
private:
    Keyboard keyboard;
    Renderer renderer;
    Mario mario;
    Level level;

    int score = 0;
    int coins = 0;
    int currentLevel = 1;
    bool running = true;
    bool gameOver = false;
    bool gameWon = false;
    bool showTitle = true;

public:
    Game() : mario(5, 18) {}

    void init() {
        keyboard.init();
        hideCursor();
        clearScreen();
    }

    void cleanup() {
        keyboard.restore();
        showCursor();
        clearScreen();
    }

    void reset() {
        mario = Mario(level.startPos.x, level.startPos.y);
        level = Level();
        score = 0;
        coins = 0;
        gameOver = false;
        gameWon = false;
    }

    void handleInput() {
        int key = keyboard.getKey();

        if (showTitle) {
            if (key == ' ' || key == '\n' || key == '\r') {
                showTitle = false;
                reset();
            } else if (key == 'q' || key == 'Q') {
                running = false;
            }
            return;
        }

        if (gameOver || gameWon) {
            if (key == ' ') {
                reset();
            } else if (key == 'q' || key == 'Q') {
                running = false;
            }
            return;
        }

        // Game controls - track movement direction
        int moveDir = 0; // -1 = left, 0 = none, 1 = right

        // Process all keys in buffer
        while (key != -1) {
            switch (key) {
                case 'a':
                case 'A':
                    moveDir = -1;
                    break;
                case 'd':
                case 'D':
                    moveDir = 1;
                    break;
                case 'w':
                case 'W':
                case ' ':
                    mario.jump();
                    break;
                case 'q':
                case 'Q':
                    running = false;
                    break;
            }
            key = keyboard.getKey();
        }

        // Apply movement with momentum
        if (moveDir < 0) {
            mario.moveLeft();
        } else if (moveDir > 0) {
            mario.moveRight();
        } else {
            // Apply friction instead of instant stop
            mario.applyFriction();
        }
    }

    void update() {
        if (showTitle || gameOver || gameWon) return;

        // Update Mario
        mario.update();

        // Update enemies
        for (auto& entity : level.entities) {
            if (entity.type == EntityType::GOOMBA && entity.active) {
                entity.rect.x += entity.velX;
                entity.animFrame = (entity.animFrame + 1) % 20;

                // Reverse direction at edges or obstacles
                for (const auto& other : level.entities) {
                    if (other.type == EntityType::PLATFORM || other.type == EntityType::PIPE) {
                        if (entity.rect.x <= 0 ||
                            (entity.rect.intersects(other.rect) && entity.rect.y < other.rect.y)) {
                            entity.velX = -entity.velX;
                        }
                    }
                }
            }
        }

        // Collision detection
        Rectangle marioRect = mario.getRect();
        mario.onGround = false;

        for (auto& entity : level.entities) {
            if (!entity.active) continue;

            Rectangle entityRect = entity.rect;

            if (!marioRect.intersects(entityRect)) continue;

            switch (entity.type) {
                case EntityType::PLATFORM:
                case EntityType::BRICK:
                case EntityType::PIPE: {
                    // Calculate overlap
                    float overlapLeft = (marioRect.x + marioRect.width) - entityRect.x;
                    float overlapRight = (entityRect.x + entityRect.width) - marioRect.x;
                    float overlapTop = (marioRect.y + marioRect.height) - entityRect.y;
                    float overlapBottom = (entityRect.y + entityRect.height) - marioRect.y;

                    float minOverlapX = std::min(overlapLeft, overlapRight);
                    float minOverlapY = std::min(overlapTop, overlapBottom);

                    if (minOverlapY < minOverlapX) {
                        if (overlapTop < overlapBottom) {
                            // Landing on top
                            mario.pos.y = entityRect.y - marioRect.height;
                            mario.vel.y = 0;
                            mario.onGround = true;
                        } else {
                            // Hitting from below
                            mario.pos.y = entityRect.y + entityRect.height;
                            mario.vel.y = 0;

                            if (entity.type == EntityType::BRICK) {
                                entity.active = false;
                                score += 10;
                            }
                        }
                    } else {
                        if (overlapLeft < overlapRight) {
                            mario.pos.x = entityRect.x - marioRect.width;
                        } else {
                            mario.pos.x = entityRect.x + entityRect.width;
                        }
                        mario.vel.x = 0;
                    }
                    break;
                }

                case EntityType::QUESTION_BLOCK: {
                    float overlapBottom = (entityRect.y + entityRect.height) - marioRect.y;
                    float overlapTop = (marioRect.y + marioRect.height) - entityRect.y;

                    if (overlapBottom < overlapTop && mario.vel.y < 0) {
                        // Hit from below - spawn coin
                        entity.type = EntityType::BRICK; // Turn into used block
                        coins++;
                        score += 100;
                        mario.vel.y = 0;
                        mario.pos.y = entityRect.y + entityRect.height;
                    } else if (overlapTop < overlapBottom) {
                        mario.pos.y = entityRect.y - marioRect.height;
                        mario.vel.y = 0;
                        mario.onGround = true;
                    }
                    break;
                }

                case EntityType::COIN:
                    entity.active = false;
                    coins++;
                    score += 200;
                    break;

                case EntityType::GOOMBA: {
                    // Check if stomping - Mario must be falling and hitting from above
                    // Mario's feet should be near the goomba's head
                    float marioBottom = marioRect.y + marioRect.height;
                    float goombaMidY = entityRect.y + entityRect.height * 0.5f;

                    // Stomp if: Mario is falling AND Mario's bottom is above goomba's middle
                    if (mario.vel.y > 0 && marioBottom < goombaMidY + 1.0f) {
                        // Stomp!
                        entity.active = false;
                        score += 100;
                        mario.vel.y = JUMP_FORCE * 0.7f; // Bounce
                    } else if (!mario.invincible) {
                        // Take damage only if not invincible
                        mario.takeDamage();
                        // Push Mario away from goomba
                        mario.vel.x = (mario.pos.x < entity.rect.x) ? -2.0f : 2.0f;
                        mario.vel.y = -1.5f;
                        if (mario.lives <= 0) {
                            gameOver = true;
                        }
                    }
                    break;
                }

                case EntityType::FLAG:
                    gameWon = true;
                    score += 1000;
                    break;
            }
        }

        // Screen boundaries
        if (mario.pos.x < 0) mario.pos.x = 0;
        if (mario.pos.x > level.levelWidth - 5) mario.pos.x = level.levelWidth - 5;

        // Fall death
        if (mario.pos.y > SCREEN_HEIGHT) {
            mario.lives--;
            if (mario.lives <= 0) {
                gameOver = true;
            } else {
                mario.pos = Vector2(level.startPos.x, level.startPos.y);
                mario.vel = Vector2(0, 0);
                level.scrollX = 0;
            }
        }

        // Update scroll
        float targetScroll = mario.pos.x - SCREEN_WIDTH / 3;
        if (targetScroll < 0) targetScroll = 0;
        if (targetScroll > level.levelWidth - SCREEN_WIDTH)
            targetScroll = level.levelWidth - SCREEN_WIDTH;

        level.scrollX += (targetScroll - level.scrollX) * 0.1f;
    }

    void render() {
        renderer.clear();

        if (showTitle) {
            renderer.drawTitle();
        } else if (gameOver) {
            renderer.drawGameOver(score);
        } else if (gameWon) {
            renderer.drawWin(score);
        } else {
            // Draw game
            renderer.drawBackground(level.scrollX);

            // Draw entities
            for (const auto& entity : level.entities) {
                if (!entity.active) continue;

                // Only draw if on screen
                float screenX = entity.rect.x - level.scrollX;
                if (screenX > -10 && screenX < SCREEN_WIDTH + 10) {
                    renderer.drawEntity(entity, level.scrollX);
                }
            }

            // Draw Mario
            renderer.drawMario(mario, level.scrollX);

            // Draw HUD
            renderer.drawHUD(score, coins, mario.lives, currentLevel);
        }

        renderer.render();
    }

    void run() {
        init();

        while (running) {
            auto frameStart = std::chrono::steady_clock::now();

            handleInput();
            update();
            render();

            // Frame timing
            auto frameEnd = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
            if (elapsed.count() < FRAME_DELAY_MS) {
                std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS - elapsed.count()));
            }
        }

        cleanup();
    }
};

// ==================== Main ====================

int main() {
    Game game;
    game.run();

    std::cout << "\nThanks for playing Super Mario Terminal Edition!\n";
    return 0;
}
