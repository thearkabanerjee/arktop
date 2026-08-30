#pragma once

class Terminal {
public:
    Terminal();

    void setup();
    void restore();

    void clear();
    void hideCursor();
    void showCursor();

    void moveCursor(int row, int column);

    int getWidth() const;
    int getHeight() const;

    bool keyAvailable();
    char readKey();

private:
    bool configured = false;
};
