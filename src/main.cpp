// M5Atom S3 + ATOM QR-CODE (UART)
//  - モジュール(UART2: RX=GPIO5 TX=GPIO6)で読んだUTF-8テキストをUSB HIDキーボードとして入力
//  - 日本語は Alt+X 方式で1文字ずつUnicode変換
//  - 画面に履歴表示 (内蔵日本語フォント)

#include <M5Unified.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

// Alt+X 変換後の安定待ち (ms)
#ifndef ALT_X_POST_DELAY_MS
#define ALT_X_POST_DELAY_MS 5
#endif

USBHIDKeyboard Keyboard;
static bool debugMode = false; // true: HID経由でデバッグ文字列送出

// デバッグ出力 (ASCII限定。未対応文字は無視)
static void debugSendRawChar(char c) {
  // US配列キーボード対応の文字マッピング
  struct Map { char c; uint8_t code; bool shift; };
  static const Map m[] = {
    {'A',0x04,true},{'B',0x05,true},{'C',0x06,true},{'D',0x07,true},{'E',0x08,true},{'F',0x09,true},{'G',0x0A,true},{'H',0x0B,true},{'I',0x0C,true},{'J',0x0D,true},{'K',0x0E,true},{'L',0x0F,true},{'M',0x10,true},{'N',0x11,true},{'O',0x12,true},{'P',0x13,true},{'Q',0x14,true},{'R',0x15,true},{'S',0x16,true},{'T',0x17,true},{'U',0x18,true},{'V',0x19,true},{'W',0x1A,true},{'X',0x1B,true},{'Y',0x1C,true},{'Z',0x1D,true},
    {'a',0x04,false},{'b',0x05,false},{'c',0x06,false},{'d',0x07,false},{'e',0x08,false},{'f',0x09,false},{'g',0x0A,false},{'h',0x0B,false},{'i',0x0C,false},{'j',0x0D,false},{'k',0x0E,false},{'l',0x0F,false},{'m',0x10,false},{'n',0x11,false},{'o',0x12,false},{'p',0x13,false},{'q',0x14,false},{'r',0x15,false},{'s',0x16,false},{'t',0x17,false},{'u',0x18,false},{'v',0x19,false},{'w',0x1A,false},{'x',0x1B,false},{'y',0x1C,false},{'z',0x1D,false},
    {'0',0x27,false},{'1',0x1E,false},{'2',0x1F,false},{'3',0x20,false},{'4',0x21,false},{'5',0x22,false},{'6',0x23,false},{'7',0x24,false},{'8',0x25,false},{'9',0x26,false},
    {' ',0x2C,false},{'-',0x2D,false},{'=',0x2E,false},{'[',0x2F,false},{']',0x30,false},{'\\',0x31,false},{';',0x33,false},{'\'',0x34,false},{'`',0x35,false},{',',0x36,false},{'.',0x37,false},{'/',0x38,false},
    {'_',0x2D,true},{'+',0x2E,true},{'{',0x2F,true},{'}',0x30,true},{'|',0x31,true},{':',0x33,true},{'"',0x34,true},{'~',0x35,true},{'<',0x36,true},{'>',0x37,true},{'?',0x38,true},
  };
  if (c=='\n') { Keyboard.pressRaw(HID_KEY_RETURN); delay(3); Keyboard.releaseRaw(HID_KEY_RETURN); return; }
  for (auto &e : m) {
    if (e.c == c) {
      if (e.shift) { Keyboard.pressRaw(0xE1); delay(1); } // Left Shift
      Keyboard.pressRaw(e.code); delay(2); Keyboard.releaseRaw(e.code); delay(2);
      if (e.shift) { Keyboard.releaseRaw(0xE1); delay(1); }
      return;
    }
  }
}

static void debugPrint(const String &s) { if (!debugMode) return; for (size_t i=0;i<s.length(); ++i) debugSendRawChar(s[i]); }
static void debugPrintln(const String &s) { debugPrint(s); debugSendRawChar('\n'); }

// QRコードのpayloadをHex文字列に変換
static String payloadToHex(const String &payload) {
  String hexStr = "";
  for (size_t i = 0; i < payload.length(); i++) {
    if (i > 0) hexStr += " ";
    char buf[4];
    sprintf(buf, "%02X", (uint8_t)payload[i]);
    hexStr += buf;
  }
  return hexStr;
}

// 行頭/末尾の制御文字を除去
static String sanitizeQRRaw(const String &raw) {
  if (!raw.length()) return raw;
  int start = 0; int end = raw.length();
  while (start < end) {
    uint8_t b = (uint8_t)raw[start];
    if (b < 0x20 && b != '\t') { start++; continue; }
    break;
  }
  while (end > start) {
    uint8_t b = (uint8_t)raw[end-1];
    if (b < 0x20 && b != '\t') { end--; continue; }
    break;
  }
  return raw.substring(start, end);
}

#ifndef SERIAL_RX_PIN
#define SERIAL_RX_PIN 5
#endif
#ifndef SERIAL_TX_PIN
#define SERIAL_TX_PIN 6
#endif
#ifndef TRIG_PIN
#define TRIG_PIN 7
#endif

// --- QRモジュール制御コマンド (最小サブセット) ---
static const uint8_t WAKEUP_CMD        = 0x00;
static const uint8_t START_SCAN_CMD[]  = {0x04, 0xE4, 0x04, 0x00, 0xFF, 0x14};
static const uint8_t STOP_SCAN_CMD[]   = {0x04, 0xE5, 0x04, 0x00, 0xFF, 0x13};
static const uint8_t HOST_MODE_CMD[]   = {0x07, 0xC6, 0x04, 0x08, 0x00, 0x8A, 0x08, 0xFE, 0x95};
static const uint8_t ACK_CMD[]         = {0x04, 0xD0, 0x00, 0x00, 0xFF, 0x2C};

// --- 状態 ---
bool scanning = false;
static const uint32_t CANDIDATE_BAUDS[] = {115200, 9600};
uint8_t baudIndex = 0;

// UTF-8 1～4バイト → Unicode code point
static bool nextCodePoint(const char* s, size_t len, size_t& i, uint32_t& cp) {
  if (i >= len) return false;
  uint8_t first = (uint8_t)s[i++];
  
  // ASCII文字 (1バイト)
  if (first < 0x80) { 
    cp = first; 
    return true; 
  }
  
  // 2バイト文字
  if ((first & 0xE0) == 0xC0) {
    if (i >= len) return false;
    uint8_t second = (uint8_t)s[i++];
    if ((second & 0xC0) == 0x80) { 
      cp = ((first & 0x1F) << 6) | (second & 0x3F); 
      return true; 
    }
    return false;
  }
  
  // 3バイト文字
  if ((first & 0xF0) == 0xE0) {
    if (i + 1 >= len) return false;
    uint8_t s2 = (uint8_t)s[i++]; 
    uint8_t s3 = (uint8_t)s[i++];
    if ((s2 & 0xC0) == 0x80 && (s3 & 0xC0) == 0x80) { 
      cp = ((first & 0x0F) << 12) | ((s2 & 0x3F) << 6) | (s3 & 0x3F); 
      return true; 
    }
    return false;
  }
  
  // 4バイト文字
  if ((first & 0xF8) == 0xF0) {
    if (i + 2 >= len) return false;
    uint8_t s2 = (uint8_t)s[i++]; 
    uint8_t s3 = (uint8_t)s[i++]; 
    uint8_t s4 = (uint8_t)s[i++];
    if ((s2 & 0xC0) == 0x80 && (s3 & 0xC0) == 0x80 && (s4 & 0xC0) == 0x80) {
      cp = ((first & 0x07) << 18) | ((s2 & 0x3F) << 12) | ((s3 & 0x3F) << 6) | (s4 & 0x3F); 
      return true; 
    }
    return false;
  }
  
  // 不正な文字
  cp = 0xFFFD; 
  return true;
}

// 16進数文字をHID usage codeに変換
static uint8_t kc_hex(char h) {
  switch (h) {
    case '0': return 0x27; case '1': return 0x1E; case '2': return 0x1F; case '3': return 0x20;
    case '4': return 0x21; case '5': return 0x22; case '6': return 0x23; case '7': return 0x24;
    case '8': return 0x25; case '9': return 0x26; case 'A': case 'a': return 0x04; case 'B': case 'b': return 0x05;
    case 'C': case 'c': return 0x06; case 'D': case 'd': return 0x07; case 'E': case 'e': return 0x08; case 'F': case 'f': return 0x09;
  }
  return 0;
}

#ifndef HID_KEY_RETURN
#define HID_KEY_RETURN 0x28
#endif

#ifndef HID_KEY_ALT_LEFT
#define HID_KEY_ALT_LEFT 0xE2
#endif

// 非ASCIIコードポイントを Alt+X 方式で入力 (Windows US配列想定)
//  - ASCIIは直接送信 (高速)
//  - 非ASCIIは: 先頭にバリア用スペース→ HEX → Alt+X → スペース除去
static void sendUnicodeAltX(uint32_t cp) {
  if (cp == '\n') { Keyboard.pressRaw(HID_KEY_RETURN); delay(2); Keyboard.releaseRaw(HID_KEY_RETURN); delay(2); return; }
  if (cp < 0x20) return;

  // ASCIIは直接キー送出
  if (cp < 0x80) {
    // 英数字/記号のみ対応 (既存 debugSendRawChar と同趣旨) - ここでは簡易実装
    char c = (char)cp;
    debugSendRawChar(c); // 既存マップ流用 (shift処理含む)
    return;
  }

  // 非ASCII: 4桁(基本多言語面)/6桁(補助面) HEX化
  char hexbuf[16];
  if (cp <= 0xFFFF) {
    snprintf(hexbuf, sizeof(hexbuf), "%04X", (unsigned)cp);
  } else {
    snprintf(hexbuf, sizeof(hexbuf), "%06X", (unsigned)cp);
  }
  // バリア: 前にスペースを1つ挿入 (ASCII末尾HEXとの連結防止)
  Keyboard.pressRaw(0x2C); delay(2); Keyboard.releaseRaw(0x2C); delay(2);

  for (char* p = hexbuf; *p; ++p) {
    uint8_t kc = kc_hex(*p);
    if (!kc) continue;
    Keyboard.pressRaw(kc); delay(2); Keyboard.releaseRaw(kc); delay(2);
  }
  // Alt+X (変換)
  Keyboard.pressRaw(HID_KEY_ALT_LEFT); delay(2);
  Keyboard.pressRaw(0x1B); delay(2); // 'X'
  Keyboard.releaseRaw(0x1B); delay(2);
  Keyboard.releaseRaw(HID_KEY_ALT_LEFT); delay(ALT_X_POST_DELAY_MS); // 変換完了待ち

  // スペース除去: Left×2 → Delete → Right
  Keyboard.pressRaw(0x50); delay(6); Keyboard.releaseRaw(0x50); delay(2); // to char
  Keyboard.pressRaw(0x50); delay(6); Keyboard.releaseRaw(0x50); delay(2); // to space
  Keyboard.pressRaw(0x4C); delay(8); Keyboard.releaseRaw(0x4C); delay(2); // delete space
  Keyboard.pressRaw(0x4F); delay(6); Keyboard.releaseRaw(0x4F); delay(2); // back after char
}

static void typeUtf8AltX(const String& text) {
  const char* s = text.c_str();
  size_t len = text.length();
  size_t i = 0;
  uint32_t cp;
  
  // デバッグ: デコード結果を16進で出力
  if (debugMode) {
    String debugStr = "[DECODE]";
    size_t debug_i = 0;
    while (nextCodePoint(s, len, debug_i, cp)) {
      debugStr += " " + String(cp, HEX);
    }
    debugPrintln(debugStr);
  }
  
  i = 0; // リセット
  while (nextCodePoint(s, len, i, cp)) {
    sendUnicodeAltX(cp);
  }
}

// 表示履歴管理 (折返し + スクロール)
static const uint8_t  MAX_LINES   = 8;   // 表示保持行数
static const uint16_t LINE_HEIGHT = 20;  // フォント16px + マージン
static String lines[MAX_LINES];
static uint8_t lineCount = 0;

void pushLine(const String& s) {
  if (lineCount < MAX_LINES) {
    lines[lineCount++] = s;
  } else {
    for (uint8_t i=1;i<MAX_LINES;i++) lines[i-1] = lines[i];
    lines[MAX_LINES-1] = s;
  }
}

void wrapAndPush(const String& s, int maxWidth) {
  if (!s.length()) { pushLine(""); return; }
  int start = 0;
  while (start < (int)s.length()) {
    int end = s.length();
    while (end > start) {
      String part = s.substring(start, end);
      if (M5.Display.textWidth(part) <= maxWidth) break;
      --end;
    }
    if (end == start) end = start + 1; // 最低1文字は表示
    pushLine(s.substring(start, end));
    start = end;
  }
}

void redrawAll() {
  M5.Display.fillRect(0,0,M5.Display.width(),M5.Display.height(),BLACK);
  for (uint8_t i=0;i<lineCount;i++) {
    int y = i * LINE_HEIGHT;
    if (y + LINE_HEIGHT > M5.Display.height()) break;
    M5.Display.setCursor(0, y);
    M5.Display.println(lines[i]);
  }
}

inline void addInfo(const String& s) { wrapAndPush(s, M5.Display.width()); redrawAll(); }
inline void addQRResult(const String& s) { wrapAndPush(s, M5.Display.width()); redrawAll(); }

// スキャン制御
void startScan() {
  Serial2.write(WAKEUP_CMD); delay(40);
  Serial2.write(START_SCAN_CMD, sizeof(START_SCAN_CMD));
  scanning = true;
  addInfo("Scanning...");
}

void stopScan() {
  Serial2.write(WAKEUP_CMD); delay(40);
  Serial2.write(STOP_SCAN_CMD, sizeof(STOP_SCAN_CMD));
  scanning = false;
  addInfo("Stopped");
}

void setup() {
  auto cfg = M5.config();
  cfg.clear_display = true;
  M5.begin(cfg);

  pinMode(TRIG_PIN, INPUT_PULLUP);

  Serial2.begin(CANDIDATE_BAUDS[baudIndex], SERIAL_8N1, SERIAL_RX_PIN, SERIAL_TX_PIN);

  // 内蔵日本語フォントを設定
  M5.Display.setFont(&fonts::lgfxJapanGothic_16);
  M5.Display.setTextColor(WHITE, BLACK);
  addInfo("Init...");

  // モジュール初期化
  delay(200);
  Serial2.write(WAKEUP_CMD);
  delay(80);
  Serial2.write(HOST_MODE_CMD, sizeof(HOST_MODE_CMD));
  delay(120);
  addInfo("Ready");
  debugPrintln("[BOOT] Ready");

  // 初期化時残存フレーム破棄
  uint32_t flushStart = millis();
  while (millis() - flushStart < 50) {
    while (Serial2.available()) { Serial2.read(); flushStart = millis(); }
  }
  debugPrintln("[BOOT] Flush");

  // USB HID初期化
  USB.begin(); delay(80);
  Keyboard.begin(); delay(80);
  addInfo("HID Ready");

  startScan();
  debugPrintln("[SCAN] start");
}

void loop() {
  M5.update();

  // ボタン: 短押し=scan切替 / 1.5s=debug切替 / 3s=baud切替
  static bool long1Done=false, long2Done=false;
  static uint32_t pressStart = 0;
  if (M5.BtnA.wasPressed()) { long1Done=false; long2Done=false; pressStart = millis(); }
  if (M5.BtnA.isPressed()) {
    uint32_t hold = millis() - pressStart;
    if (hold >= 3000 && !long2Done) {
      baudIndex = (baudIndex + 1) % (sizeof(CANDIDATE_BAUDS)/sizeof(CANDIDATE_BAUDS[0]));
      Serial2.updateBaudRate(CANDIDATE_BAUDS[baudIndex]);
      String msg = "Baud->" + String(CANDIDATE_BAUDS[baudIndex]);
      addInfo(msg); debugPrintln("[BAUD] " + msg);
      long2Done = true; long1Done = true; // 3s達成時は1.5s動作も無効化
    } else if (hold >= 1500 && !long1Done) {
      debugMode = !debugMode;
      addInfo(debugMode?"Debug ON":"Debug OFF");
      debugPrintln(debugMode?"[MODE] Debug ON":"[MODE] Debug OFF");
      long1Done = true;
    }
  }
  if (M5.BtnA.wasReleased()) {
    if (!long1Done && !long2Done) { // 短押し時のスキャン切り替え
      if (scanning) {
        stopScan();
        debugPrintln("[SCAN] stop");
      } else {
        startScan();
        debugPrintln("[SCAN] start");
      }
    }
  }

  // 受信処理
  static String line;
  static uint32_t lastByteTs = 0;
  // 制御フレーム除去 (先頭 0x04 / 0x07 判定)
  while (Serial2.available()) {
    int first = Serial2.peek();
    if (first == 0x04 || first == 0x07) {
      // 最低 2 バイトは必要
      if (Serial2.available() < 2) break;
      uint8_t hdr[2];
      hdr[0] = Serial2.read();
      hdr[1] = Serial2.read();
      if (hdr[0] == 0x04 && (hdr[1] == 0xE4 || hdr[1] == 0xE5 || hdr[1] == 0xD0)) {
        // 既知 6バイト固定フレーム (START/STOP/ACK)
        while (Serial2.available() < 4) { /* wait */ }
        for (int i=0;i<4 && Serial2.available();++i) Serial2.read();
        continue; // 制御フレーム捨て
      } else if (hdr[0] == 0x07 && hdr[1] == 0xC6) {
        // HOST_MODE_CMD エコー 9バイト
        while (Serial2.available() < 7) { /* wait */ }
        for (int i=0;i<7 && Serial2.available();++i) Serial2.read();
        continue;
      } else {
        // 不明フレーム -> line に追加
        line += (char)hdr[0];
        line += (char)hdr[1];
        break;
      }
    } else {
      break; // 制御フレームでない
    }
  }
  while (Serial2.available()) {
    int ch = Serial2.read();
    if (ch < 0) break;
    lastByteTs = millis();
    if (ch == '\r') continue;
    if (ch == '\n') {
      if (line.length()) {
        String payload = sanitizeQRRaw(line);
        addQRResult(payload);
        // HID送信
        typeUtf8AltX(payload);
  sendUnicodeAltX('\n');
        debugPrintln("[QR]" + payloadToHex(payload));
        Serial2.write(ACK_CMD, sizeof(ACK_CMD));
        line = "";
      }
    } else {
      line += (char)ch;
    }
  }
  // タイムアウト確定 (500ms)
  if (line.length() && (millis() - lastByteTs) > 500) {
    String payload = sanitizeQRRaw(line);
    addQRResult(payload);
    typeUtf8AltX(payload);
  sendUnicodeAltX('\n');
    debugPrintln("[QRp]" + payloadToHex(payload));
    Serial2.write(ACK_CMD, sizeof(ACK_CMD));
    line = "";
  }
}
