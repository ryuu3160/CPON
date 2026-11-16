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
#include <sstream>

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
}

bool cpon::WriteToFile(_In_ const std::string_view In_FilePath)
{
	std::string PathStr = std::string(In_FilePath);

	if(!HasExtension(PathStr))
		PathStr += ".cpon";

	// Build the whole text first: nothing is written to the file when the data cannot be written
	std::ostringstream Text;
	bool Written = false;
	try
	{
		Written = cpon_text_helper::Write(Text, m_Objects);
	}
	catch(...)
	{
		Written = false;
	}
	if(!Written)
	{
		std::cerr << "データを書き込めませんでした : " << PathStr << std::endl;
		return false;
	}

	std::ofstream File(std::string(PathStr), std::ios::out | std::ios::trunc);
	if (!File.is_open())
	{
		std::cerr << "ファイルを開けませんでした : " << PathStr << std::endl;
		return false;
	}

	File << Text.str();
	File.close();
	return !File.fail();
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
