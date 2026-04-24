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
#include "cpon_binary_helper.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <random>

cpon_object &cpon::operator[](_In_ int In_Index)
{
	if(m_Objects.size() <= static_cast<size_t>(In_Index) || In_Index < 0)
	{
		throw std::out_of_range("指定されたインデックスのオブジェクトが存在しません: " + std::to_string(In_Index));
	}
	return *(m_Objects[In_Index]);
}

cpon_object &cpon::operator[](_In_ std::string_view In_ObjectName)
{
	auto itr = std::find_if(m_Objects.begin(), m_Objects.end(),
		[&In_ObjectName](const std::shared_ptr<cpon_object> &obj)
		{
			return obj->GetObjectName() == In_ObjectName;
		});
	if (itr != m_Objects.end())
		return **itr;

	std::string ErrorMsg = "指定された名前のオブジェクトが存在しません: ";
	ErrorMsg += In_ObjectName;
	throw std::out_of_range(ErrorMsg);
}

std::shared_ptr<cpon_object> cpon::GetObjectPtr(_In_ std::string_view In_ObjectName)
{
	auto itr = std::find_if(m_Objects.begin(), m_Objects.end(),
		[&In_ObjectName](const std::shared_ptr<cpon_object> &obj)
		{
			return obj->GetObjectName() == In_ObjectName;
		});
	if(itr != m_Objects.end())
		return *itr;
	return nullptr;
}

std::shared_ptr<cpon_object> cpon::CreateObject(_In_ const std::string_view In_ObjectName)
{
	auto newObject = std::make_shared<cpon_object>();
	newObject->SetObjectName(In_ObjectName);
	m_Objects.push_back(newObject);
	return newObject;
}

std::shared_ptr<cpon_object> cpon::TryCreateObject(_In_ std::string_view In_ObjectName)
{
	auto itr = std::find_if(m_Objects.begin(), m_Objects.end(),
		[&In_ObjectName](const std::shared_ptr<cpon_object> &obj)
		{
			return obj->GetObjectName() == In_ObjectName;
		});
	if(itr != m_Objects.end())
		return *itr;
	return CreateObject(In_ObjectName);
}

void cpon::AddObject(_In_ std::shared_ptr<cpon_object> In_Object) noexcept
{
	m_Objects.push_back(In_Object);
}

void cpon::ClearObjectsData() noexcept
{
	// 現在保持しているオブジェクトを削除
	for(auto &itr : m_Objects)
	{
		itr.reset();
	}
	m_Objects.clear();
	m_FileHeader.clear();
}

bool cpon::WriteToFile(_In_ const std::string_view In_FilePath)
{
	std::string PathStr = std::string(In_FilePath);

	if(IsStringNpos(PathStr.find_last_of('.')))
		PathStr += ".cpon";

	std::ofstream File(std::string(PathStr), std::ios::out | std::ios::trunc);
	if (!File.is_open())
	{
		std::cerr << "ファイルを開けませんでした : " << PathStr << std::endl;
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
	std::string PathStr = std::string(In_FilePath);

	if(IsStringNpos(PathStr.find_last_of('.')))
		PathStr += ".cpon";

	std::ifstream File(std::string(PathStr), std::ios::in);
	if (!File.is_open())
	{
		std::cerr << "ファイルを開けませんでした : " << PathStr << std::endl;
		return false;
	}

	// 現在保持しているオブジェクトを削除
	ClearObjectsData();

	std::string line;

	// ヘッダの取得
	std::getline(File, line);
	if (IsStringNpos(line.find("#ObjNum : ")))
	{
		std::cerr << "ヘッダ情報が読み取れませんでした。" << PathStr << std::endl;
		return false;
	}
	m_FileHeader = line;

	// オブジェクトの個数を取得
	int ObjCount;
	ObjCount = ReadObjectDataCount(m_FileHeader);

	for (int i = 0; i < ObjCount; ++i)
	{
		if(std::getline(File, line))
		{
			// オブジェクトヘッダの読み取り
			if(IsStringNpos(line.find("}:")))
			{
				std::cerr << "データ形式が不正です : " << PathStr << std::endl;
				return false;
			}

			std::string ObjName;
			ObjName = ReadObjectName(line);

			// オブジェクト作成
			auto Obj = CreateObject(ObjName);

			if(!ReadObject(File, line, Obj, PathStr))
				return false;
		}
		else
		{
			std::cerr << "データを読み取れませんでした : " << PathStr << std::endl;
			return false;
		}
	}

	File.close();
	return true;
}

std::vector<uint8_t> cpon::Serialize()
{
	// ペイロード (オブジェクトデータ本体) を生成
	std::vector<uint8_t> payload;
	payload.reserve(1024);
 
	cpon_binary::WriteU32(payload, static_cast<uint32_t>(m_Objects.size()));
	for(const auto &obj : m_Objects)
		cpon_binary_helper::SerializeObject(payload, obj);
 
	// RLE圧縮
	const uint32_t originalSize = static_cast<uint32_t>(payload.size());
	std::vector<uint8_t> compressed = cpon_binary::RleCompress(payload);
	const uint32_t compressedSize = static_cast<uint32_t>(compressed.size());
 
	// XORシードをランダム生成(0は「スクランブルなし」なので除外)
	std::mt19937 rng(std::random_device{}());
	uint32_t seed = 0;
	while(seed == 0)
		seed = rng();
 
	// XORスクランブル
	cpon_binary::XorScramble(compressed, seed);
 
	// ファイルヘッダ + ペイロードを結合
	std::vector<uint8_t> buf;
	buf.reserve(4 + 1 + 4 + 4 + 4 + compressedSize);
 
	buf.insert(buf.end(), std::begin(cpon_binary::MAGIC), std::end(cpon_binary::MAGIC));
	cpon_binary::WriteU8 (buf, cpon_binary::VERSION);
	cpon_binary::WriteU32(buf, seed);
	cpon_binary::WriteU32(buf, originalSize);
	cpon_binary::WriteU32(buf, compressedSize);
	buf.insert(buf.end(), compressed.begin(), compressed.end());
 
	return buf;
}

bool cpon::Deserialize(_In_ std::span<const uint8_t> In_Data)
{
	cpon_binary::Reader r(In_Data);

	// マジックナンバー確認
	uint8_t magic[4];
	for(int i = 0; i < 4; ++i)
	{
		if(!r.ReadU8(magic[i]))
			return false;
	}

	if(std::memcmp(magic, cpon_binary::MAGIC, 4) != 0)
	{
		std::cerr << "マジックナンバーが一致しません。" << std::endl;
		return false;
	}

	// バージョン確認
	uint8_t version;
	if(!r.ReadU8(version))
	{
		std::cerr << "バージョン情報の読み取りに失敗しました。" << std::endl;
		return false;
	}
	if(version != cpon_binary::VERSION)
	{
		std::cerr << "バイナリバージョンが異なります: 0x" << std::hex
			<< static_cast<int>(version) << " (期待: 0x"
			<< static_cast<int>(cpon_binary::VERSION) << ")" << std::dec << std::endl;
		return false;
	}

	// ヘッダ読み取り
	uint32_t seed, originalSize, compressedSize;
	if(!r.ReadU32(seed))
		return false;
	if(!r.ReadU32(originalSize))
		return false;
	if(!r.ReadU32(compressedSize))
		return false;

	// 圧縮データを取り出す
	// ヘッダサイズ: magic(4) + version(1) + seed(4) + originalSize(4) + compressedSize(4) = 17B
	constexpr size_t HeaderSize = 4 + 1 + 4 + 4 + 4;
	if(In_Data.size() < HeaderSize + compressedSize)
	{
		std::cerr << "ファイルサイズが不正です。" << std::endl;
		return false;
	}
	std::vector<uint8_t> compressed(
		In_Data.data() + HeaderSize,
		In_Data.data() + HeaderSize + compressedSize);

	// XORデスクランブル
	cpon_binary::XorDescramble(compressed, seed);

	// RLE展開
	std::vector<uint8_t> payload;
	if(!cpon_binary::RleDecompress(compressed, originalSize, payload))
		return false;

	// ペイロードをデシリアライズ
	ClearObjectsData();
	cpon_binary::Reader pr(payload);

	uint32_t objCount;
	if(!pr.ReadU32(objCount)) return false;

	for(uint32_t oi = 0; oi < objCount; ++oi)
	{
		std::string objName, blockHints;
		if(!pr.ReadShortString(objName))
			return false;
		if(!pr.ReadShortString(blockHints))
			return false;

		auto obj = CreateObject(objName);
		obj->SetHints(blockHints);

		uint32_t blockCount;
		if(!pr.ReadU32(blockCount)) return false;

		for(uint32_t bi = 0; bi < blockCount; ++bi)
		{
			auto block = obj->CreateDataBlock();

			uint32_t itemCount;
			if(!pr.ReadU32(itemCount)) return false;

			for(uint32_t di = 0; di < itemCount; ++di)
			{
				std::string key;
				uint8_t typeId;
				if(!pr.ReadShortString(key))
					return false;
				if(!pr.ReadU8(typeId))
					return false;
				if(!cpon_binary_helper::DeserializeValue(pr, block, key, typeId))
					return false;
			}
		}
	}

	return true;
}

bool cpon::WriteToBinaryFile(_In_ std::string_view In_FilePath)
{
	std::string path(In_FilePath);
	if(path.find_last_of('.') == std::string::npos)
		path += ".cponb";
 
	const auto buf = Serialize();
	std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
	if(!file.is_open())
	{
		std::cerr << "バイナリファイルを開けませんでした: " << path << std::endl;
		return false;
	}
	file.write(reinterpret_cast<const char *>(buf.data()), static_cast<std::streamsize>(buf.size()));
	return !file.fail();
}

bool cpon::LoadFromBinaryFile(_In_ std::string_view In_FilePath)
{
	std::string path(In_FilePath);
	if(path.find_last_of('.') == std::string::npos)
		path += ".cponb";

	std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
	if(!file.is_open())
	{
		std::cerr << "バイナリファイルを開けませんでした: " << path << std::endl;
		return false;
	}

	const std::streamsize fileSize = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> buf(static_cast<size_t>(fileSize));
	if(!file.read(reinterpret_cast<char *>(buf.data()), fileSize))
		return false;

	return Deserialize(std::span<const uint8_t>(buf.data(), buf.size()));
}

// ---------------------------------------------
// cponクラスの非公開関数の実装
// ---------------------------------------------

void cpon::CreateFileHeader()
{
	m_FileHeader.clear();
	std::string Num;
	Num = ToStr(m_Objects.size());
	m_FileHeader = "#ObjNum : " + Num;
}

void cpon::WriteObjectHeader(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object)
{
	// ブロック内のオブジェクトはキーがオブジェクト名として描き込まれるため、名前の出力をスキップ
	if(In_Object->m_NestedLevel == 0)
		In_File << In_Object->GetObjectName();

	In_File << "[" << In_Object->GetDataCount() << "]" << "{" << In_Object->GetHints() << "}:\n";
}

void cpon::WriteDataBlocks(_In_ std::ofstream &In_File, _In_ std::shared_ptr<cpon_object> In_Object)
{
	auto Blocks = In_Object->GetDataBlocks();
	std::string Hints = In_Object->GetBlockHints();
	int BlockDataCount = CountElement(Hints, ':');

	for (int i = 0; i < Blocks.size();++i)
	{
		std::string Hint = Hints;
		for (int j = 0; j < BlockDataCount; ++j)
		{
			std::string Key = Hint;
			size_t pos = Key.find(",");
			if(!IsStringNpos(pos))
				Key = Key.erase(Key.find(","));
			Key = Key.erase(Key.find(":"));

			// ヒント情報を進める
			pos = Hint.find(",");
			if(!IsStringNpos(pos))
				Hint = Hint.erase(0, Hint.find(",") + 1);

			auto data = Blocks[i]->m_BlockData.find(Key);
			if(data == Blocks[i]->m_BlockData.end())
				continue;

			for(int indent = 0; indent < Blocks[i]->m_NestedLevel; ++indent)
				In_File << "  ";

			In_File << data->first << ":";
			if (std::holds_alternative<cpon_block::DataValue>(data->second))
			{
				const auto &value = std::get<cpon_block::DataValue>(data->second);
				WriteDataBlockValue(In_File, value);
			}
			else if (std::holds_alternative<cpon_block::Array>(data->second))
			{
				const auto &array = std::get<cpon_block::Array>(data->second);
				WriteDataBlockArray(In_File, array);
			}
			else if(std::holds_alternative<cpon_block::Object>(data->second))
			{
				WriteObjectHeader(In_File, std::get<cpon_block::Object>(data->second));
				WriteDataBlocks(In_File, std::get<cpon_block::Object>(data->second));
			}
		}
		if(i < Blocks.size() - 1)
			In_File << "\n";
	}
}

void cpon::WriteDataBlockValue(_In_ std::ofstream &In_File, _In_ const cpon_block::DataValue &In_Value)
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
	In_File << "\n";
}

void cpon::WriteDataBlockArray(_In_ std::ofstream &In_File, _In_ const cpon_block::Array &In_Array)
{
	size_t ArraySize = std::visit([](auto &&arg) -> size_t {
		return arg.size();
		}, In_Array);

	In_File << "[" << ArraySize << "]";
	for (size_t i = 0; i < ArraySize; ++i)
	{
		auto opt = std::visit(cpon_block::GetElementAsStringVisitor{i}, In_Array);

		if (!opt.has_value())
			continue;
		// 要素の書き込み
		In_File << opt.value();
		// 最後の要素でなければカンマを追加
		if (i < ArraySize - 1)
			In_File << ", ";
	}
	In_File << "\n";
}

bool cpon::ReadObject(_In_ std::ifstream &In_File, _In_ std::string_view In_Line, _In_ std::shared_ptr<cpon_object> In_Object, _In_ std::string_view In_FilePath)
{
	std::string line = std::string(In_Line);

	std::string BlockHints;
	int BlockNum = 0;
	int BlockDataNum = 0;

	ReadBlockInfo(line, BlockNum, BlockHints);
	BlockDataNum = CountElement(BlockHints, ':');

	// オブジェクトデータの読み取り
	for(int BlockCount = 0; BlockCount < BlockNum; ++BlockCount)
	{
		// ブロックの作成
		auto block = In_Object->CreateDataBlock();
		std::string BlockHint = BlockHints;

		// ブロックデータの読み取り
		for(int DataCount = 0; DataCount < BlockDataNum; ++DataCount)
		{
			std::string HintID;
			std::string HintType;
			ReadHintInfo(BlockHint, HintID, HintType);

			if(std::getline(In_File, line))
			{
				auto IDPos = line.find(HintID);

				// 空行の場合はスキップ
				if(line.empty())
					continue;

				// データが存在しないので、ヒントを更新して再検索
				if(IsStringNpos(IDPos))
				{
					++DataCount;
					for(; DataCount < BlockDataNum; ++DataCount)
					{
						BlockHint = BlockHint.erase(0, BlockHint.find(",") + 1);
						ReadHintInfo(BlockHint, HintID, HintType);
						IDPos = line.find(HintID);
						if(!IsStringNpos(IDPos))
							break;
					}
					// 最後まで見つからなかった場合、異常終了
					if(IsStringNpos(IDPos))
					{
						std::cerr << "データを読み取れませんでした : " << In_FilePath << std::endl;
						return false;
					}
				}

				line = line.erase(0, IDPos + HintID.size() + 1);

				if(!IsStringNpos(HintType.find("array<")))
				{
					ReadBlockArray(block, line, HintID, HintType);
				}
				else if(!IsStringNpos(HintType.find("object")))
				{
					auto Obj = block->CreateObject(HintID);
					// オブジェクト型の読み取り
					if(!ReadObject(In_File,line, Obj, In_FilePath))
						return false;
				}
				else
				{
					ReadBlockValue(block, line, HintID, HintType);
				}
			}
			else
			{
				std::cerr << "データを読み取れませんでした : " << In_FilePath << std::endl;
				return false;
			}
			BlockHint = BlockHint.erase(0, BlockHint.find(",") + 1);
		}
		if(In_Object->m_NestedLevel == 0)
			std::getline(In_File, line); // 空白行を飛ばすために読み取る
	}
	return true;
}

std::string cpon::ReadObjectName(_In_ const std::string_view In_Line) const
{
	std::string Line = std::string(In_Line);
	std::string ObjName = Line;
	return ObjName.erase(ObjName.find("["));
}

int cpon::ReadObjectDataCount(_In_ const std::string_view In_Line)
{
	int ObjCount;
	std::string NumStr = std::string(In_Line);
	NumStr = NumStr.erase(0, NumStr.find(":") + 2);
	ObjCount = FromStr<int>(NumStr);
	return ObjCount;
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

void cpon::ReadBlockValue(_In_ std::shared_ptr<cpon_block> In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType)
{
	if (In_HintType == "string")
		In_Block->SetValue(In_HintID, In_Line.data());
	else if (In_HintType == "int")
		In_Block->SetValue(In_HintID, FromStr<int>(In_Line));
	else if (In_HintType == "uint")
		In_Block->SetValue(In_HintID, FromStr<unsigned int>(In_Line));
	else if (In_HintType == "float")
		In_Block->SetValue(In_HintID, FromStr<float>(In_Line));
	else if (In_HintType == "double")
		In_Block->SetValue(In_HintID, FromStr<double>(In_Line));
	else if (In_HintType == "bool")
	{
		if (!IsStringNpos(In_Line.find("true")))
			In_Block->SetValue(In_HintID, true);
		else
			In_Block->SetValue(In_HintID, false);
	}
}

void cpon::ReadBlockArray(_In_ std::shared_ptr<cpon_block> In_Block, _In_ const std::string_view In_Line, _In_ const std::string_view In_HintID, _In_ const std::string_view In_HintType)
{
	std::string Line = std::string(In_Line);
	std::string ArrayType = std::string(In_HintType);
	std::string ArrayCountStr = Line.erase(0, Line.find("[") + 1);
	std::string ArrayValues = Line.erase(0, Line.find("]") + 1);
	int ArrayCount;
	ArrayType = ArrayType.erase(0, ArrayType.find("<"));
	ArrayCountStr = ArrayCountStr.erase(ArrayCountStr.find("]"));
	ArrayCount = FromStr<int>(ArrayCountStr);

	cpon_block::Array array = CreateArrayByType(ArrayType);
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

	In_Block->SetArray(In_HintID, array);
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

cpon_block::Array cpon::CreateArrayByType(_In_ const std::string_view In_Type)
{
	if(In_Type == "<string>")
		return cpon_block::Array(std::vector<std::string>{});
	 else if(In_Type == "<int>")
		return cpon_block::Array(std::vector<int>{});
	 else if(In_Type == "<uint>")
		return cpon_block::Array(std::vector<unsigned int>{});
	 else if(In_Type == "<float>")
		return cpon_block::Array(std::vector<float>{});
	 else if(In_Type == "<double>")
		return cpon_block::Array(std::vector<double>{});
	 else if(In_Type == "<bool>")
		return cpon_block::Array(std::vector<bool>{});
	return cpon_block::Array(std::vector<std::string>{});
}
