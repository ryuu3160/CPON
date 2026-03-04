/*+===================================================================
	File: cpon.hpp
	Summary: cpon(C++ Object Notation)のパーサークラス
			 cponは、TONLやTOONを参考にした、C++向けのデータ記述言語です。
	Author: ryuu3160
	Date: 2025/12/6 Sat PM 10:03:46 初回作成
===================================================================+*/
#pragma once

#if defined(_MSC_VER)
#define CPP_STD _MSVC_LANG
#else
#define CPP_STD __cplusplus
#endif

// もしC++20未満の環境であれば、警告文を表示してコンパイルを中止します
#if CPP_STD < 202002L
#error ("C++20以降のコンパイラが必要です。")
#endif

// ==============================
//	include
// ==============================
#include <string>
#include <vector>
#include "cpon_object.hpp"

/// <summary>
/// cponクラス
/// </summary>
class cpon
{
public:
	cpon() = default;
	~cpon() = default;

	/// <summary>
	/// 指定したインデックスにあるオブジェクトへの参照を返す配列添字演算子
	/// </summary>
	/// <param name="[In_Index]">アクセスするオブジェクトのインデックス</param>
	/// <returns>指定したインデックスにあるcpon_objectへの参照</returns>
	cpon_object &operator[](_In_ int In_Index);

	/// <summary>
	/// 指定した名前に対応する cpon_object への参照を返す配列アクセス演算子
	/// </summary>
	/// <param name="[In_ObjectName]">取得するオブジェクトの名前</param>
	/// <returns>指定された名前に対応するcpon_objectへの参照</returns>
	cpon_object &operator[](_In_ std::string_view In_ObjectName);

	/// <summary>
	/// 指定された名前のオブジェクトへのシェアポインタを返します
	/// </summary>
	/// <param name="[In_ObjectName]">取得するオブジェクトの名前</param>
	/// <returns>指定された名前のcpon_objectへのシェアポインタ</returns>
	std::shared_ptr<cpon_object> GetObjectPtr(_In_ std::string_view In_ObjectName);

	/// <summary>
	/// 指定された名前のオブジェクトを作成し、その参照を返します
	/// </summary>
	/// <param name="[In_ObjectName]">作成するオブジェクトの名前を表す文字列ビュー</param>
	/// <returns>作成された cpon_object への参照を返します</returns>
	std::shared_ptr<cpon_object> CreateObject(_In_ const std::string_view In_ObjectName);

	/// <summary>
	/// <para>指定された名前のオブジェクトを作成します</para>
	/// <para>既に存在する場合は、そのオブジェクトを返します</para>
	/// </summary>
	/// <param name="[In_ObjectName]">オブジェクト名</param>
	/// <returns>作成された、または既にあるオブジェクトへのシェアポインタ</returns>
	std::shared_ptr<cpon_object> TryCreateObject(_In_ std::string_view In_ObjectName);

	/// <summary>
	/// 既に存在するオブジェクトを追加します
	/// </summary>
	/// <param name="[In_Object]">cpon_objectへのシェアポインタ</param>
	void AddObject(_In_ std::shared_ptr<cpon_object> In_Object) noexcept;

	/// <summary>
	/// 格納されているオブジェクトの数を取得します
	/// </summary>
	/// <returns>オブジェクトの個数をint型で返します。</returns>
	[[nodiscard]] int GetObjectCount() const noexcept { return static_cast<int>(m_Objects.size()); }

	/// <summary>
	/// オブジェクトデータをすべてクリアします
	/// </summary>
	void ClearObjectsData() noexcept;

	/// <summary>
	/// オブジェクトデータが空であるかを確認します
	/// </summary>
	/// <returns>オブジェクトデータが空である場合はtrue、そうでない場合はfalseを返します</returns>
	[[nodiscard]] bool IsEmpty() const noexcept { return m_Objects.empty(); }

	/// <summary>
	/// <para>指定したファイルパスにデータを書き込みます</para>
	/// <para>ファイルの拡張子は".cpon"である必要があります</para>
	/// </summary>
	/// <param name="[In_FilePath]">書き込み先のファイルパスを表す入力用のstd::string_view</param>
	/// <returns>書き込みに成功した場合はtrue、失敗した場合はfalseを返します</returns>
	bool WriteToFile(_In_ const std::string_view In_FilePath);

	/// <summary>
	/// <para>指定したファイルからデータを読み込みます</para>
	/// <para>ファイルの拡張子は".cpon"である必要があります</para>
	/// </summary>
	/// <param name="[In_FilePath]">読み込むファイルのパスを示す文字列ビュー</param>
	/// <returns>読み込みに成功した場合はtrue、失敗した場合はfalseを返します</returns>
	bool LoadFromFile(_In_ const std::string_view In_FilePath);

private:

	/// <summary>
	/// ファイルヘッダーを作成します
	/// </summary>
	void CreateFileHeader();

	/// <summary>
	/// 指定された位置が std::string::npos であるかどうかを確認します
	/// </summary>
	/// <param name="[In_Pos]">確認する位置の値</param>
	/// <returns>位置が std::string::nposと等しい場合はtrue、それ以外の場合はfalse</returns>
	bool IsStringNpos(_In_ const size_t In_Pos) const noexcept { return In_Pos == std::string::npos; }

	// ---------------------------------------------
	// ファイルの書き込みに関するヘルパー関数
	// ---------------------------------------------

	/// <summary>
	/// オブジェクトヘッダーをファイルに書き込みます
	/// </summary>
	/// <param name="[In_File]">書き込み先の出力ファイルストリーム</param>
	/// <param name="[In_Object]">ヘッダーを書き込むオブジェクトへの共有ポインタ</param>
	void WriteObjectHeader(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object);

	/// <summary>
	/// データブロックをファイルに書き込みます
	/// </summary>
	/// <param name="[In_File]">書き込み先の出力ファイルストリーム</param>
	/// <param name="[In_Object]">書き込むCPONオブジェクト</param>
	void WriteDataBlocks(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object);

	/// <summary>
	/// データブロックの値を出力ファイルストリームに書き込みます
	/// </summary>
	/// <param name="[In_File]">書き込み先の出力ファイルストリーム</param>
	/// <param name="[In_Value]">書き込むデータ値</param>
	void WriteDataBlockValue(_In_ std::ofstream &In_File, _In_ const cpon_block::DataValue &In_Value);

	/// <summary>
	/// データブロックの配列をファイルストリームに書き込みます
	/// </summary>
	/// <param name="[In_File]">書き込み先の出力ファイルストリーム</param>
	/// <param name="[In_Array]">書き込むデータブロック配列</param>
	void WriteDataBlockArray(_In_ std::ofstream &In_File, _In_ const cpon_block::Array &In_Array);

	// ---------------------------------------------
	// ファイルの読み込みに関するヘルパー関数
	// ---------------------------------------------

	/// <summary>
	/// ファイルからオブジェクトを読み取ります
	/// </summary>
	/// <param name="[In_File]">読み取り元の入力ファイルストリーム</param>
	/// <param name="[In_Line]">処理する行の文字列ビュー</param>
	/// <param name="[In_Object]">読み取ったデータを格納するcpon_objectへの共有ポインタ</param>
	/// <param name="[In_FilePath]">ファイルパスの文字列ビュー</param>
	/// <returns>オブジェクトの読み取りに成功した場合はtrue、それ以外の場合はfalse</returns>
	bool ReadObject(_In_ std::ifstream &In_File, _In_ std::string_view In_Line, _In_ std::shared_ptr<cpon_object> In_Object, _In_ std::string_view In_FilePath);

	/// <summary>
	/// 行からオブジェクト名を読み取ります
	/// </summary>
	/// <param name="[In_Line]">オブジェクト名を含む入力行</param>
	/// <returns>読み取られたオブジェクト名</returns>
	std::string ReadObjectName(_In_ const std::string_view In_Line) const;

	/// <summary>
	/// 行からオブジェクトデータの数を読み取ります
	/// </summary>
	/// <param name="[In_Line]">解析する入力行</param>
	/// <returns>オブジェクトデータの数</returns>
	int ReadObjectDataCount(_In_ const std::string_view In_Line);

	/// <summary>
	/// 行からブロック情報を読み取ります
	/// </summary>
	/// <param name="[In_Line]">解析する入力行</param>
	/// <param name="[Out_BlockNum]">ブロック番号を受け取る出力パラメータ</param>
	/// <param name="[Out_BlockHints]">ブロックヒントを受け取る出力パラメータ</param>
	void ReadBlockInfo(_In_ const std::string_view In_Line, _Out_ int &Out_BlockNum, _Out_ std::string &Out_BlockHints);

	/// <summary>
	/// ヒント情報を解析し、ヒントIDとヒントタイプを抽出します
	/// </summary>
	/// <param name="[In_Hint]">解析するヒント情報</param>
	/// <param name="[Out_HintID]">抽出されたヒントIDの出力先</param>
	/// <param name="[Out_HintType]">抽出されたヒントタイプの出力先</param>
	void ReadHintInfo(_In_ const std::string_view In_Hint, _Out_ std::string &Out_HintID, _Out_ std::string &Out_HintType);

	/// <summary>
	/// ヒントIDとヒントタイプを使用して、行からブロックの値を読み取ります
	/// </summary>
	/// <param name="[In_Block]">値を読み取る対象のCPONブロックへの共有ポインター</param>
	/// <param name="[In_Line]">解析する行</param>
	/// <param name="[In_HintID]">値の識別子を示すヒントID</param>
	/// <param name="[In_HintType]">値の型を示すヒントタイプ</param>
	void ReadBlockValue(_In_ std::shared_ptr<cpon_block> In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType);
	
	/// <summary>
	/// ブロック配列を読み取ります
	/// </summary>
	/// <param name="[In_Block]">読み取り対象のCPONブロック</param>
	/// <param name="[In_Line]">処理する行</param>
	/// <param name="[In_HintID]">処理のヒント ID</param>
	/// <param name="[In_HintType]">処理のヒント型</param>
	void ReadBlockArray(_In_ std::shared_ptr<cpon_block> In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType);

	// ---------------------------------------------
	// 文字列処理に関するヘルパー関数
	// ---------------------------------------------

	/// <summary>
	/// 文字列内の特定の文字の出現回数を数えます
	/// </summary>
	/// <param name="[In_Data]">検索対象の文字列ビュー</param>
	/// <param name="[In_CountTarget]">数える対象の文字</param>
	/// <returns>指定された文字の出現回数</returns>
	int CountElement(_In_ const std::string_view In_Data, _In_ char In_CountTarget) const noexcept;

	/// <summary>
	/// 型指定に基づいて配列を作成します
	/// </summary>
	/// <param name="[In_Type]">作成する配列の型を指定する文字列ビュー</param>
	/// <returns>指定された型に基づいて作成された配列</returns>
	cpon_block::Array CreateArrayByType(_In_ const std::string_view In_Type);

	/// <summary>
	/// 値を文字列表現に変換します
	/// </summary>
	/// <typeparam name="[T]">変換する値の型</typeparam>
	/// <param name="[In_Value]">文字列に変換する値</param>
	/// <returns>値の文字列表現</returns>
	template<typename T>
	std::string ToStr(_In_ T In_Value)
	{
		std::string Str;
		Str.resize(std::numeric_limits<size_t>::digits10 + 2);
		auto res = std::to_chars(Str.data(), Str.data() + Str.size(), In_Value);
		Str.resize(res.ptr - Str.data());
		return Str;
	}

	/// <summary>
	/// 文字列を指定された型に変換します
	/// </summary>
	/// <typeparam name="[T]">変換先の型</typeparam>
	/// <param name="[In_Str]">変換する文字列</param>
	/// <returns>変換された値</returns>
	template<typename T>
	T FromStr(_In_ const std::string_view In_Str)
	{
		T Value;
		std::from_chars(In_Str.data(), In_Str.data() + In_Str.size(), Value);
		return Value;
	}

private:
	std::string m_FileHeader;
	std::vector<std::shared_ptr<cpon_object>> m_Objects;
};
