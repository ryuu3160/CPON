/*+===================================================================
	File: cpon_binary.cpp
	Summary: CPONのバイナリ変換機能の実装
	Author: 青木雄一郎
	Date: 2026/04/23 Thu AM 10:16:20 初回作成
===================================================================+*/

// ==============================
//	include
// ==============================
#include "cpon_binary_helper.hpp"
#include "cpon_object.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <bit>

namespace cpon_binary
{
	// ----------------------------------------
	// リトルエンディアン書き込みヘルパー
	// ----------------------------------------

	void WriteU8(std::vector<uint8_t> &buf, uint8_t v)
	{
		buf.push_back(v);
	}

	void WriteU16(std::vector<uint8_t> &buf, uint16_t v)
	{
		buf.push_back(static_cast<uint8_t>(v & 0xFF));
		buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
	}

	void WriteU32(std::vector<uint8_t> &buf, uint32_t v)
	{
		buf.push_back(static_cast<uint8_t>(v & 0xFF));
		buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
		buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
		buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
	}

	void WriteI32(std::vector<uint8_t> &buf, int32_t v)
	{
		WriteU32(buf, static_cast<uint32_t>(v));
	}

	void WriteF32(std::vector<uint8_t> &buf, float v)
	{
		uint32_t bits;
		std::memcpy(&bits, &v, sizeof(bits));
		WriteU32(buf, bits);
	}

	void WriteF64(std::vector<uint8_t> &buf, double v)
	{
		uint64_t bits;
		std::memcpy(&bits, &v, sizeof(bits));
		buf.push_back(static_cast<uint8_t>(bits & 0xFF));
		buf.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
		buf.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
		buf.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
		buf.push_back(static_cast<uint8_t>((bits >> 32) & 0xFF));
		buf.push_back(static_cast<uint8_t>((bits >> 40) & 0xFF));
		buf.push_back(static_cast<uint8_t>((bits >> 48) & 0xFF));
		buf.push_back(static_cast<uint8_t>((bits >> 56) & 0xFF));
	}

	// 文字列: [長さ:4B][UTF-8データ]
	void WriteString(std::vector<uint8_t> &buf, const std::string &s)
	{
		WriteU32(buf, static_cast<uint32_t>(s.size()));
		buf.insert(buf.end(), s.begin(), s.end());
	}

	// 短い文字列 (キー/名前等): [長さ:2B][UTF-8データ]
	void WriteShortString(std::vector<uint8_t> &buf, const std::string &s)
	{
		WriteU16(buf, static_cast<uint16_t>(s.size()));
		buf.insert(buf.end(), s.begin(), s.end());
	}

	// ----------------------------------------
	// シリアライズ: DataValue
	// ----------------------------------------
	void SerializeDataValue(std::vector<uint8_t> &buf, const cpon_block::DataValue &val)
	{
		if(std::holds_alternative<std::string>(val))
		{
			WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::String));
			WriteString(buf, std::get<std::string>(val));
		}
		else if(std::holds_alternative<int>(val))
		{
			WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::Int));
			WriteI32(buf, std::get<int>(val));
		}
		else if(std::holds_alternative<unsigned int>(val))
		{
			WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::UInt));
			WriteU32(buf, std::get<unsigned int>(val));
		}
		else if(std::holds_alternative<float>(val))
		{
			WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::Float));
			WriteF32(buf, std::get<float>(val));
		}
		else if(std::holds_alternative<double>(val))
		{
			WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::Double));
			WriteF64(buf, std::get<double>(val));
		}
		else if(std::holds_alternative<bool>(val))
		{
			WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::Bool));
			WriteU8(buf, std::get<bool>(val) ? 1 : 0);
		}
	}

	// ----------------------------------------
	// シリアライズ: Array
	// ----------------------------------------
	void SerializeArray(std::vector<uint8_t> &buf, const cpon_block::Array &arr)
	{
		std::visit([&buf](const auto &vec)
			{
				using Vec = std::decay_t<decltype(vec)>;
				using Elem = typename Vec::value_type;

				if constexpr(std::is_same_v<Elem, std::string>)
				{
					WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::ArrayString));
					WriteU32(buf, static_cast<uint32_t>(vec.size()));
					for(const auto &s : vec) WriteString(buf, s);
				}
				else if constexpr(std::is_same_v<Elem, int>)
				{
					WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::ArrayInt));
					WriteU32(buf, static_cast<uint32_t>(vec.size()));
					for(const auto &v : vec) WriteI32(buf, v);
				}
				else if constexpr(std::is_same_v<Elem, unsigned int>)
				{
					WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::ArrayUInt));
					WriteU32(buf, static_cast<uint32_t>(vec.size()));
					for(const auto &v : vec) WriteU32(buf, v);
				}
				else if constexpr(std::is_same_v<Elem, float>)
				{
					WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::ArrayFloat));
					WriteU32(buf, static_cast<uint32_t>(vec.size()));
					for(const auto &v : vec) WriteF32(buf, v);
				}
				else if constexpr(std::is_same_v<Elem, double>)
				{
					WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::ArrayDouble));
					WriteU32(buf, static_cast<uint32_t>(vec.size()));
					for(const auto &v : vec) WriteF64(buf, v);
				}
				else if constexpr(std::is_same_v<Elem, bool>)
				{
					WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::ArrayBool));
					WriteU32(buf, static_cast<uint32_t>(vec.size()));
					for(bool v : vec) WriteU8(buf, v ? 1 : 0);
				}
			}, arr);
	}

	// ----------------------------------------
	// シリアライズ: DataItem (DataValue / Array / Object)
	// ----------------------------------------
	void SerializeDataItem(std::vector<uint8_t> &buf, const cpon_block::DataItem &item)
	{
		if(std::holds_alternative<cpon_block::DataValue>(item))
		{
			SerializeDataValue(buf, std::get<cpon_block::DataValue>(item));
		}
		else if(std::holds_alternative<cpon_block::Array>(item))
		{
			SerializeArray(buf, std::get<cpon_block::Array>(item));
		}
		else if(std::holds_alternative<cpon_block::Object>(item))
		{
			WriteU8(buf, static_cast<uint8_t>(cpon_binary::TypeID::Object));
			cpon_binary_helper::SerializeObject(buf, std::get<cpon_block::Object>(item));
		}
	}

	// ----------------------------------------
	// リトルエンディアン読み込みヘルパー
	// ----------------------------------------

	bool Reader::ReadU8(uint8_t &out)
	{
		if(!HasBytes(1)) return false;
		out = data[pos++];
		return true;
	}

	bool Reader::ReadU16(uint16_t &out)
	{
		if(!HasBytes(2)) return false;
		out = static_cast<uint16_t>(data[pos])
			| (static_cast<uint16_t>(data[pos + 1]) << 8);
		pos += 2;
		return true;
	}

	bool Reader::ReadU32(uint32_t &out)
	{
		if(!HasBytes(4)) return false;
		out = static_cast<uint32_t>(data[pos])
			| (static_cast<uint32_t>(data[pos + 1]) << 8)
			| (static_cast<uint32_t>(data[pos + 2]) << 16)
			| (static_cast<uint32_t>(data[pos + 3]) << 24);
		pos += 4;
		return true;
	}

	bool Reader::ReadI32(int32_t &out)
	{
		uint32_t u;
		if(!ReadU32(u)) return false;
		out = static_cast<int32_t>(u);
		return true;
	}

	bool Reader::ReadF32(float &out)
	{
		uint32_t bits;
		if(!ReadU32(bits)) return false;
		std::memcpy(&out, &bits, sizeof(out));
		return true;
	}

	bool Reader::ReadF64(double &out)
	{
		if(!HasBytes(8)) return false;
		uint64_t bits =
			static_cast<uint64_t>(data[pos])
			| (static_cast<uint64_t>(data[pos + 1]) << 8)
			| (static_cast<uint64_t>(data[pos + 2]) << 16)
			| (static_cast<uint64_t>(data[pos + 3]) << 24)
			| (static_cast<uint64_t>(data[pos + 4]) << 32)
			| (static_cast<uint64_t>(data[pos + 5]) << 40)
			| (static_cast<uint64_t>(data[pos + 6]) << 48)
			| (static_cast<uint64_t>(data[pos + 7]) << 56);
		pos += 8;
		std::memcpy(&out, &bits, sizeof(out));
		return true;
	}

	bool Reader::ReadString(std::string &out)
	{
		uint32_t len;
		if(!ReadU32(len)) return false;
		if(!HasBytes(len)) return false;
		out.assign(reinterpret_cast<const char *>(&data[pos]), len);
		pos += len;
		return true;
	}

	bool Reader::ReadShortString(std::string &out)
	{
		uint16_t len;
		if(!ReadU16(len)) return false;
		if(!HasBytes(len)) return false;
		out.assign(reinterpret_cast<const char *>(&data[pos]), len);
		pos += len;
		return true;
	}

}

// ----------------------------------------
// cpon_binary_helperの実装
// ----------------------------------------
void cpon_binary_helper::SerializeObject(std::vector<uint8_t> &buf, const std::shared_ptr<cpon_object> &obj)
{
	// オブジェクト名
	cpon_binary::WriteShortString(buf, obj->GetObjectName());
	// ブロックヒント
	cpon_binary::WriteShortString(buf, obj->GetBlockHints());

	const auto &blocks = const_cast<cpon_object *>(obj.get())->GetDataBlocks();
	cpon_binary::WriteU32(buf, static_cast<uint32_t>(blocks.size()));

	for(const auto &block : blocks)
	{
		// データ項目数を後から埋めるためにプレースホルダを置く
		const size_t countPos = buf.size();
		cpon_binary::WriteU32(buf, 0); // placeholder

		uint32_t itemCount = 0;

		// ヒント文字列を使ってキー順序を復元する
		const std::string &hints = obj->GetBlockHints();
		std::string hintWalk = hints;

		// ヒントが空の場合はm_BlockDataを直接列挙する (フォールバック)
		if(hints.empty())
		{
			// ヒント無し: キー順不定だがすべて書き出す
			// (実際にはヒントが必ず存在するが安全策)
			for(const auto &[key, val] : block->m_BlockData)
			{
				cpon_binary::WriteShortString(buf, key);
				cpon_binary::SerializeDataItem(buf, val);
				++itemCount;
			}
		}
		else
		{
			// ヒント順でキーを列挙する
			while(!hintWalk.empty())
			{
				// 次のヒントエントリを切り出す
				std::string entry = hintWalk;
				const size_t commaPos = hintWalk.find(',');
				if(commaPos != std::string::npos)
				{
					entry = hintWalk.substr(0, commaPos);
					hintWalk = hintWalk.substr(commaPos + 1);
				}
				else
				{
					hintWalk.clear();
				}

				// キー名を取り出す (コロンの前まで)
				const size_t colonPos = entry.find(':');
				if(colonPos == std::string::npos) continue;
				const std::string key = entry.substr(0, colonPos);

				auto itr = block->m_BlockData.find(key);
				if(itr == block->m_BlockData.end()) continue;

				cpon_binary::WriteShortString(buf, key);
				cpon_binary::SerializeDataItem(buf, itr->second);
				++itemCount;
			}
		}

		// プレースホルダをデータ項目数で上書き
		buf[countPos + 0] = static_cast<uint8_t>(itemCount & 0xFF);
		buf[countPos + 1] = static_cast<uint8_t>((itemCount >> 8) & 0xFF);
		buf[countPos + 2] = static_cast<uint8_t>((itemCount >> 16) & 0xFF);
		buf[countPos + 3] = static_cast<uint8_t>((itemCount >> 24) & 0xFF);
	}
}
bool cpon_binary_helper::DeserializeValue(cpon_binary::Reader &r, std::shared_ptr<cpon_block> &block, const std::string &key, uint8_t typeId)
{
	using TID = cpon_binary::TypeID;
	switch(static_cast<TID>(typeId))
	{
	case TID::String:
	{
		std::string s;
		if(!r.ReadString(s)) return false;
		block->SetValue(key, s);
		break;
	}
	case TID::Int:
	{
		int32_t v;
		if(!r.ReadI32(v)) return false;
		block->SetValue(key, static_cast<int>(v));
		break;
	}
	case TID::UInt:
	{
		uint32_t v;
		if(!r.ReadU32(v)) return false;
		block->SetValue(key, static_cast<unsigned int>(v));
		break;
	}
	case TID::Float:
	{
		float v;
		if(!r.ReadF32(v)) return false;
		block->SetValue(key, v);
		break;
	}
	case TID::Double:
	{
		double v;
		if(!r.ReadF64(v)) return false;
		block->SetValue(key, v);
		break;
	}
	case TID::Bool:
	{
		uint8_t v;
		if(!r.ReadU8(v)) return false;
		block->SetValue(key, v != 0);
		break;
	}
	case TID::ArrayString:
	{
		uint32_t cnt;
		if(!r.ReadU32(cnt)) return false;
		std::vector<std::string> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			std::string s;
			if(!r.ReadString(s)) return false;
			arr.push_back(std::move(s));
		}
		block->SetArray<std::string>(key, arr);
		break;
	}
	case TID::ArrayInt:
	{
		uint32_t cnt;
		if(!r.ReadU32(cnt)) return false;
		std::vector<int> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			int32_t v;
			if(!r.ReadI32(v)) return false;
			arr.push_back(static_cast<int>(v));
		}
		block->SetArray<int>(key, arr);
		break;
	}
	case TID::ArrayUInt:
	{
		uint32_t cnt;
		if(!r.ReadU32(cnt)) return false;
		std::vector<unsigned int> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			uint32_t v;
			if(!r.ReadU32(v)) return false;
			arr.push_back(static_cast<unsigned int>(v));
		}
		block->SetArray<unsigned int>(key, arr);
		break;
	}
	case TID::ArrayFloat:
	{
		uint32_t cnt;
		if(!r.ReadU32(cnt)) return false;
		std::vector<float> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			float v;
			if(!r.ReadF32(v)) return false;
			arr.push_back(v);
		}
		block->SetArray<float>(key, arr);
		break;
	}
	case TID::ArrayDouble:
	{
		uint32_t cnt;
		if(!r.ReadU32(cnt)) return false;
		std::vector<double> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			double v;
			if(!r.ReadF64(v)) return false;
			arr.push_back(v);
		}
		block->SetArray<double>(key, arr);
		break;
	}
	case TID::ArrayBool:
	{
		uint32_t cnt;
		if(!r.ReadU32(cnt)) return false;
		std::vector<bool> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			uint8_t v;
			if(!r.ReadU8(v)) return false;
			arr.push_back(v != 0);
		}
		block->SetArray<bool>(key, arr);
		break;
	}
	case TID::Object:
	{
		// ネストオブジェクトの読み込み (再帰対応)
		// block->CreateObject でshared_ptrを取得し、そこにブロックを追加する
		auto nestedObj = block->CreateObject(key);

		// シリアライズ時と同じ順序でオブジェクト名・ヒント・ブロックを読む
		std::string objName, blockHints;
		if(!r.ReadShortString(objName)) return false;
		if(!r.ReadShortString(blockHints)) return false;
		nestedObj->SetObjectName(objName);
		nestedObj->SetHints(blockHints);

		uint32_t blockCount;
		if(!r.ReadU32(blockCount)) return false;

		for(uint32_t bi = 0; bi < blockCount; ++bi)
		{
			auto nestedBlock = nestedObj->CreateDataBlock();

			uint32_t itemCount;
			if(!r.ReadU32(itemCount)) return false;

			for(uint32_t di = 0; di < itemCount; ++di)
			{
				std::string nestedKey;
				if(!r.ReadShortString(nestedKey)) return false;
				uint8_t nestedTypeId;
				if(!r.ReadU8(nestedTypeId)) return false;
				// 再帰的に値を読み込む (ネストオブジェクトがさらに深くてもOK)
				if(!DeserializeValue(r, nestedBlock, nestedKey, nestedTypeId)) return false;
			}
		}
		break;
	}
	default:
		std::cerr << "未知の型ID: 0x" << std::hex << static_cast<int>(typeId) << std::dec << std::endl;
		return false;
	}
	return true;
}
