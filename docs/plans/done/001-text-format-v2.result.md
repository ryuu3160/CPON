# 結果: 001-text-format-v2

計画: docs/plans/done/001-text-format-v2.md
設計書: docs/design.md
範囲: 758e876..9a24125
振り返り: まだ

## 判断(進行役が実装中に決めたこと)
- [タスク 2] v1 のネストで、読み位置がまったく進まないブロックが最後以外にあれば失敗にする(計画に無い防御を追加) — 理由: ネストの巨大なブロック数で空ブロックを大量に作るのを防ぐ。計画の手順1は EOF のときしか守れなかった — 影響: 間違っていても、位置が進まないブロックが続く壊れたファイルを拒否するだけ。空行で区切られた空ブロックは読める — 関連: CPON/src/cpon_text.cpp / 設計書「v1 の読み方」
- [全体] ヒントのキーが重複するオブジェクトは、Write が同じキーを1回だけ書く(false にはしない) — 理由: 保存できないデータを作らないため。最終レビューの指摘 — 影響: 間違っていても、重複したヒントの2つ目が黙って無視されるだけ — 関連: CPON/src/cpon_text.cpp(WriteBlocks) / 設計書「エラー処理と境界条件」

## 保留(直しきれなかった指摘)
- なし

## 軽微(記録だけした指摘)
- [タスク 1] size_t を使うのに <cstddef> を include していない — CPON/src/cpon_hints.hpp:5
- [タスク 1] ParseHints 失敗後に Out_Entries が空かをテストが確かめていない — CPON/src/cpon_hints.hpp
- [タスク 2] cpon_object::SetHints が noexcept なのに std::string を確保する。メモリ不足で terminate になる — CPON/src/cpon_object.hpp:459
- [タスク 2] v1 はネストの最初のブロックが空だと、次のブロックの値が前にずれて読まれる(形式の曖昧さ。v2 で解消) — CPON/src/cpon_text.cpp
- [タスク 3] WriteBlocks は 0 ブロックのオブジェクトでも深さ 64 超で false。読み込み側は 0 ブロックなら深さを見ないので非対称 — CPON/src/cpon_text.cpp
- [タスク 3] ヒントに同じキーが重複すると Write が同じ項目を2回書く — CPON/src/cpon_text.cpp(WriteBlocks)
- [タスク 4] Write はオブジェクト名に [ や改行があっても拒否しない。読み戻せないファイルができうる(backlog に既出) — CPON/src/cpon_text.cpp(Write)
- [全体] WriteToFile はファイルへの書き込み中にメモリ不足になると、途中まで書いたファイルが残る — CPON/src/cpon.cpp:135

## タスク
- タスク 1: 完了(758e876..6a4ad95、承認)
- タスク 2: 完了(6a4ad95..fddf1c1、承認)
- タスク 3: 完了(fddf1c1..d12e950、承認)
- タスク 4: 完了(d12e950..845cdd7、修正1回で承認)
- タスク 5: 完了(845cdd7..5c45621、最終レビュー送り)
