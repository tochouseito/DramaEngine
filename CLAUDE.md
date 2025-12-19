# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## プロジェクト概要

DramaEngineはC++20で書かれたWindowsゲームエンジンです。Visual Studio 2022 (v145ツールセット) を使用し、x64プラットフォームのみをサポートしています。

## ビルド

```powershell
# Visual Studio Developer Command Promptで実行
msbuild DramaEngine.slnx /p:Configuration=Debug /p:Platform=x64
msbuild DramaEngine.slnx /p:Configuration=Develop /p:Platform=x64
msbuild DramaEngine.slnx /p:Configuration=Release /p:Platform=x64
```

### ビルド構成
- **Debug**: DLL出力、デバッグ情報あり
- **Develop**: DLL出力、最適化あり、インクリメンタルリンク有効
- **Release**: 静的ライブラリ出力、完全最適化

出力先: `generated/outputs/{Configuration}/`

## アーキテクチャ

### プロジェクト構成
- **Engine**: コアエンジンライブラリ (DLL/静的ライブラリ)
- **Editor**: Windowsアプリケーション、Engineに依存

### 名前空間
- `Drama`: エンジンコア
- `Drama::API`: エンジン生成・操作API (`CreateEngine`, `DestroyEngine`, `SetEngine`, `RunEngine`)

### DLLエクスポート
`DRAMA_API`マクロを使用。`ENGINE_EXPORTS`定義時はエクスポート、それ以外はインポート。

## コーディング規約 (CODING_RULES.md参照)

### 命名規則
| 種別 | 規則 | 例 |
|------|------|-----|
| 型 | PascalCase | `RenderEngine` |
| 関数 | snake_case | `get_device()` |
| 変数 | camelCase | `frameCount` |
| メンバ変数 | m_ + camelCase | `m_frameCount` |
| 定数 | k_ + camelCase | `k_maxBufferSize` |
| bool | 疑問形 | `isEnabled`, `hasData` |

頭字語は最初のみ大文字（例: `Http`, `Xml`）

### ファイル構成
- ファイル名: PascalCase
- フォルダ: `include/` (ヘッダ), `source/` (ソース), `shader/` (シェーダ)

### ヘッダ規約
- `#pragma once`使用
- `using namespace`禁止
- インクルード順: 自分のヘッダ → 標準ライブラリ → 外部ライブラリ

### 所有権・エラー処理
- 基本は`std::unique_ptr`、共有時のみ`std::shared_ptr`
- 生ポインタは非所有の意味でのみ使用
- 例外は使用しない（戻り値でエラーを返す）
- `new`/`delete`直書き禁止

### コメント言語
- C++: 日本語
- HLSL (.hlsli): 英語

### 自動整形
clang-format、Allmanスタイル
