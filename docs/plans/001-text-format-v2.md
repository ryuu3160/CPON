# 実装計画: テキスト形式 v2

設計書: docs/design.md
目的: `.cpon` テキスト形式で、書いたデータが必ず読み戻せるようにし、壊れたファイルは例外ではなく `false` で返す。
構成: テキストの読み書きを `cpon` から内部用の `cpon_text_helper`(`cpon_text.hpp` / `cpon_text.cpp`)に移す。書き込みは常に新形式(v2)。読み込みは1行目の `#Version` で v2 と旧形式(v1)を切り替える。ヒント文字列の分解は `cpon_hints.hpp` の1か所にまとめる。
規約スキル: cpp-conventions

## 全体の制約
- C++20、MSVC。`CPON\Test\build_test.bat` の出力に warning を出さない。
- `CPON/src/` の既存の `.hpp` / `.cpp` は Shift-JIS(CP932)、改行は CRLF。Edit / Write ツールでは編集できない。python で `open(p,'rb').read().decode('cp932')` → 置換 → `encode('cp932')` で書き戻す(置換する文字列の改行は `\r\n`、置換前の文字列がちょうど1か所に一致することを assert する)。
- 新しく作るソースは ASCII だけで書き、改行は CRLF にする。コメントは英語。
- `CPON/Test/test_binary.cpp` は ASCII、CRLF。Edit ツールで編集できる。テストの断片ファイル(`docs/plans/001-text-format-v2/tests/`)は LF なので、挿入するときに CRLF にそろえる。
- 既存の命名に合わせる: 引数は `In_` / `Out_` / `InOut_`、メンバー変数は `m_`、SAL 注釈(`_In_` など)を付ける。
- `cpon` クラスの公開メンバー関数は増やさない、減らさない。バイナリ形式(`Serialize` / `Deserialize` の出力)は変えない。
- 例外を `cpon::LoadFromFile` / `cpon::WriteToFile` の外に出さない。
- ファイルから読んだ数(オブジェクト数、ブロック数、配列の要素数)を `reserve` や `resize` に使わない。
- テストの断片を挿入する位置は2つ。「名前空間の中」= `test_binary.cpp` の `} // namespace` の行の直前。「main の中」= `// Corrupt / truncated input must be rejected, not crash` の行の直前。
- コミットはタスクごとに1つ。`CPON.nupkg/` と `CLAUDE.md` は git の管理外で、触らない。

## レビュー重点
- 壊れた旧形式のファイル(`[` `]` `{` `}:` `:` が無い行) → 例外を出さず `false`(テスト: タスク 2 の `TestV1Rejected`)
- ファイルに書かれた巨大な数(ブロック数、要素数、オブジェクト数が 1000000000) → 確保せず、すぐ `false`(テスト: タスク 2 の `v1-rej-huge-block-count` / `v1-rej-huge-object-count`、タスク 4 の `v2-rej-block-count-huge` / `v2-rej-array-count-huge`)
- 読み込みに失敗したとき → 読む前に持っていたデータがそのまま残る(テスト: タスク 2 の `ExpectRejected` が全件で確かめる)
- NUL、Ctrl-Z(0x1A)、改行、`\` を含む文字列 → テキストモードのファイルを通しても同じバイト列で戻る(テスト: タスク 3 の `v2-string-escapes` と、`large-100k` のテキスト往復)
- 書けないデータ(深さ 65) → `WriteToFile` が `false` を返し、同じパスの既存ファイルを変えない(テスト: タスク 4 の `TestV2WriteRefused`)

## タスク

### タスク 1: ヒント文字列の分解を1か所にまとめる
- レビュー: 通常

**ファイル**
- 作成: `CPON/src/cpon_hints.hpp`
- 変更: `CPON/src/cpon_binary_helper.cpp`(該当箇所: `cpon_binary_helper::SerializeObject`)、`CPON/CPON.vcxproj` と `CPON/CPON.vcxproj.filters`(`ClInclude` に `src\cpon_hints.hpp` を足す)
- テスト: `CPON/Test/test_binary.cpp`

**インターフェース**
- 使う: なし
- 作る(`CPON/src/cpon_hints.hpp`、`#pragma once`、ヘッダーのみ):
  ```cpp
  namespace cpon_detail
  {
      struct HintEntry { std::string Key; std::string Type; };
      // "Name:string,Scores:array<int>" -> {{"Name","string"},{"Scores","array<int>"}}
      // Out_Entries is replaced. Returns false when an entry has no ':', an empty key or an empty type
      // (this includes a leading or trailing ','). An empty string succeeds with 0 entries.
      [[nodiscard]] inline bool ParseHints(std::string_view In_Hints, std::vector<HintEntry> &Out_Entries);
  }
  ```

**テスト**(先に置いて、失敗することを確かめる)
- `test_binary.cpp` の include に `#include "../src/cpon_hints.hpp"` を足す。
- 追記: `docs/plans/001-text-format-v2/tests/task-1/namespace.cpp` → `CPON/Test/test_binary.cpp`(名前空間の中)
- main の中に `TestParseHints(ran);` を足す。

**実装**
- 項目は `,` で区切り、各項目は最初の `:` でキーと型に分ける。
- `SerializeObject` のヒントを1項目ずつ切り出しているループを `ParseHints` に置き換える。ヒントが空、または `ParseHints` が `false` のときは、今の「ヒントが空」の経路(`m_BlockData` を順に全部書く)を通す。出力のバイト列は変えない。

**完了条件**
- 全体: `CPON\Test\build_test.bat` → 最後の行が `ALL TESTS PASSED`、warning なし

### タスク 2: 旧形式(v1)の読み込みを cpon_text_helper に移す
- レビュー: 難所

**ファイル**
- 作成: `CPON/src/cpon_text.hpp`、`CPON/src/cpon_text.cpp`
- 変更: `CPON/src/cpon.cpp`(`LoadFromFile`、読み込み用の非公開関数と `FindLineKey` を消す)、`CPON/src/cpon.hpp`(`ReadObject`、`ReadObjectName`、`ReadObjectDataCount`、`ReadBlockInfo`、`ReadHintInfo`、`ReadBlockValue`、`ReadBlockArray`、`CreateArrayByType`、`FromStr` の宣言を消す)、`CPON/src/cpon_object.hpp`(`cpon_block` と `cpon_object` に `friend class cpon_text_helper;` を足す)、`CPON/CPON.vcxproj` と `CPON/CPON.vcxproj.filters`(`cpon_text.cpp` / `cpon_text.hpp` を足す)、`CPON/Test/build_test.bat`(2つの `cl` の行に `"%SRC%\cpon_text.cpp"` を足す)
- テスト: `CPON/Test/test_binary.cpp`

**インターフェース**
- 使う: `cpon_detail::ParseHints`(タスク 1)
- 作る(`CPON/src/cpon_text.hpp`。内部用。`cpon_object.hpp` を include する):
  ```cpp
  class cpon_text_helper
  {
  public:
      // Reads the whole stream. On success replaces Out_Objects and returns true.
      // On failure returns false and leaves Out_Objects untouched. Never throws.
      [[nodiscard]] static bool Read(_In_ std::istream &In_Stream, _Out_ std::vector<std::shared_ptr<cpon_object>> &Out_Objects);
  private:
      // private static helpers may be added freely (they need the friend access)
  };
  ```
- `cpon_text.cpp` の中で、タスク 3 と 4 が使う定数: `inline constexpr int kMaxDepth = 64;`(最上位のオブジェクトのブロックが深さ 1)

**テスト**(先に置いて、失敗することを確かめる)
- `test_binary.cpp` の include に `#include <fstream>` を足す。
- 追記: `docs/plans/001-text-format-v2/tests/task-2/namespace.cpp` → `CPON/Test/test_binary.cpp`(名前空間の中、タスク 1 で足した断片のあと)
- main の中に `TestV1Fixtures(ran);` と `TestV1Rejected(ran);` を足す。

**実装**
- `cpon::LoadFromFile`: 拡張子の補完とファイルを開く処理は今のまま。開けたら、一時的な `std::vector<std::shared_ptr<cpon_object>>` に `cpon_text_helper::Read` で読み、`true` のときだけ `m_Objects` を入れ替える。`Read` の呼び出しは `try` / `catch(...)` で囲み、例外は `false` にする。失敗したら `std::cerr` に1行出す(今と同じ文面でよい)。
- `Read` は、最初に全行を `std::getline` で `std::vector<std::string>` に読み、位置を進めながら解析する(行を消費せずに先を見られるようにする)。
- 1行目: `#ObjNum : ` を含まなければ失敗。`#Version` を含んでいたら失敗(v2 はタスク 3 で足す)。オブジェクト数は `: ` のあとを `std::from_chars` で読む(読めなければ 0)。
- ヘッダー行(最上位: `名前[数]{ヒント}:`、ネスト: `[数]{ヒント}:`)は、`[`、そのあとの `]`、そのあとの `{`、そのあとの `}:` を順に探し、1つでも無ければ失敗。数は `from_chars`(読めなければ 0、負ならブロックなし)。ヒントは `ParseHints` で分解し、`false` なら失敗。
- オブジェクトを作ったら、ブロックを読む前に `SetHints(ヘッダーのヒント文字列)` を呼ぶ(ヒントの順をファイルと同じにする)。
- v1 のブロックの読み方(深さ d。d が `kMaxDepth` を超えたら失敗):
  1. ブロックごとに: 残りの行が無ければ失敗。今の行が空行なら1行進める(1回だけ)。
  2. ヒントの位置 h を 0 から始める。残りの行が無い、または今の行が空行なら、ブロックの終わり(行は消費しない)。
  3. 行のキー(先頭の空白とタブを飛ばした位置から、最初の `:` の手前まで。`:` が無ければキーなし)が、h 以降のどのヒントのキーと一致するかを前から探す。
  4. 一致が無いとき: d が 1(最上位のオブジェクト)なら失敗。ネストなら、ブロックの終わり(行は消費しない)。
  5. 一致したら行を消費し、`キー:` のあとを値として、そのヒントの型で読む。h を一致した位置の次にして、2 に戻る。
  6. ブロックのあと: d が 1 で、今の行が空行なら1行進める。
- v1 の値の読み方: `string` はそのまま(エスケープなし)。`int` / `uint` / `float` / `double` は `from_chars` で読み、読めなければ 0(後ろの余分な文字は無視)。`bool` は `true` を含めば真。`array<T>` は `[` と `]` が無ければ失敗。要素数 n は `from_chars`(読めなければ 0)。`]` のあとを `,` で分け、各要素の先頭の空白を1つだけ取り除く。分けた個数が n より少なければ失敗、多ければ先頭の n 個を使う。n が 0 以下なら空の配列。`object` は、行の残りをネストのヘッダーとして読み、`cpon_block::CreateObject(キー)` で作って深さ d+1 で読む。これ以外の型は失敗。
- 書き込み側(`WriteToFile` と書き込み用の非公開関数)は、このタスクでは触らない。

**完了条件**
- 全体: `CPON\Test\build_test.bat` → 最後の行が `ALL TESTS PASSED`、warning なし

### タスク 3: v2 の書き込みと読み込み
- レビュー: 通常

**ファイル**
- 変更: `CPON/src/cpon_text.hpp`、`CPON/src/cpon_text.cpp`、`CPON/src/cpon.cpp`(`WriteToFile`、`ClearObjectsData`、書き込み用の非公開関数と `FloatToStr` を消す)、`CPON/src/cpon.hpp`(`CreateFileHeader`、`IsStringNpos`、`WriteObjectHeader`、`WriteDataBlocks`、`WriteDataBlockValue`、`WriteDataBlockArray`、`CountElement`、`ToStr`、`m_FileHeader` を消す)、`CPON/src/cpon_object.hpp`(`cpon_block::GetElementAsStringVisitor` を消す。ほかで使われていないことを grep で確かめてから)
- テスト: `CPON/Test/test_binary.cpp`

**インターフェース**
- 使う: `cpon_text_helper::Read`、`kMaxDepth`(タスク 2)、`cpon_detail::ParseHints`(タスク 1)
- 作る(`cpon_text_helper` の public に足す):
  ```cpp
  // Writes the v2 text format. Returns false, possibly after writing part of the output, when:
  // an object pointer is null, an object's hints cannot be parsed, nesting is deeper than kMaxDepth,
  // or the stream fails.
  [[nodiscard]] static bool Write(_In_ std::ostream &In_Stream, _In_ const std::vector<std::shared_ptr<cpon_object>> &In_Objects);
  ```

**テスト**(先に置いて、失敗することを確かめる)
- `test_binary.cpp` の include に `#include <sstream>` と `#include <memory>` を足す。
- 追記: `docs/plans/001-text-format-v2/tests/task-3/namespace.cpp` → `CPON/Test/test_binary.cpp`(名前空間の中、タスク 2 で足した断片のあと)
- main を次のように変える:
  - `MakeCases()` を回す最初のループに、`RoundTripFile(c);` のあと `RoundTripText(c);` を足す。
  - `textCases` に `MakeFloatTextCases()` を足しているループのあとに、同じ形で `MakeTextV2Cases()` を足す。
  - main の中に `TestV2ExactOutput(ran);` と `TestV2Fixtures(ran);` を足す。

**実装**
- 形式は設計書の「v2 の形式」のとおり。期待する出力の全文が `TestV2ExactOutput` にある。
- `cpon::WriteToFile`: 拡張子の補完は今のまま。`std::ostringstream` に `Write` で書き、`true` のときだけファイルを開いて(テキストモード、`trunc`)中身を書く。`false` のときはファイルを開かない。`Write` の呼び出しは `try` / `catch(...)` で囲む。
- インデントは再帰の深さから決める(`m_NestedLevel` は読まない)。ブロックの行はヒントの順に書き、そのブロックに無いキーは書かない。
- 数値は `std::to_chars`(`float` / `double` は書式を指定しない最短表現)。配列の区切りは `,`(空白なし)。
- 文字列のエスケープはバイト単位: `\` → `\\`、0x0A → `\n`、0x0D → `\r`、`,` → `\,`、それ以外の 0x00〜0x1F(0x09 を除く)→ `\xHH`(大文字の16進2桁)。それ以外のバイトはそのまま。
- `Read`: 1行目が `#Version` を含むとき、行全体が `#ObjNum : <N> #Version : 2`(区切りは半角空白1つずつ、N は0以上の10進)でなければ失敗。合っていれば v2 として読む。`#Version` を含まなければ、タスク 2 の v1 の読み方。
- v2 の読み方(深さ d のブロック。インデントは行頭の半角空白の数):
  1. オブジェクトのヘッダーは、最上位が `名前[数]{ヒント}:`、ネストが `キー:` のあとに `[数]{ヒント}:`。作ったら `SetHints` を呼び、ブロックを「数」個読む。
  2. ブロック: 行が無い、空行、インデントが 2×d より小さい、のどれかになるまで行を読む(終わりの行は消費しない)。各行は、インデントがちょうど 2×d で、キーがヒントにあること(ヒントの順でなくてよい)。値はヒントの型で読む。
  3. 最後以外のブロックのあと: 今の行が空行なら1行進める。空行でなければ失敗。
  4. 最上位のオブジェクトの最後のブロックのあと: 今の行が空行なら1行進める。行が残っていて空行でなければ失敗。
  5. `#ObjNum` 個のオブジェクトを読んだあと、空行以外の行が残っていれば失敗。
- v2 の値: `string` はエスケープを戻す(エスケープされていない `,` はそのまま `,`)。配列は `[<n>]` のあとを、エスケープされていない `,` で分ける。n が 0 なら残りは空でなければならない。n が 1 以上なら、分けた個数が n と一致しなければ失敗。`bool` は `true` か `false` と完全に一致。数値は `from_chars` が文字列の最後まで読み切ること。
- このタスクでは、正しいファイルが読めればよい。上に書いた以外の失敗の検出(同じキーの重複、深さの上限、ヘッダーの細かい形など)はタスク 4 で仕上げる。

**完了条件**
- 全体: `CPON\Test\build_test.bat` → 最後の行が `ALL TESTS PASSED`、warning なし

### タスク 4: v2 の壊れた入力を拒否する
- レビュー: 難所

**ファイル**
- 変更: `CPON/src/cpon_text.cpp`
- テスト: `CPON/Test/test_binary.cpp`

**インターフェース**
- 使う: `cpon_text_helper::Read` / `Write`、`kMaxDepth`(タスク 2、3)。テストは `ExpectRejected`、`Report`(タスク 2 の断片)と `ReadWholeTextFile`(タスク 3 の断片)を使う。
- 作る: なし

**テスト**(先に置いて、失敗するものがあることを確かめる)
- 追記: `docs/plans/001-text-format-v2/tests/task-4/namespace.cpp` → `CPON/Test/test_binary.cpp`(名前空間の中、タスク 3 で足した断片のあと)
- main の中に `TestV2Rejected(ran);` と `TestV2WriteRefused(ran);` を足す。

**実装**
- v2 の読み込みで、次をすべて失敗(`false`)にする。どれも例外を出さない。
  - ヘッダー行: `[`、`]`、`{`、`}:` がこの順に無い。`}:` のあとに文字がある。数が10進の0以上の整数でない。ヒントを `ParseHints` で分解できない。ヒントの型が `string` / `int` / `uint` / `float` / `double` / `bool` / `object` / `array<` + 左の6つの型のどれか + `>` のどれでもない。
  - ブロックの行: インデントが 2×d より大きい。`:` が無い。キーがヒントに無い。同じブロックに同じキーが2回ある。
  - 値: 数値が空、前後に余分な文字がある、型の範囲に入らない(`uint` に `-1` など)。`bool` が `true` / `false` と完全に一致しない。`object` の値がヘッダーの形でない。
  - 配列: `[` で始まらない、`]` が無い、要素数が0以上の整数でない、分けた個数が合わない、要素が型として読めない。
  - エスケープ: `\` のあとが `\`、`n`、`r`、`,`、`x` + 16進2桁 のどれでもない。文字列が `\` 1つで終わる。
  - 深さ d が `kMaxDepth` を超える。
- `Write`: 深さが `kMaxDepth` を超えるデータは `false`。

**完了条件**
- 全体: `CPON\Test\build_test.bat` → 最後の行が `ALL TESTS PASSED`、warning なし

### タスク 5: README の形式の説明を v2 に合わせる
- レビュー: 最終のみ

**ファイル**
- 変更: `README.md`(該当箇所: `#ObjNum` と `ObjectName[N]{...}:` を説明している節と、その例)

**実装**
- 1行目が `#ObjNum : <数> #Version : 2` になったこと、配列の区切りが `,`(空白なし)になったこと、文字列のエスケープ(`\\`、`\n`、`\r`、`\,`、`\xHH`)を、今の説明と例に反映する。
- `#Version` の無い旧形式のファイルも読めること、書き込みは常に新形式になること、新形式のファイルは 1.4.x 以前では読めないことを、2〜3行で足す。
- README の文字コードと改行は、今のまま保つ。

**完了条件**
- 対象テスト: `CPON\Test\build_test.bat` → 最後の行が `ALL TESTS PASSED`
