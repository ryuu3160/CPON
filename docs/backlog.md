# backlog

2026-11-16 のレビューで見つかり、まだ手を付けていないもの。

- `cpon_block::GetValue<T>` が、キーが無い・型が違うときに null 参照を返す(未定義動作)。例外にするかは公開 API の変更。`CPON/src/cpon_object.hpp`
- `cpon_block` が親オブジェクトのヒント文字列を参照で持つ。ブロックの `shared_ptr` だけを持ち続けると、親が消えたあと無効な参照になる。`CPON/src/cpon_object.hpp`(`m_BlockHintsRef`)
- `cpon_object::operator[]` が、負のインデックスで未定義動作、範囲外で黙って最後のブロックを返す。空のときに作るブロックの深さも `m_NestedLevel + 1` になっていない。`CPON/src/cpon_object.cpp`
- バイナリの読み込みで、ファイルに書かれた要素数やサイズをそのまま `reserve` に使う。細工されたファイルで `bad_alloc` が外に出る。ネストの深さにも上限が無い。`CPON/src/cpon_binary_helper.cpp`、`CPON/src/cpon.cpp`(`Deserialize`)
- キー名・オブジェクト名に `:` `,` `[` `{` 改行があると、テキストでもバイナリでも壊れる。保存時に拒否するか、エスケープする。
- `cpon_binary_helper::DeserializeValue` の配列6種類がほぼ同じ形。オブジェクトの読み込みも `cpon::Deserialize` と重複している。
- ヘッダーの作法: ヘッダー内の無名名前空間(`TypeValue`)、`CPP_STD` マクロの漏れ、`<iostream>` の include と `std::cerr` への直接出力、`<sal.h>` を include せずに `_In_` を使っている。
- `m_NestedLevel`(`cpon_block`、`cpon_object`)は、テキストの書き込みが深さを自分で数えるようになったら使われなくなる。消す。
- `cpon::WriteToFile` が書き込み中に失敗(メモリ不足など)すると、途中まで書いたファイルが残る。一時ファイルに書いて置き換える。`CPON/src/cpon.cpp`(WriteToFile)(001)
