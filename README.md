# AtomS3 QR Entry (HID Unicode Alt+X)

[M5Atom S3](https://docs.m5stack.com/ja/core/AtomS3) と [ATOMic QR-CODE](https://docs.m5stack.com/ja/atom/ATOM%20QR-CODE%20Kit) モジュールを使い、読み取った QR コード内容 (UTF-8) を USB HID キーボードとしてホストPCへ入力するサンプルです。 
日本語など ASCII 以外の文字は Windows の `Alt+X` Unicode 変換機能を利用して 1 文字ずつ入力します。

## 特徴
- UART2 (RX=GPIO5 / TX=GPIO6) で ATOM QR-CODE モジュールと接続
- 受信した UTF-8 テキストを画面表示 (折返し + 履歴スクロール)
- USB HID キーボードとして自動入力
- 日本語・多バイト文字は Alt+X による Unicode 変換方式
- ボタン長押しでデバッグ出力 (HID経由) ON/OFF、さらに長押しでボーレート切替
- コード内マクロでタイミング微調整可能 (`ALT_X_POST_DELAY_MS` 等)

## ハードウェア接続
| Signal | AtomS3 Pin | QR Module | 備考 |
|--------|------------|-----------|------|
| RX     | GPIO5      | TX        | モジュール→M5 方向 |
| TX     | GPIO6      | RX        | M5→モジュール 方向 |
| TRIG   | GPIO7 (PullUp) | (任意) | 未使用でも可 |
| GND    | GND        | GND       | 共通 |
| 5V     | 5V         | 5V        | 電源 |

(本コードでは `TRIG_PIN` は入力 PullUp 設定のみ。必要に応じて将来トリガ制御に利用できます。)

ATOMicモジュールとの通信速度は、115200bpsとしていますが、デフォルトのままだと9600bpsなので、
[モジュールのマニュアル](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/atombase/AtomicQR/AtomicQR_Reader_EN.pdf) にあるバーコードを読み取らせて設定変更する必要があります。


## ビルド & アップロード
PlatformIO (VS Code) で以下を実行:
1. `platformio.ini` で環境: `[env:m5stack-atoms3]`
2. 接続後 `Upload` 実行

## 操作方法 (本体ボタン)
| アクション | 動作 |
|------------|------|
| 短押し | スキャン開始 / 停止 切替 |
| 約1.5秒長押し | デバッグモード (HID経由の `[DECODE]` 出力) トグル |
| 約3秒長押し | UARTボーレート切替 (115200 ↔ 9600) |

## デバッグ出力
`debugMode` 有効時、HIDキーボードとして:
- `[DECODE]` : 入力テキストの Unicode コードポイント列
- `[QR]` / `[QRp]` : フレーム / タイムアウト確定で受信した生バイト列 (Hex) 表示

(シリアルモニタは HID 動作と排他になるため使用していません)

## 文字入力方式 (Alt+X)
1. ASCII (`0x20-0x7E`) は通常のキー入力
2. 非ASCII は: 先頭にバリアスペース挿入 → HEX(大文字) → `Alt+X` → スペース除去 (Left×2, Delete, Right)
3. 変換後、次の文字へ続行

### Windows 環境での Alt+X / EnableHexNumpad について
- 本プロジェクトは「文字の16進コードをタイプ → Alt+X」で Unicode 変換する Windows 標準の Alt+X 方式を利用します。
- 一般的な日本語環境 (IME 有効) では追加設定なしで利用可能です。
- もし一部環境で Alt+X が反応しない場合や、NumPad での 0x(先頭ゼロ) 付き入力などを拡張したい場合は `EnableHexNumpad` レジストリを有効化してみてください。

| 項目 | 目的 | 既定 | 必須か |
|------|------|------|--------|
| EnableHexNumpad | Alt + (テンキー 16進) による Unicode 入力拡張 | 多くの環境で未定義 (＝無効) | Alt+X 方式自体には不要 |

`Alt+X` と `Alt+(テンキーコード)` は別機能です。本ファームウェアは Alt+X を使うので通常この設定は不要ですが、検証や他方式へ切替する際の参考として記載します。

#### レジストリ (EnableHexNumpad) 追加手順
1. レジストリエディタで `HKEY_CURRENT_USER\Control Panel\Input Method` を開く
2. 文字列値 (REG_SZ) 名 `EnableHexNumpad` を作成
3. 値に `1` を設定
4. サインアウト or 再起動

#### PowerShell ワンライナー (現在ユーザ)
以下を 1 行で実行すると作成/更新されます:
```powershell
New-Item -Path 'HKCU:\Control Panel\Input Method' -Force | Out-Null; New-ItemProperty -Path 'HKCU:\Control Panel\Input Method' -Name EnableHexNumpad -Value 1 -PropertyType String -Force | Out-Null; Write-Host 'EnableHexNumpad=1 set. Sign out to apply.'
```
無効化するには:
```powershell
Remove-ItemProperty -Path 'HKCU:\Control Panel\Input Method' -Name EnableHexNumpad -ErrorAction SilentlyContinue; Write-Host 'EnableHexNumpad removed. Sign out to apply.'
```

> 注意: グループポリシーや企業管理端末では書き込みが制限される場合があります。

### 調整可能パラメータ
| マクロ | 既定値 | 説明 |
|--------|--------|------|
| `ALT_X_POST_DELAY_MS` | 5 | Alt+X 解放後の確定待ち (アプリ相性で 20～40 に増やすと安定する場合あり) |
|

## 既知の制限
- Alt+X をサポートしない環境/アプリでは多バイト文字が入力できません
- 補助面 (U+10000 以上) 文字はホスト環境フォントに依存
- 長大テキスト連続入力では Alt+X 処理がボトルネックになり速度低下
- IME / エディタによってはスペース除去手順が異なる挙動を見せる可能性あり
- 現状コードで、Microsoft Wordにおいてはほぼ問題なく入力ができるようです。しかしメモ帳では一部文字化けする場合があります。

## ライセンス
本リポジトリで追加したソース（`src/main.cpp` など生成/編集部分）は MIT ライセンスで配布可能です。下記 `LICENSE` を参照してください。

本プロジェクトは以下に依存しています:
- M5Unified (M5Stack, Apache License 2.0)
- ESP32 Arduino Core (ESP-IDF / Arduino, 各ライセンス条項)

これら第三者ライブラリのライセンス条件も遵守してください。

## 免責
本コードはサンプルです。運用環境で使用する場合は入力対象アプリとの相性やセキュリティポリシーを確認してください。

## 謝辞
本コードは、その多くをGitHub CoPilotのアシストのもとで作成しました。

---
Happy Hacking!
