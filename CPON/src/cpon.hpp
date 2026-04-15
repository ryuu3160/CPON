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

/**
 * @brief CPONクラス
 */
class cpon
{
public:
	cpon() = default;
	~cpon() = default;

	/**
	 * @brief 指定したインデックスにあるオブジェクトへの参照を返す配列添字演算子
	 * @param In_Index アクセスするオブジェクトのインデックス
	 * @return 指定したインデックスにあるcpon_objectへの参照
	 */
	cpon_object &operator[](_In_ int In_Index);

	/**
	 * @brief 指定した名前に対応するcpon_objectへの参照を返す配列アクセス演算子
	 * @param In_ObjectName 取得するオブジェクトの名前を表す文字列ビュー
	 * @return 指定された名前に対応するcpon_objectへの参照
	 */
	cpon_object &operator[](_In_ std::string_view In_ObjectName);

	/**
	 * @brief 指定された名前のオブジェクトへのシェアポインタを返す関数
	 * @param In_ObjectName 作成するオブジェクトの名前を表す文字列ビュー
	 * @return 指定された名前のcpon_objectへのシェアポインタ(オブジェクトが存在しない場合はnullptrを返す)
	 */
	std::shared_ptr<cpon_object> GetObjectPtr(_In_ std::string_view In_ObjectName);

	/**
	 * @brief 指定された名前のオブジェクトを作成し、そのシェアポインタを返す関数
	 * @param In_ObjectName 作成するオブジェクトの名前を表す文字列ビュー
	 * @return 作成されたcpon_objectへのシェアポインタを返す
	 */
	std::shared_ptr<cpon_object> CreateObject(_In_ const std::string_view In_ObjectName);
	
	/**
	 * @brief 指定された名前のオブジェクトを作成する
	 * @brief 既に存在する場合は、そのオブジェクトを返す
	 * @param In_ObjectName 作成するオブジェクトの名前を表す文字列ビュー
	 * @return 作成された、または既に存在するcpon_objectへのシェアポインタを返す
	 */
	std::shared_ptr<cpon_object> TryCreateObject(_In_ std::string_view In_ObjectName);

	/**
	 * @brief 既に存在するオブジェクトを追加する
	 * @param In_Object 追加するcpon_objectへのシェアポインタ
	 */
	void AddObject(_In_ std::shared_ptr<cpon_object> In_Object) noexcept;
	
	/**
	 * @brief 格納されているオブジェクトの数を取得する
	 * @return 格納されているオブジェクトの数をint型で返す
	 */
	[[nodiscard]] int GetObjectCount() const noexcept { return static_cast<int>(m_Objects.size()); }

	/**
	 * @brief 格納されているすべてのオブジェクトデータをクリアする関数
	 */
	void ClearObjectsData() noexcept;

	/**
	 * @brief 格納されているオブジェクトが空であるかを確認する関数
	 * @return 格納されているオブジェクトが空である場合はtrue、そうでない場合はfalseを返す
	 */
	[[nodiscard]] bool IsEmpty() const noexcept { return m_Objects.empty(); }

	/**
	 * @brief 指定されたファイルパスにデータを書き込む関数
	 * @brief 拡張子が指定されていない場合は、自動的に".cpon"が付加される
	 * @param In_FilePath 書き込み先のファイルパスを表す文字列ビュー
	 * @return 書き込みに成功した場合はtrue、失敗した場合はfalseを返す
	 */
	bool WriteToFile(_In_ const std::string_view In_FilePath);

	/**
	 * @brief 指定されたファイルからデータを読み込む関数
	 * @brief 拡張子が指定されていない場合は、自動的に".cpon"が付加される
	 * @param In_FilePath 読み込むファイルのパスを表す文字列ビュー
	 * @return 読み込みに成功した場合はtrue、失敗した場合はfalseを返す
	 */
	bool LoadFromFile(_In_ const std::string_view In_FilePath);

private:

	/**
	 * @brief ファイルヘッダーを作成する関数
	 */
	void CreateFileHeader();

	/**
	 * @brief 指定された位置がstd::string::nposであるかどうかを確認する関数
	 * @param In_Pos 確認する位置の値を表すsize_t型の引数
	 * @return 位置がstd::string::nposと等しい場合はtrue、そうでない場合はfalseを返す
	 */
	bool IsStringNpos(_In_ const size_t In_Pos) const noexcept { return In_Pos == std::string::npos; }

	// ---------------------------------------------
	// ファイルの書き込みに関するヘルパー関数
	// ---------------------------------------------

	/**
	 * @brief オブジェクトヘッダーをファイルに書き込む関数
	 * @param In_File 書き込み先の出力ファイルストリームを表すstd::ofstreamへの参照
	 * @param In_Object ヘッダーを書き込むオブジェクトへの共有ポインタを表すstd::shared_ptr<cpon_object>
	 */
	void WriteObjectHeader(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object);

	/**
	 * @brief データブロックをファイルに書き込む関数
	 * @param In_File 書き込み先の出力ファイルストリームを表すstd::ofstreamへの参照
	 * @param In_Object 書き込むCPONオブジェクトへの共有ポインタを表すstd::shared_ptr<cpon_object>
	 */
	void WriteDataBlocks(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object);

	/**
	 * @brief データブロックの値をファイルストリームに書き込む関数
	 * @param In_File 書き込み先の出力ファイルストリームを表すstd::ofstreamへの参照
	 * @param In_Value 書き込むデータ値を表すcpon_block::DataValueへの定数参照
	 */
	void WriteDataBlockValue(_In_ std::ofstream &In_File, _In_ const cpon_block::DataValue &In_Value);

	/**
	 * @brief データブロックの配列をファイルストリームに書き込む関数
	 * @param In_File 書き込み先の出力ファイルストリームを表すstd::ofstreamへの参照
	 * @param In_Array 書き込むデータブロックの配列を表すcpon_block::Arrayへの定数参照
	 */
	void WriteDataBlockArray(_In_ std::ofstream &In_File, _In_ const cpon_block::Array &In_Array);

	// ---------------------------------------------
	// ファイルの読み込みに関するヘルパー関数
	// ---------------------------------------------

	/**
	 * @brief ファイルからオブジェクトを読み取る関数
	 * @param In_File 読み取り元の入力ファイルストリームを表すstd::ifstreamへの参照
	 * @param In_Line 処理する行の文字列ビュー
	 * @param In_Object 読み取ったデータを格納するcpon_objectへの共有ポインタ
	 * @param In_FilePath ファイルパスの文字列ビュー
	 * @return オブジェクトの読み取りに成功した場合はtrue、それ以外の場合はfalse
	 */
	bool ReadObject(_In_ std::ifstream &In_File, _In_ std::string_view In_Line, _In_ std::shared_ptr<cpon_object> In_Object, _In_ std::string_view In_FilePath);

	/**
	 * @brief 行からオブジェクト名を読み取る関数
	 * @param In_Line オブジェクト名を含む入力行の文字列ビュー
	 * @return 読み取られたオブジェクト名をstd::stringとして返す
	 */
	std::string ReadObjectName(_In_ const std::string_view In_Line) const;

	/**
	 * @brief 行からオブジェクトデータの数を読み取る関数
	 * @param In_Line オブジェクトデータの数を含む入力行の文字列ビュー
	 * @return 読み取られたオブジェクトデータの数をint型で返す
	 */
	int ReadObjectDataCount(_In_ const std::string_view In_Line);

	/**
	 * @brief 行からブロック情報を読み取る関数
	 * @param In_Line 解析する入力行の文字列ビュー
	 * @param Out_BlockNum ブロック番号を受け取る出力パラメータへの参照
	 * @param Out_BlockHints ブロックヒントを受け取る出力パラメータへの参照
	 */
	void ReadBlockInfo(_In_ const std::string_view In_Line, _Out_ int &Out_BlockNum, _Out_ std::string &Out_BlockHints);

	/**
	 * @brief ヒント情報を解析し、ヒントIDとヒントタイプを抽出する関数
	 * @param In_Hint 解析するヒント情報を表す文字列ビュー
	 * @param Out_HintID 抽出されたヒントIDの出力先
	 * @param Out_HintType 抽出されたヒントタイプの出力先
	 */
	void ReadHintInfo(_In_ const std::string_view In_Hint, _Out_ std::string &Out_HintID, _Out_ std::string &Out_HintType);

	/**
	 * @brief ヒントIDとヒントタイプを使用して、行からブロックの値を読み取る関数
	 * @param In_Block 値を読み取る対象のCPONブロックへの共有ポインター
	 * @param In_Line 解析する行
	 * @param In_HintID 値の識別子を示すヒントID
	 * @param In_HintType 値の型を示すヒントタイプ
	 */
	void ReadBlockValue(_In_ std::shared_ptr<cpon_block> In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType);

	/**
	 * @brief ヒントIDとヒントタイプを使用して、行からブロックの配列を読み取る関数
	 * @param In_Block 配列を読み取る対象のCPONブロックへの共有ポインター
	 * @param In_Line 解析する行
	 * @param In_HintID 配列の識別子を示すヒントID
	 * @param In_HintType 配列の型を示すヒントタイプ
	 */
	void ReadBlockArray(_In_ std::shared_ptr<cpon_block> In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType);

	// ---------------------------------------------
	// 文字列処理に関するヘルパー関数
	// ---------------------------------------------

	/**
	 * @brief 文字列内の特定の文字の出現回数を数える関数
	 * @param In_Data 検索対象の文字列ビュー
	 * @param In_CountTarget 数える対象の文字
	 * @return 指定された文字の出現回数
	 */
	int CountElement(_In_ const std::string_view In_Data, _In_ char In_CountTarget) const noexcept;

	/**
	 * @brief 型指定に基づいて配列を作成する関数
	 * @param In_Type 作成する配列の型を指定する文字列ビュー
	 * @return 指定された型に基づいて作成された配列
	 */
	cpon_block::Array CreateArrayByType(_In_ const std::string_view In_Type);

	/**
	 * @brief 値を文字列に変換する関数
	 * @tparam T 変換する値の型
	 * @param In_Value 文字列に変換する値
	 * @return 値の文字列表現
	 */
	template<typename T>
	std::string ToStr(_In_ T In_Value)
	{
		std::string Str;
		Str.resize(std::numeric_limits<size_t>::digits10 + 2);
		auto res = std::to_chars(Str.data(), Str.data() + Str.size(), In_Value);
		Str.resize(res.ptr - Str.data());
		return Str;
	}

	/**
	 * @brief 文字列を指定された型に変換する関数
	 * @tparam T 変換先の型
	 * @param In_Str 変換する文字列
	 * @return 変換された値
	 */
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
