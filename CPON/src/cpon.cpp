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
#include "cpon_text.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <random>

namespace
{
	// True when the file name part of In_Path (after the last '/' or '\') contains a '.'
	// (a '.' in a directory name, as in "./save" or "dir.v2/save", is not an extension)
	bool HasExtension(const std::string &In_Path)
	{
		const size_t Sep = In_Path.find_last_of("/\\");
		const size_t Dot = In_Path.find_last_of('.');
		return Dot != std::string::npos && (Sep == std::string::npos || Dot > Sep);
	}

	// Shortest text that reads back as the same value (operator<< keeps only 6 significant digits)
	template<typename T>
	std::string FloatToStr(T In_Value)
	{
		char Buf[32];
		const auto Res = std::to_chars(Buf, Buf + sizeof(Buf), In_Value);
		return std::string(Buf, Res.ptr);
	}
}

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

	if(!HasExtension(PathStr))
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

	if(!HasExtension(PathStr))
		PathStr += ".cpon";

	std::ifstream File(std::string(PathStr), std::ios::in);
	if (!File.is_open())
	{
		std::cerr << "ファイルを開けませんでした : " << PathStr << std::endl;
		return false;
	}

	// Read into a temporary: the current data is replaced only when the whole file was read
	std::vector<std::shared_ptr<cpon_object>> Objects;
	bool Loaded = false;
	try
	{
		Loaded = cpon_text_helper::Read(File, Objects);
	}
	catch(...)
	{
		Loaded = false;
	}
	if(!Loaded)
	{
		std::cerr << "データ形式が不正です : " << PathStr << std::endl;
		return false;
	}

	m_Objects = std::move(Objects);
	m_FileHeader.clear();
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
	if(In_Data.size() - HeaderSize < compressedSize) // the header was read above, so size() >= HeaderSize; no overflow on Win32
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
	if(!HasExtension(path))
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
	if(!HasExtension(path))
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

	for (size_t i = 0; i < Blocks.size();++i)
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
		In_File << FloatToStr(std::get<float>(In_Value));
	else if (std::holds_alternative<double>(In_Value))
		In_File << FloatToStr(std::get<double>(In_Value));
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
