# CPON (C++ Object Notation)

**CPON** はTOON(Token-Oriented Object Notation)とTONL(Token-Optimized Notation Language)を参考にして作成した、<br>C++専用のデータプラットフォームです。<br>
本ライブラリは**MSVC**と**C++20**の環境向けに開発しました。

## 📊 CPONのフォーマット

### ファイルヘッダー
```cpon
#ObjNum : "ObjectNum"
```

### オブジェクトヘッダ
```cpon
ObjectName[N]{Id:int,Name:string,Roles:array<string>}:
```

### 全体構造
```cpon
#ObjNum : 2
Users[2]{Id:int,Name:string,Roles:array<string>}:
  Id:1
  Name:Alice
  Roles:[2]editor, user

  Id:2
  Name:Bob
  Roles:[3]admin, editor, user

RoleList[3]{Name:string,Level:int}:
  Name:admin
  Level:3

  Name:editor
  Level:2

  Name:user
  Level:1
```
---

## 注意事項

現段階ではオブジェクトのネスト機能が実装されていません。

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
