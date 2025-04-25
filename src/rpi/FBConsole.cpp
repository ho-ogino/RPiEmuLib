#include "FBConsole.hpp"
#include <string.h>

FBConsole::FBConsole(uint16_t width, uint16_t height)
    : width(width), height(height), buffer(nullptr), buffer_owned(false), pitch(width) {}

FBConsole::FBConsole(uint16_t width, uint16_t height, FBConsoleColor* frame_buffer, size_t pitch)
    : width(width), height(height), buffer(frame_buffer), buffer_owned(false), pitch(pitch) {}

FBConsole::~FBConsole() {
    if (buffer_owned && buffer != nullptr) {
        free(buffer);
    }
}

bool FBConsole::init(uint16_t* external_buffer, size_t pitch) {
    if (external_buffer != nullptr) {
        buffer = external_buffer;
        buffer_owned = false;
    } else {
        buffer = (FBConsoleColor*)malloc(width * height * sizeof(FBConsoleColor));
        if (buffer == nullptr) {
            return false;
        }
        buffer_owned = true;
    }

    this->pitch = pitch;

    clear(0); // 黒で初期化
    return true;
}

void FBConsole::clear(FBConsoleColor color) {
    for (uint32_t i = 0; i < width * height; i++) {
        buffer[i] = color;
    }
}

void FBConsole::setPixel(uint16_t x, uint16_t y, FBConsoleColor color) {
    if (x < width && y < height) {
        buffer[y * pitch + x] = color;
    }
}

void FBConsole::drawChar(char c, uint16_t x, uint16_t y, FBConsoleColor color, FBConsoleColor bgColor, bool transparent) {
    if (c < 0 || c > 127) return;

    const unsigned char* fontData = font8x8_basic[(unsigned char)c];

    for (uint8_t row = 0; row < 8; row++) {
        char rowData = fontData[row];
        for (uint8_t col = 0; col < 8; col++) {
            if (rowData & (1 << col)) {
                setPixel(x + col, y + row, color);
            } else if (!transparent) {
                setPixel(x + col, y + row, bgColor);
            }
        }
    }
}

void FBConsole::drawChar16(char c, uint16_t x, uint16_t y, FBConsoleColor color, FBConsoleColor bgColor, bool transparent) {
    if (c < 0 || c > 127) return;

    const unsigned char* fontData = font8x8_basic[(unsigned char)c];

    for (uint8_t row = 0; row < 8; row++) {
        char rowData = fontData[row];
        for (uint8_t col = 0; col < 8; col++) {
            if (rowData & (1 << col)) {
                // 各ピクセルを2x2の大きさで描画
                setPixel(x + col*2, y + row*2, color);
                setPixel(x + col*2 + 1, y + row*2, color);
                setPixel(x + col*2, y + row*2 + 1, color);
                setPixel(x + col*2 + 1, y + row*2 + 1, color);
            } else if (!transparent) {
                // 背景ピクセルも2x2の大きさで描画
                setPixel(x + col*2, y + row*2, bgColor);
                setPixel(x + col*2 + 1, y + row*2, bgColor);
                setPixel(x + col*2, y + row*2 + 1, bgColor);
                setPixel(x + col*2 + 1, y + row*2 + 1, bgColor);
            }
        }
    }
}

void FBConsole::drawString(const char* str, uint16_t x, uint16_t y, FBConsoleColor color, FBConsoleColor bgColor, bool transparent) {
    uint16_t currentX = x;

    while (*str) {
        drawChar(*str, currentX, y, color, bgColor, transparent);
        str++;
        currentX += 8;

        if (currentX + 8 > width) {
            currentX = x;
            y += 8;

            if (y + 8 > height) {
                break;
            }
        }
    }
}

void FBConsole::drawString16(const char* str, uint16_t x, uint16_t y, FBConsoleColor color, FBConsoleColor bgColor, bool transparent) {
    uint16_t currentX = x;

    while (*str) {
        drawChar16(*str, currentX, y, color, bgColor, transparent);
        str++;
        currentX += 16; // 16ピクセル幅で移動（8x8フォントの倍）

        if (currentX + 16 > width) {
            currentX = x;
            y += 16; // 16ピクセル高で移動（8x8フォントの倍）

            if (y + 16 > height) {
                break;
            }
        }
    }
}

void FBConsole::scrollUp(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t lines, FBConsoleColor fillColor) {
    // 座標が有効範囲内にあることを確認
    if (x1 >= width || y1 >= height || x2 >= width || y2 >= height || x1 > x2 || y1 > y2) {
        return;
    }

    // スクロールする行数が矩形の高さ以上の場合は、単に塗りつぶす
    if (lines >= (y2 - y1 + 1)) {
        for (uint16_t y = y1; y <= y2; y++) {
            for (uint16_t x = x1; x <= x2; x++) {
                buffer[y * pitch + x] = fillColor;
            }
        }
        return;
    }

    // ピクセルを上に移動
    for (uint16_t y = y1; y <= y2 - lines; y++) {
        for (uint16_t x = x1; x <= x2; x++) {
            buffer[y * pitch + x] = buffer[(y + lines) * pitch + x];
        }
    }

    // 下部を塗りつぶす
    for (uint16_t y = y2 - lines + 1; y <= y2; y++) {
        for (uint16_t x = x1; x <= x2; x++) {
            buffer[y * pitch + x] = fillColor;
        }
    }
}

void FBConsole::scrollDown(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t lines, FBConsoleColor fillColor) {
    // 座標が有効範囲内にあることを確認
    if (x1 >= width || y1 >= height || x2 >= width || y2 >= height || x1 > x2 || y1 > y2) {
        return;
    }

    // スクロールする行数が矩形の高さ以上の場合は、単に塗りつぶす
    if (lines >= (y2 - y1 + 1)) {
        for (uint16_t y = y1; y <= y2; y++) {
            for (uint16_t x = x1; x <= x2; x++) {
                buffer[y * pitch + x] = fillColor;
            }
        }
        return;
    }
    
    // ピクセルを下に移動
    for (uint16_t y = y2; y >= y1 + lines; y--) {
        for (uint16_t x = x1; x <= x2; x++) {
            buffer[y * pitch + x] = buffer[(y - lines) * pitch + x];
        }
    }

    // 上部を塗りつぶす
    for (uint16_t y = y1; y < y1 + lines; y++) {
        for (uint16_t x = x1; x <= x2; x++) {
            buffer[y * pitch + x] = fillColor;
        }
    }
}

FBConsoleColor* FBConsole::getBuffer() const {
    return buffer;
}

uint16_t FBConsole::getWidth() const {
    return width;
}

uint16_t FBConsole::getHeight() const {
    return height;
}

FBConsoleColor FBConsole::Color_fromRGB(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((r & 0x1F) << 11 | (g & 0x3F) << 5 | (b & 0x1F));
}

FBConsoleColor FBConsole::Color_fromRGB8(uint8_t r, uint8_t g, uint8_t b) {
    return Color_fromRGB(r >> 3, g >> 2, b >> 3);
} 

void FBConsole::putPixel(uint16_t x, uint16_t y, FBConsoleColor color) {
    if (x < width && y < height) {
        buffer[y * pitch + x] = color;
    }
}

void FBConsole::drawHLine(uint16_t x1, uint16_t x2, uint16_t y, FBConsoleColor color) {
    if (x1 >= width || x2 >= width || y >= height || x1 > x2) {
        return;
    }
    for (uint16_t x = x1; x <= x2; x++) {
        buffer[y * pitch + x] = color;
    }
}