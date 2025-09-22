#pragma once
/*
  簡易日本語ビットマップフォント(16x16, 1bpp)
  対応文字: 「日」「本」「語」「コ」「ー」「ド」
  目的: QRコードで受信した "日本語コード" を画面に日本語表示するデモ。

  各グリフは 16x16 ピクセル。上から下へ行単位、左→右。
  1行16ピクセル = 16bit = 2byte。16行で 32byte/グリフ。

  ビット並び: 上位ビット(0x80) が左端ピクセル。

  (注意) このビットマップはデモ用のラフな図案です。必要に応じて調整してください。
*/
#include <stdint.h>

#ifdef ARDUINO_ARCH_ESP32
#include <pgmspace.h>
#else
#define PROGMEM
#endif

typedef struct {
    uint32_t codepoint;   // Unicode コードポイント
    const uint8_t *bitmap; // 16x16 (32 bytes)
} JPFontGlyph;

// 16x16 グリフデータ (ラフデザイン) ---------------------------------------
// できるだけ視認できる程度。後で微調整可能。
static const uint8_t GLYPH_NICHI[] PROGMEM = {
  0x00,0x00,
  0x7F,0xFE,
  0x40,0x02,
  0x5F,0xFA,
  0x48,0x12,
  0x48,0x12,
  0x5F,0xF2,
  0x48,0x12,
  0x48,0x12,
  0x5F,0xF2,
  0x48,0x12,
  0x4F,0xF2,
  0x40,0x02,
  0x7F,0xFE,
  0x00,0x00,
  0x00,0x00,
};
static const uint8_t GLYPH_HON[] PROGMEM = {
  0x00,0x00,
  0x7F,0xFE,
  0x40,0x02,
  0x47,0xE2,
  0x44,0x22,
  0x47,0xE2,
  0x40,0x02,
  0x5F,0xFA,
  0x48,0x12,
  0x48,0x12,
  0x4F,0xF2,
  0x48,0x12,
  0x40,0x02,
  0x7F,0xFE,
  0x00,0x00,
  0x00,0x00,
};
static const uint8_t GLYPH_GO[] PROGMEM = { // 語 (簡略)
  0x00,0x00,
  0x7F,0xFE,
  0x42,0x02,
  0x5F,0xFA,
  0x42,0x02,
  0x5F,0xFA,
  0x48,0x12,
  0x4F,0xF2,
  0x48,0x12,
  0x5F,0xFA,
  0x48,0x12,
  0x48,0x12,
  0x40,0x02,
  0x7F,0xFE,
  0x00,0x00,
  0x00,0x00,
};
static const uint8_t GLYPH_KO[] PROGMEM = { // コ (片仮名)
  0x00,0x00,
  0x7F,0xFE,
  0x40,0x00,
  0x40,0x00,
  0x40,0x00,
  0x40,0x00,
  0x5F,0xFC,
  0x40,0x04,
  0x40,0x04,
  0x40,0x04,
  0x40,0x04,
  0x40,0x04,
  0x40,0x04,
  0x7F,0xFC,
  0x00,0x00,
  0x00,0x00,
};
static const uint8_t GLYPH_BAR[] PROGMEM = { // ー (長音)
  0x00,0x00,
  0x00,0x00,
  0x00,0x00,
  0x00,0x00,
  0x7F,0xF8,
  0x00,0x08,
  0x00,0x08,
  0x00,0x08,
  0x00,0x08,
  0x00,0x08,
  0x00,0x00,
  0x00,0x00,
  0x00,0x00,
  0x00,0x00,
  0x00,0x00,
  0x00,0x00,
};
static const uint8_t GLYPH_DO[] PROGMEM = { // ド (片仮名+濁点簡略)
  0x00,0x00,
  0x7F,0xFE,
  0x40,0x02,
  0x40,0x02,
  0x5F,0xFA,
  0x40,0x02,
  0x5F,0xFA,
  0x40,0x02,
  0x40,0x02,
  0x40,0x02,
  0x5F,0xFA,
  0x40,0x02,
  0x40,0x02,
  0x7F,0xFE,
  0x00,0x00,
  0x00,0x00,
};

// Unicode コードポイント
#define CP_NICHI 0x65E5
#define CP_HON   0x672C
#define CP_GO    0x8A9E
#define CP_KO    0x30B3
#define CP_BAR   0x30FC
#define CP_DO    0x30C9

static const JPFontGlyph JP_GLYPHS[] PROGMEM = {
    {CP_NICHI, GLYPH_NICHI},
    {CP_HON,   GLYPH_HON},
    {CP_GO,    GLYPH_GO},
    {CP_KO,    GLYPH_KO},
    {CP_BAR,   GLYPH_BAR},
    {CP_DO,    GLYPH_DO},
};
static const size_t JP_GLYPH_COUNT = sizeof(JP_GLYPHS)/sizeof(JP_GLYPHS[0]);

// 検索関数
inline const uint8_t* jpfont_find(uint32_t cp) {
    for (size_t i=0;i<JP_GLYPH_COUNT;i++) {
        if (JP_GLYPHS[i].codepoint == cp) return JP_GLYPHS[i].bitmap;
    }
    return nullptr;
}

// 16x16 描画 (前景色=fg, 背景塗らない/透過)
#include <M5Unified.h>
inline void jpfont_drawGlyph(int x, int y, uint32_t cp, uint16_t fg) {
    auto bmp = jpfont_find(cp);
    if (!bmp) return; // 未登録は飛ばす
    for (int row=0; row<16; ++row) {
        uint8_t b0 = bmp[row*2];
        uint8_t b1 = bmp[row*2+1];
        uint16_t bits = (b0 << 8) | b1; // 左→右
        for (int col=0; col<16; ++col) {
            if (bits & (0x8000 >> col)) {
                M5.Display.drawPixel(x+col, y+row, fg);
            }
        }
    }
}

// UTF-8 → コードポイント → グリフ描画
// 戻り値: 描画したピクセル幅
inline int jpfont_drawUTF8(int x, int y, const char* utf8, uint16_t fg) {
    const uint8_t* p = (const uint8_t*)utf8;
    int cursorX = x;
    while (*p) {
        uint32_t cp;
        int advanceBytes = 1;
        if ((*p & 0x80) == 0) { // ASCII
            // ASCII は標準フォントへ (半角1文字=8px相当で揃える)
            M5.Display.setCursor(cursorX, y);
            M5.Display.print((char)*p);
            cursorX += 8;
            p++;
            continue;
        } else if ((*p & 0xE0) == 0xC0) {
            cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
            advanceBytes = 2;
        } else if ((*p & 0xF0) == 0xE0) {
            cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            advanceBytes = 3;
        } else if ((*p & 0xF8) == 0xF0) { // 4byte (今回は未使用想定)
            cp = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            advanceBytes = 4;
        } else {
            // 不正シーケンススキップ
            p++;
            continue;
        }
        if (jpfont_find(cp)) {
            jpfont_drawGlyph(cursorX, y, cp, fg);
            cursorX += 16; // 全角幅
        } else {
            // 未登録 -> "□" 的な簡易表示 (16x16枠)
            for (int i=0;i<16;i++) {
                M5.Display.drawPixel(cursorX+i, y, fg);
                M5.Display.drawPixel(cursorX+i, y+15, fg);
                M5.Display.drawPixel(cursorX, y+i, fg);
                M5.Display.drawPixel(cursorX+15, y+i, fg);
            }
            cursorX += 16;
        }
        p += advanceBytes;
    }
    return cursorX - x;
}
