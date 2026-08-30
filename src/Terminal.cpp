#include "Terminal.hpp"

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstdio>

static struct termios originalTerminal;

Terminal::Terminal() {
}

void Terminal::setup() {

    if (configured) {
        return;
    }

    tcgetattr(STDIN_FILENO, &originalTerminal);

    struct termios raw = originalTerminal;

    raw.c_lflag &= ~(ICANON | ECHO);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(
        STDIN_FILENO,
        TCSANOW,
        &raw
    );

    int flags = fcntl(
        STDIN_FILENO,
        F_GETFL,
        0
    );

    fcntl(
        STDIN_FILENO,
        F_SETFL,
        flags | O_NONBLOCK
    );

    hideCursor();

    configured = true;
}

void Terminal::restore() {

    if (!configured) {
        return;
    }

    tcsetattr(
        STDIN_FILENO,
        TCSANOW,
        &originalTerminal
    );

    showCursor();

    configured = false;
}

void Terminal::clear() {

    std::printf("\033[2J");
    std::printf("\033[H");
}

void Terminal::hideCursor() {

    std::printf("\033[?25l");
}

void Terminal::showCursor() {

    std::printf("\033[?25h");
}

void Terminal::moveCursor(int row, int column) {

    std::printf(
        "\033[%d;%dH",
        row,
        column
    );
}

int Terminal::getWidth() const {

    struct winsize windowSize{};

    ioctl(
        STDOUT_FILENO,
        TIOCGWINSZ,
        &windowSize
    );

    return windowSize.ws_col;
}

int Terminal::getHeight() const {

    struct winsize windowSize{};

    ioctl(
        STDOUT_FILENO,
        TIOCGWINSZ,
        &windowSize
    );

    return windowSize.ws_row;
}

bool Terminal::keyAvailable() {

    char c;

    return read(
        STDIN_FILENO,
        &c,
        1
    ) > 0;
}

char Terminal::readKey() {

    char c = 0;

    read(
        STDIN_FILENO,
        &c,
        1
    );

    return c;
}
