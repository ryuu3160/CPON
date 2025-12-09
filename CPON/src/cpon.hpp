/*+===================================================================
	File: cpon.hpp
	Summary: cpon(C++ Object Notation)のパーサークラス
			 cponは、TONLやTOONを参考にした、C++向けのデータ記述言語です。
	Author: ryuu3160
	Date: 2025/12/6 Sat PM 10:03:46 初回作成
===================================================================+*/
#pragma once

// ==============================
//	include
// ==============================
#include <string>
#include <vector>
#include "cpon_object.hpp"
// ==============================
//	定数定義
// ==============================
namespace
{
}

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
	cpon_object &operator[](_In_ std::string In_ObjectName);

	/// <summary>
	/// 指定された名前のオブジェクトを作成し、その参照を返します
	/// </summary>
	/// <param name="[In_ObjectName]">作成するオブジェクトの名前を表す文字列ビュー</param>
	/// <returns>作成された cpon_object への参照を返します</returns>
	cpon_object &CreateObject(_In_ const std::string_view In_ObjectName);

	/// <summary>
	/// 格納されているオブジェクトの数を取得します
	/// </summary>
	/// <returns>オブジェクトの個数をint型で返します。</returns>
	[[nodiscard]] int GetObjectCount() const noexcept { return static_cast<int>(m_Objects.size()); }

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
	/// <param name="[In_FilePath]">読み込むファイルのパスを示す文字列ビュー。</param>
	/// <returns>読み込みに成功した場合はtrue、失敗した場合はfalseを返します</returns>
	bool LoadFromFile(_In_ const std::string_view In_FilePath);

private:

	void CreateFileHeader();

	bool IsStringNpos(_In_ const size_t In_Pos) const noexcept { return In_Pos == std::string::npos; }

	void WriteObjectHeader(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object);
	void WriteDataBlocks(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object);
	void WriteDataBlockValue(_In_ std::ofstream &In_File, _In_ const cpon_block::DataValue &In_Value);
	void WriteDataBlockArray(_In_ std::ofstream &In_File, _In_ const cpon_block::Array &In_Array);

	bool ReadObject(_In_ std::ifstream &In_File, _In_ std::string_view In_Line, _In_ cpon_object &In_Object, _In_ std::string_view In_FilePath);
	std::string ReadObjectName(_In_ const std::string_view In_Line) const;
	int ReadObjectDataCount(_In_ const std::string_view In_Line);
	void ReadBlockInfo(_In_ const std::string_view In_Line, _Out_ int &Out_BlockNum, _Out_ std::string &Out_BlockHints);
	void ReadHintInfo(_In_ const std::string_view In_Hint, _Out_ std::string &Out_HintID, _Out_ std::string &Out_HintType);
	void ReadBlockValue(_In_ std::shared_ptr<cpon_block> In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType);
	void ReadBlockArray(_In_ std::shared_ptr<cpon_block> In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType);

	int CountElement(_In_ const std::string_view In_Data, _In_ char In_CountTarget) const noexcept;

	cpon_block::Array CreateArrayByType(_In_ const std::string_view In_Type);

	template<typename T>
	std::string ToStr(_In_ T In_Value)
	{
		std::string Str;
		Str.resize(std::numeric_limits<size_t>::digits10 + 2);
		auto res = std::to_chars(Str.data(), Str.data() + Str.size(), In_Value);
		Str.resize(res.ptr - Str.data());
		return Str;
	}

	template<typename T>
	T FromStr(_In_ const std::string_view In_Str)
	{
		T Value;
		std::from_chars(In_Str.data(), In_Str.data() + In_Str.size(), Value);
		return Value;
	}

	std::string m_FileHeader;
	std::vector<std::shared_ptr<cpon_object>> m_Objects;
};
