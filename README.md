# CPON (C++ Object Notation)

**CPON** はTOON(Token-Oriented Object Notation)とTONL(Token-Optimized Notation Language)を参考にして作成した、<br>C++専用のデータプラットフォームです。<br>
本ライブラリは**MSVC**と**C++20**の環境向けに開発しました。

## 📚 ドキュメント
- 使い方・仕様は GitHub Wiki を参照してください： https://github.com/ryuu3160/CPON/wiki

## 🎉 最新リリース
- スタティックライブラリ(.lib)に変更
- 2つのビルドツールに対応(v143,v145)
- x64,Win32(x86)両方に対応
- MT,MTd,MD,MDd全てに対応
- v1.4.4: バイナリ保存(Serialize/WriteToBinaryFile)でデバッグビルド時にハングする不具合を修正

## 📊 CPONのフォーマット

### ファイルヘッダー
```cpon
#ObjNum : <オブジェクト数> #Version : 2
```

### オブジェクトヘッダ
```cpon
ObjectName[N]{Id:int,Name:string,Roles:array<string>,Contact:object}:
```
ネストしたオブジェクトは `Contact:[N]{Email:string,Phone:string}:` のように、キーの後ろに `:` を付けて書きます。

### 値の書き方
- 配列は `[要素数]` のあとに、要素を `,`(空白なし)で区切って書きます。
- 文字列は次のようにエスケープします(バイト単位)。
  - `\` は `\\`、改行は `\n`、復帰は `\r`、`,` は `\,`
  - タブ以外の 0x00〜0x1F の制御文字は `\xHH`(16進2桁)
- 文字列の先頭や末尾の空白は、そのまま保存され、読み込み時にも削られません。

### 全体構造
```cpon
#ObjNum : 2 #Version : 2
Users[2]{Id:int,Name:string,Roles:array<string>,Contact:object}:
  Id:1
  Name:Alice
  Roles:[2]editor,user
  Contact:[1]{Email:string,Phone:string}:
    Email:alice@example.com
    Phone:+123456789

  Id:2
  Name:Bob
  Roles:[3]admin,editor,user
  Contact:[1]{Email:string,Phone:string}:
    Email:bob@example.com
    Phone:+123456789

RoleList[3]{Name:string,Level:int}:
  Name:admin
  Level:3

  Name:editor
  Level:2

  Name:user
  Level:1

```

### 旧形式との互換性
- 1行目に `#Version` が無い旧形式(v1)のファイルも、今までどおり読み込めます。
- 書き込み(`WriteToFile`)は、常に新形式(v2)になります。
- 新形式のファイルは、1.4.x 以前のバージョンでは読み込めません。
---

## 動作環境

- **C++標準**：C++20
- **コンパイラ**：MSVC (Visual Studio 2022以降推奨)
- **OS**：Windows 10以降

---

## 📄 ライセンス

MITライセンス - 詳細はLICENSEファイルを確認してください。

---

<div align="center">

**CPON**: C++で読み取りやすいデータプラットフォームを作りたかっただけなんだな。

*結構自己満足みたいなところはある(　˙-˙　)*

</div>
