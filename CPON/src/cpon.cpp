/*+===================================================================
	File: cpon.cpp
	Summary: cpon(C++ Object Notation)のパーサークラス
			 cponは、TONLやTOONを参考にした、C++向けのデータ記述言語です。
	Author: ryuu3160
	Date: 2025/12/6 Sat PM 10:03:52 初回作成
===================================================================+*/

// ==============================
//	include
// ==============================
#include "cpon.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>

// ==============================
//	定数定義
// ==============================
namespace
{
}

cpon_object &cpon::operator[](_In_ int In_Index)
{
	if(m_Objects.size() <= static_cast<size_t>(In_Index) || In_Index < 0)
	{
		throw std::out_of_range("指定されたインデックスのオブジェクトが存在しません: " + std::to_string(In_Index));
	}
	return *(m_Objects[In_Index]);
}

cpon_object &cpon::operator[](_In_ std::string In_ObjectName)
{
	auto itr = std::find_if(m_Objects.begin(), m_Objects.end(),
		[&In_ObjectName](const std::shared_ptr<cpon_object> &obj)
		{
			return obj->GetObjectName() == In_ObjectName;
		});
	if (itr != m_Objects.end())
		return **itr;

	throw std::out_of_range("指定された名前のオブジェクトが存在しません: " + In_ObjectName);
}

cpon_object &cpon::CreateObject(_In_ const std::string_view In_ObjectName)
{
	auto newObject = std::make_shared<cpon_object>();
	newObject->SetObjectName(In_ObjectName);
	m_Objects.push_back(newObject);
	return *newObject;
}

bool cpon::WriteToFile(_In_ const std::string_view In_FilePath)
{
	if (IsStringNpos(In_FilePath.find(".cpon")))
	{
		std::cerr << "ファイルの拡張子が不正です : " << In_FilePath << std::endl;
		return false;
	}

	std::ofstream File(std::string(In_FilePath), std::ios::out | std::ios::trunc);
	if (!File.is_open())
	{
		std::cerr << "ファイルを開けませんでした : " << In_FilePath << std::endl;
		return false;
	}

	CreateFileHeader();

	File << m_FileHeader << '\n';

	for (const auto &obj : m_Objects)
	{
		// オブジェクトヘッダの書き込み
		WriteObjectHeader(File, obj);

		// データブロックの書き込み
		WriteDataBlocks(File, obj);

		File << '\n';
	}
	File.close();
	return true;
}

bool cpon::LoadFromFile(_In_ const std::string_view In_FilePath)
{
	if (IsStringNpos(In_FilePath.find(".cpon")))
	{
		std::cerr << "ファイルの拡張子が不正です : " << In_FilePath << std::endl;
		return false;
	}

	std::ifstream File(std::string(In_FilePath), std::ios::in);
	if (!File.is_open())
	{
		std::cerr << "ファイルを開けませんでした : " << In_FilePath << std::endl;
		return false;
	}

	// 現在保持しているオブジェクトを削除
	for (auto &itr : m_Objects)
	{
		itr.reset();
	}
	m_Objects.clear();
	m_FileHeader.clear();

	std::string line;

	// ヘッダの取得
	std::getline(File, line);
	if (IsStringNpos(line.find("#ObjNum : ")))
	{
		std::cerr << "ヘッダ情報が読み取れませんでした。" << In_FilePath << std::endl;
		return false;
	}
	m_FileHeader = line;

	// オブジェクトの個数を取得
	int ObjCount;
	std::string Num;
	Num = m_FileHeader.erase(0,m_FileHeader.find(":") + 2);
	ObjCount = FromStr<int>(Num);

	for (int i = 0; i < ObjCount; ++i)
	{
		if (std::getline(File, line))
		{
			// オブジェクトヘッダの読み取り
			if (IsStringNpos(line.find("}:")))
			{
				std::cerr << "データ形式が不正です : " << In_FilePath << std::endl;
				return false;
			}

			std::string ObjName;
			std::string BlockHints;
			int BlockNum = 0;
			int BlockDataNum = 0;
			ObjName = ReadObjectName(line);
			ReadBlockInfo(line, BlockNum, BlockHints);
			BlockDataNum = CountElement(BlockHints,':');

			// オブジェクト作成
			auto &Obj = CreateObject(ObjName);

			// オブジェクトの読み取り
			for (int BlockCount = 0; BlockCount < BlockNum; ++BlockCount)
			{
				// ブロックの作成
				auto &block = Obj.CreateDataBlock();
				std::string BlockHint = BlockHints;

				// ブロックデータの読み取り
				for (int DataCount = 0; DataCount < BlockDataNum; ++DataCount)
				{
					std::string HintID;
					std::string HintType;
					ReadHintInfo(BlockHint, HintID, HintType);

					if (std::getline(File, line))
					{
						auto IDPos = line.find(HintID);

						if (IsStringNpos(IDPos))
						{
							std::cerr << "データ形式が不正です : " << In_FilePath << std::endl;
							return false;
						}

						line = line.erase(0, IDPos + HintID.size() + 1);

						if (IsStringNpos(HintType.find("array<")))
						{
							ReadBlockValue(block, line, HintID, HintType);
						}
						else
						{
							ReadBlockArray(block, line, HintID, HintType);
						}
					}
					else
					{
						std::cerr << "データを読み取れませんでした : " << In_FilePath << std::endl;
						return false;
					}
					BlockHint = BlockHint.erase(0, BlockHint.find(",") + 1);
				}
				std::getline(File, line); // 空白行を飛ばすために読み取る
			}
		}
		else
		{
			std::cerr << "データを読み取れませんでした : " << In_FilePath << std::endl;
			return false;
		}
		std::getline(File, line); // 空白行を飛ばすために読み取る
	}

	File.close();
	return true;
}

void cpon::CreateFileHeader()
{
	m_FileHeader.clear();
	std::string Num;
	Num = ToStr(m_Objects.size());
	m_FileHeader = "#ObjNum : " + Num;
}

void cpon::WriteObjectHeader(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object)
{
	In_File << In_Object->GetObjectName() << "[" << In_Object->GetDataCount() << "]" << "{" << In_Object->GetHints() << "}:\n";
}

void cpon::WriteDataBlocks(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object)
{
	for (const auto &block : In_Object->GetDataBlocks())
	{
		for (const auto &data : block.m_BlockData)
		{
			In_File << "  " << data.first << ":";
			if (std::holds_alternative<cpon_object::cpon_block::DataValue>(data.second))
			{
				const auto &value = std::get<cpon_object::cpon_block::DataValue>(data.second);
				WriteDataBlockValue(In_File, value);
			}
			else if (std::holds_alternative<cpon_object::cpon_block::Array>(data.second))
			{
				const auto &array = std::get<cpon_object::cpon_block::Array>(data.second);
				WriteDataBlockArray(In_File, array);
			}
			In_File << "\n";
		}

		In_File << "\n";
	}
}

void cpon::WriteDataBlockValue(_In_ std::ofstream &In_File, _In_ const cpon_object::cpon_block::DataValue &In_Value)
{
	if (std::holds_alternative<std::string>(In_Value))
		In_File << std::get<std::string>(In_Value);
	else if (std::holds_alternative<int>(In_Value))
		In_File << std::get<int>(In_Value);
	else if (std::holds_alternative<unsigned int>(In_Value))
		In_File << std::get<unsigned int>(In_Value);
	else if (std::holds_alternative<float>(In_Value))
		In_File << std::get<float>(In_Value);
	else if (std::holds_alternative<double>(In_Value))
		In_File << std::get<double>(In_Value);
	else if (std::holds_alternative<bool>(In_Value))
		In_File << (std::get<bool>(In_Value) ? "true" : "false");
}

void cpon::WriteDataBlockArray(_In_ std::ofstream &In_File, _In_ const cpon_object::cpon_block::Array &In_Array)
{
	size_t ArraySize = std::visit([](auto &&arg) -> size_t {
		return arg.size();
		}, In_Array);

	In_File << "[" << ArraySize << "]";
	for (size_t i = 0; i < ArraySize; ++i)
	{
		auto opt = std::visit(cpon_object::cpon_block::GetElementAsStringVisitor{i}, In_Array);

		if (!opt.has_value())
			continue;
		// 要素の書き込み
		In_File << opt.value();
		// 最後の要素でなければカンマを追加
		if (i < ArraySize - 1)
			In_File << ", ";
	}
}

std::string cpon::ReadObjectName(_In_ const std::string_view In_Line) const
{
	std::string Line = std::string(In_Line);
	std::string ObjName = Line;
	return ObjName.erase(ObjName.find("["));
}

void cpon::ReadBlockInfo(_In_ const std::string_view In_Line, _Out_ int &Out_BlockNum, _Out_ std::string &Out_BlockHints)
{
	std::string Line = std::string(In_Line);
	std::string BlockNumStr;
	std::string BlockHints;
	BlockNumStr = Line.erase(0, Line.find("[") + 1);
	BlockNumStr = BlockNumStr.erase(BlockNumStr.find("]"));
	Out_BlockNum = FromStr<int>(BlockNumStr);
	BlockHints = Line.erase(0, Line.find("{") + 1);
	Out_BlockHints = BlockHints.erase(Line.find("}"));
}

void cpon::ReadHintInfo(_In_ const std::string_view In_Hint, _Out_ std::string &Out_HintID, _Out_ std::string &Out_HintType)
{
	std::string Hint = std::string(In_Hint);
	std::string HintID;
	std::string HintType;
	auto CommaPos = Hint.find(",");
	if (!IsStringNpos(CommaPos))
		Hint = Hint.erase(CommaPos);
	HintID = HintType = Hint;
	Out_HintID = HintID.erase(HintID.find(":"));
	Out_HintType = HintType.erase(0, HintType.find(":") + 1);
}

void cpon::ReadBlockValue(_In_ cpon_object::cpon_block &In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType)
{
	if (In_HintType == "string")
		In_Block.SetValue(In_HintID, In_Line.data());
	else if (In_HintType == "int")
		In_Block.SetValue(In_HintID, FromStr<int>(In_Line));
	else if (In_HintType == "uint")
		In_Block.SetValue(In_HintID, FromStr<unsigned int>(In_Line));
	else if (In_HintType == "float")
		In_Block.SetValue(In_HintID, FromStr<float>(In_Line));
	else if (In_HintType == "double")
		In_Block.SetValue(In_HintID, FromStr<double>(In_Line));
	else if (In_HintType == "bool")
	{
		if (!IsStringNpos(In_Line.find("true")))
			In_Block.SetValue(In_HintID, true);
		else
			In_Block.SetValue(In_HintID, false);
	}
}

void cpon::ReadBlockArray(_In_ cpon_object::cpon_block &In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType)
{
	std::string Line = std::string(In_Line);
	std::string ArrayType = std::string(In_HintType);
	std::string ArrayCountStr = Line.erase(0, Line.find("[") + 1);
	std::string ArrayValues = Line.erase(0, Line.find("]") + 1);
	int ArrayCount;
	ArrayType = ArrayType.erase(0, ArrayType.find("<"));
	ArrayCountStr = ArrayCountStr.erase(ArrayCountStr.find("]"));
	ArrayCount = FromStr<int>(ArrayCountStr);

	cpon_object::cpon_block::Array array;
	for (int i = 0; i < ArrayCount; ++i)
	{
		std::string ArrayValue = ArrayValues;
		auto CommaPos = ArrayValue.find(",");

		if (!IsStringNpos(CommaPos))
			ArrayValue = ArrayValue.erase(CommaPos);

		if (ArrayValue[0] == ' ')
			ArrayValue = ArrayValue.erase(0, 1);

		if (ArrayType == "<string>")
			std::get<std::vector<std::string>>(array).push_back(ArrayValue);
		else if (ArrayType == "<int>")
			std::get<std::vector<int>>(array).push_back(FromStr<int>(ArrayValue));
		else if (ArrayType == "<uint>")
			std::get<std::vector<unsigned int>>(array).push_back(FromStr<unsigned int>(ArrayValue));
		else if (ArrayType == "<float>")
			std::get<std::vector<float>>(array).push_back(FromStr<float>(ArrayValue));
		else if (ArrayType == "<double>")
			std::get<std::vector<double>>(array).push_back(FromStr<double>(ArrayValue));
		else if (ArrayType == "<bool>")
		{
			bool boolian = false;
			if (!IsStringNpos(ArrayValue.find("true")))
				boolian = true;
			else
				boolian = false;
			std::get<std::vector<bool>>(array).push_back(boolian);
		}

		ArrayValues = ArrayValues.erase(0, CommaPos + 1);
	}

	In_Block.CreateArray(In_HintID, array);
}

int cpon::CountElement(_In_ const std::string_view In_Data, _In_ char In_CountTarget) const noexcept
{
	if (In_Data.empty())
		return 0;
	int Count = 0;
	for (const auto &ch : In_Data)
	{
		if (ch == In_CountTarget)
			++Count;
	}
	return Count;
}
