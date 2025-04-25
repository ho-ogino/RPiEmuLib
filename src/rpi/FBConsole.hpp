#ifndef FB_CONSOLE_HPP
#define FB_CONSOLE_HPP

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "font8x8_basic.h"

typedef uint16_t FBConsoleColor;

class FBConsole {
public:
    FBConsole(uint16_t width, uint16_t height);
    FBConsole(uint16_t width, uint16_t height, FBConsoleColor* frame_buffer, size_t pitch);
    ~FBConsole();

    bool init(uint16_t* external_buffer = nullptr, size_t pitch = 0);
    void clear(FBConsoleColor color);
    void setPixel(uint16_t x, uint16_t y, FBConsoleColor color);
    void drawChar(char c, uint16_t x, uint16_t y, FBConsoleColor color, FBConsoleColor bgColor, bool transparent);
    void drawString(const char* str, uint16_t x, uint16_t y, FBConsoleColor color, FBConsoleColor bgColor, bool transparent);
    void drawChar16(char c, uint16_t x, uint16_t y, FBConsoleColor color, FBConsoleColor bgColor, bool transparent);
    void drawString16(const char* str, uint16_t x, uint16_t y, FBConsoleColor color, FBConsoleColor bgColor, bool transparent);
    void scrollUp(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t lines, FBConsoleColor fillColor = 0);
    void scrollDown(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t lines, FBConsoleColor fillColor = 0);
    void putPixel(uint16_t x, uint16_t y, FBConsoleColor color);
    void drawHLine(uint16_t x1, uint16_t x2, uint16_t y, FBConsoleColor color);
    FBConsoleColor* getBuffer() const;
    uint16_t getWidth() const;
    uint16_t getHeight() const;

    static FBConsoleColor Color_fromRGB(uint8_t r, uint8_t g, uint8_t b);
    static FBConsoleColor Color_fromRGB8(uint8_t r, uint8_t g, uint8_t b);

private:
    uint16_t width;
    uint16_t height;
    FBConsoleColor* buffer;
    bool buffer_owned;
    size_t pitch;
};

#endif // FB_CONSOLE_HPP 