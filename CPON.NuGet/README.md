# CPON (C++ Object Notation)

**CPON** はTOON(Token-Oriented Object Notation)とTONL(Token-Optimized Notation Language)を参考にして作成した、<br>C++専用のデータプラットフォームです。<br>
本ライブラリは**MSVC**と**C++20**の環境向けに開発しました。

## 📚 ドキュメント
- 使い方・仕様は GitHub Wiki を参照してください： https://github.com/ryuu3160/CPON/wiki

## 🎉 最新リリース
- オブジェクトのネスト機能追加
- すでにあるオブジェクトをブロックデータに追加する機能追加

## 📊 CPONのフォーマット

### ファイルヘッダー
```cpon
#ObjNum : "ObjectNum"
```

### オブジェクトヘッダ
```cpon
ObjectName[N]{Id:int,Name:string,Roles:array<string>,Contact:object}:
```

### 全体構造
```cpon
#ObjNum : 2
Users[2]{Id:int,Name:string,Roles:array<string>,Contact:object}:
  Id:1
  Name:Alice
  Roles:[2]editor, user
  Contact[1]{Email:string,Phone:string}:
    Email:alice@example.com
    Phone:+123456789

  Id:2
  Name:Bob
  Roles:[3]admin, editor, user
  Contact[1]{Email:string,Phone:string}:
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
