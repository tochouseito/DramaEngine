# 会話記録: カスタムコマンド /save の作成

**日付**: 2025-12-18

## 議論した内容
- 以前作成したカスタムコマンド `[!save]` について
- 会話記録をSerenaのメモリに保存するためのコマンド

## 実装した変更
- `.claude/commands/save.md` を作成
- `/save` コマンドで会話の重要な情報をSerenaのメモリに保存する機能

## コマンドの使い方
1. 会話の最後に `/save` を実行
2. 重要な情報がSerenaのメモリに保存される
3. 次回の会話で `list_memories` と `read_memory` で確認可能

## 保存される内容
- 議論したトピック
- 決定事項
- 実装した変更点
- 今後のタスクやTODO

## 備考
- カスタムコマンドはセッション再起動後に有効になる場合がある
- プロジェクト: DramaEngine (C:\Users\sinse\source\repos\DramaEngine)
