/*+===================================================================
	File: cpon.hpp
	Summary: cpon(C++ Object Notation)のパーサークラス
			 cponは、TONLやTOONを参考にした、C++向けのデータ記述言語です。
	Author: AT13C192 01 青木雄一郎
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

	cpon_object &operator[](_In_ int In_Index);

	cpon_object &CreateObject(_In_ const std::string_view In_ObjectName);
	[[nodiscard]] int GetObjectCount() const noexcept { return static_cast<int>(m_Objects.size()); }

	bool WriteToFile(_In_ const std::string_view In_FilePath);

	bool LoadFromFile(_In_ const std::string_view In_FilePath);

private:

	void CreateFileHeader();

	bool IsStringNpos(_In_ const size_t In_Pos) const noexcept { return In_Pos == std::string::npos; }

	void WriteObjectHeader(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object);
	void WriteDataBlocks(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object);
	void WriteDataBlockValue(_In_ std::ofstream &In_File, _In_ const cpon_object::cpon_block::DataValue &In_Value);
	void WriteDataBlockArray(_In_ std::ofstream &In_File, _In_ const cpon_object::cpon_block::Array &In_Array);

	std::string ReadObjectName(_In_ const std::string_view In_Line) const;
	void ReadBlockInfo(_In_ const std::string_view In_Line, _Out_ int &Out_BlockNum, _Out_ std::string &Out_BlockHints);
	void ReadHintInfo(_In_ const std::string_view In_Hint, _Out_ std::string &Out_HintID, _Out_ std::string &Out_HintType);
	void ReadBlockValue(_In_ cpon_object::cpon_block &In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType);
	void ReadBlockArray(_In_ cpon_object::cpon_block &In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType);

	int CountElement(_In_ const std::string_view In_Data, _In_ char In_CountTarget) const noexcept;

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
