/*+===================================================================
	File: cpon_binary_helper.cpp
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
#include <random>

namespace cpon_binary
{
	// ==============================
	// バイト列書き込みヘルパー
	// ==============================

	void WriteU8(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ uint8_t In_Value)
	{
		InOut_Buf.push_back(In_Value);
	}

	void WriteU16(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ uint16_t In_Value)
	{
		InOut_Buf.push_back(static_cast<uint8_t>(In_Value & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((In_Value >> 8) & 0xFF));
	}

	void WriteU32(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ uint32_t In_Value)
	{
		InOut_Buf.push_back(static_cast<uint8_t>(In_Value & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((In_Value >> 8) & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((In_Value >> 16) & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((In_Value >> 24) & 0xFF));
	}

	void WriteI32(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ int32_t In_Value)
	{
		WriteU32(InOut_Buf, static_cast<uint32_t>(In_Value));
	}

	void WriteF32(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ float In_Value)
	{
		uint32_t bits;
		std::memcpy(&bits, &In_Value, sizeof(bits));
		WriteU32(InOut_Buf, bits);
	}

	void WriteF64(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ double In_Value)
	{
		uint64_t bits;
		std::memcpy(&bits, &In_Value, sizeof(bits));
		InOut_Buf.push_back(static_cast<uint8_t>(bits & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((bits >> 32) & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((bits >> 40) & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((bits >> 48) & 0xFF));
		InOut_Buf.push_back(static_cast<uint8_t>((bits >> 56) & 0xFF));
	}

	void WriteString(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ const std::string &In_Str)
	{
		WriteU32(InOut_Buf, static_cast<uint32_t>(In_Str.size()));
		InOut_Buf.insert(InOut_Buf.end(), In_Str.begin(), In_Str.end());
	}

	void WriteShortString(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ const std::string &In_Str)
	{
		WriteU16(InOut_Buf, static_cast<uint16_t>(In_Str.size()));
		InOut_Buf.insert(InOut_Buf.end(), In_Str.begin(), In_Str.end());
	}

	// ==============================
	// RLE 圧縮
	// ==============================
	// フォーマット:
	//   run  (連続): トークン = 0x80 | (count-1)  続いて1バイト  count=1~128
	//   lit (非連続): トークン = (count-1)          続いてNバイト  count=1~128
	// ==============================

	std::vector<uint8_t> RleCompress(_In_ std::span<const uint8_t> In_Data)
	{
		std::vector<uint8_t> out;
		out.reserve(In_Data.size());

		const size_t n = In_Data.size();
		size_t i = 0;

		while(i < n)
		{
			// --- run の検出 ---
			size_t runLen = 1;
			while(runLen < 128 && i + runLen < n && In_Data[i + runLen] == In_Data[i])
				++runLen;

			if(runLen >= 2)
			{
				// run トークン: 0x80 | (runLen-1)
				out.push_back(static_cast<uint8_t>(0x80 | (runLen - 1)));
				out.push_back(In_Data[i]);
				i += runLen;
				continue;
			}

			// --- lit の検出 ---
			// 次にrunが始まる位置まで、または128バイトまで非連続として扱う
			size_t litLen = 0;
			size_t j = i;
			while(litLen < 128 && j < n)
			{
				// 2バイト以上連続するrunを見つけたらlit終了
				size_t ahead = 1;
				while(ahead < 2 && j + ahead < n && In_Data[j + ahead] == In_Data[j])
					++ahead;
				if(ahead >= 2) break;
				++litLen;
				++j;
			}
			if(litLen == 0) litLen = 1; // 安全策

			// lit トークン: (litLen-1)
			out.push_back(static_cast<uint8_t>(litLen - 1));
			out.insert(out.end(), &In_Data[i], &In_Data[i + litLen]);
			i += litLen;
		}

		return out;
	}

	// ==============================
	// RLE 展開
	// ==============================

	_Success_(return != false)
	bool RleDecompress(_In_ std::span<const uint8_t> In_Data, _In_ uint32_t In_OriginalSize, _Out_ std::vector<uint8_t> &Out_Data)
	{
		Out_Data.clear();
		Out_Data.reserve(In_OriginalSize);

		size_t i = 0;
		const size_t n = In_Data.size();

		while(i < n)
		{
			const uint8_t token = In_Data[i++];

			if(token & 0x80)
			{
				// run
				const uint32_t count = (token & 0x7F) + 1;
				if(i >= n)
				{
					std::cerr << "RLE展開エラー: runデータが不足しています" << std::endl;
					return false;
				}
				const uint8_t byte = In_Data[i++];

				if(Out_Data.size() + count > In_OriginalSize)
				{
					std::cerr << "RLE展開エラー: 展開後サイズが期待値を超えました" << std::endl;
					return false;
				}
				for(uint32_t k = 0; k < count; ++k)
					Out_Data.push_back(byte);
			}
			else
			{
				// lit
				const uint32_t count = static_cast<uint32_t>(token) + 1;
				if(i + count > n)
				{
					std::cerr << "RLE展開エラー: litデータが不足しています" << std::endl;
					return false;
				}
				if(Out_Data.size() + count > In_OriginalSize)
				{
					std::cerr << "RLE展開エラー: 展開後サイズが期待値を超えました" << std::endl;
					return false;
				}
				Out_Data.insert(Out_Data.end(), &In_Data[i], &In_Data[i + count]);
				i += count;
			}
		}

		if(Out_Data.size() != In_OriginalSize)
		{
			std::cerr << "RLE展開エラー: 展開後サイズが一致しません"
				<< " (期待=" << In_OriginalSize << " 実際=" << Out_Data.size() << ")" << std::endl;
			return false;
		}

		return true;
	}

	// ==============================
	// XOR スクランブル
	// ==============================
	// xorshift32 でシードから乱数列を生成し、各バイトとXORする
	// XORの性質上、同じ処理でデスクランブルも兼ねる
	// ==============================

	void XorScramble(_Inout_ std::vector<uint8_t> &InOut_Data, _In_ uint32_t In_Seed)
	{
		if(In_Seed == 0) return; // シード0はスクランブルなし

		uint32_t state = In_Seed;
		for(auto &byte : InOut_Data)
		{
			// xorshift32
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			byte ^= static_cast<uint8_t>(state & 0xFF);
		}
	}

	// ==============================
	// シリアライズ: DataValue
	// ==============================

	static void SerializeDataValue(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ const cpon_block::DataValue &In_Value)
	{
		if(std::holds_alternative<std::string>(In_Value))
		{
			WriteU8(InOut_Buf, static_cast<uint8_t>(TypeID::String));
			WriteString(InOut_Buf, std::get<std::string>(In_Value));
		}
		else if(std::holds_alternative<int>(In_Value))
		{
			WriteU8(InOut_Buf, static_cast<uint8_t>(TypeID::Int));
			WriteI32(InOut_Buf, std::get<int>(In_Value));
		}
		else if(std::holds_alternative<unsigned int>(In_Value))
		{
			WriteU8(InOut_Buf, static_cast<uint8_t>(TypeID::UInt));
			WriteU32(InOut_Buf, std::get<unsigned int>(In_Value));
		}
		else if(std::holds_alternative<float>(In_Value))
		{
			WriteU8(InOut_Buf, static_cast<uint8_t>(TypeID::Float));
			WriteF32(InOut_Buf, std::get<float>(In_Value));
		}
		else if(std::holds_alternative<double>(In_Value))
		{
			WriteU8(InOut_Buf, static_cast<uint8_t>(TypeID::Double));
			WriteF64(InOut_Buf, std::get<double>(In_Value));
		}
		else if(std::holds_alternative<bool>(In_Value))
		{
			WriteU8(InOut_Buf, static_cast<uint8_t>(TypeID::Bool));
			WriteU8(InOut_Buf, std::get<bool>(In_Value) ? 1 : 0);
		}
	}

	// ==============================
	// シリアライズ: Array
	// ==============================

	static void SerializeArray(_Out_ std::vector<uint8_t> &Out_Buf, _In_ const cpon_block::Array &In_Array)
	{
		std::visit([&Out_Buf](const auto &vec)
			{
				using Vec = std::decay_t<decltype(vec)>;
				using Elem = typename Vec::value_type;

				if constexpr(std::is_same_v<Elem, std::string>)
				{
					WriteU8(Out_Buf, static_cast<uint8_t>(TypeID::ArrayString));
					WriteU32(Out_Buf, static_cast<uint32_t>(vec.size()));
					for(const auto &s : vec)
						WriteString(Out_Buf, s);
				}
				else if constexpr(std::is_same_v<Elem, int>)
				{
					WriteU8(Out_Buf, static_cast<uint8_t>(TypeID::ArrayInt));
					WriteU32(Out_Buf, static_cast<uint32_t>(vec.size()));
					for(const auto &v : vec)
						WriteI32(Out_Buf, v);
				}
				else if constexpr(std::is_same_v<Elem, unsigned int>)
				{
					WriteU8(Out_Buf, static_cast<uint8_t>(TypeID::ArrayUInt));
					WriteU32(Out_Buf, static_cast<uint32_t>(vec.size()));
					for(const auto &v : vec)
						WriteU32(Out_Buf, v);
				}
				else if constexpr(std::is_same_v<Elem, float>)
				{
					WriteU8(Out_Buf, static_cast<uint8_t>(TypeID::ArrayFloat));
					WriteU32(Out_Buf, static_cast<uint32_t>(vec.size()));
					for(const auto &v : vec) WriteF32(Out_Buf, v);
				}
				else if constexpr(std::is_same_v<Elem, double>)
				{
					WriteU8(Out_Buf, static_cast<uint8_t>(TypeID::ArrayDouble));
					WriteU32(Out_Buf, static_cast<uint32_t>(vec.size()));
					for(const auto &v : vec)
						WriteF64(Out_Buf, v);
				}
				else if constexpr(std::is_same_v<Elem, bool>)
				{
					WriteU8(Out_Buf, static_cast<uint8_t>(TypeID::ArrayBool));
					WriteU32(Out_Buf, static_cast<uint32_t>(vec.size()));
					for(bool v : vec)
						WriteU8(Out_Buf, v ? 1 : 0);
				}
			}, In_Array);
	}

	// ==============================
	// シリアライズ: DataItem (DataValue / Array / Object)
	// ==============================

	void SerializeDataItem(std::vector<uint8_t> &buf, const cpon_block::DataItem &item)
	{
		if(std::holds_alternative<cpon_block::DataValue>(item))
			SerializeDataValue(buf, std::get<cpon_block::DataValue>(item));
		else if(std::holds_alternative<cpon_block::Array>(item))
			SerializeArray(buf, std::get<cpon_block::Array>(item));
		else if(std::holds_alternative<cpon_block::Object>(item))
		{
			WriteU8(buf, static_cast<uint8_t>(TypeID::Object));
			cpon_binary_helper::SerializeObject(buf, std::get<cpon_block::Object>(item));
		}
	}

	// ==============================
	// Reader メンバ実装
	// ==============================

	_Success_(return != false)
	bool Reader::ReadU8(_Out_ uint8_t &Out_Value)
	{
		if(!HasBytes(1))
			return false;
		Out_Value = m_Data[m_Pos++];
		return true;
	}
	_Success_(return != false)
	bool Reader::ReadU16(_Out_ uint16_t &Out_Value)
	{
		if(!HasBytes(2))
			return false;
		Out_Value = static_cast<uint16_t>(m_Data[m_Pos])
			| (static_cast<uint16_t>(m_Data[m_Pos + 1]) << 8);
		m_Pos += 2;
		return true;
	}
	_Success_(return != false)
	bool Reader::ReadU32(_Out_ uint32_t &Out_Value)
	{
		if(!HasBytes(4))
			return false;
		Out_Value = static_cast<uint32_t>(m_Data[m_Pos])
			| (static_cast<uint32_t>(m_Data[m_Pos + 1]) << 8)
			| (static_cast<uint32_t>(m_Data[m_Pos + 2]) << 16)
			| (static_cast<uint32_t>(m_Data[m_Pos + 3]) << 24);
		m_Pos += 4;
		return true;
	}
	_Success_(return != false)
	bool Reader::ReadI32(_Out_ int32_t &Out_Value)
	{
		uint32_t u;
		if(!ReadU32(u))
			return false;
		Out_Value = static_cast<int32_t>(u);
		return true;
	}
	_Success_(return != false)
	bool Reader::ReadF32(_Out_ float &Out_Value)
	{
		uint32_t bits;
		if(!ReadU32(bits))
			return false;
		std::memcpy(&Out_Value, &bits, sizeof(Out_Value));
		return true;
	}
	_Success_(return != false)
	bool Reader::ReadF64(_Out_ double &Out_Value)
	{
		if(!HasBytes(8))
			return false;
		uint64_t bits =
			static_cast<uint64_t>(m_Data[m_Pos])
			| (static_cast<uint64_t>(m_Data[m_Pos + 1]) << 8)
			| (static_cast<uint64_t>(m_Data[m_Pos + 2]) << 16)
			| (static_cast<uint64_t>(m_Data[m_Pos + 3]) << 24)
			| (static_cast<uint64_t>(m_Data[m_Pos + 4]) << 32)
			| (static_cast<uint64_t>(m_Data[m_Pos + 5]) << 40)
			| (static_cast<uint64_t>(m_Data[m_Pos + 6]) << 48)
			| (static_cast<uint64_t>(m_Data[m_Pos + 7]) << 56);
		m_Pos += 8;
		std::memcpy(&Out_Value, &bits, sizeof(Out_Value));
		return true;
	}
	_Success_(return != false)
	bool Reader::ReadString(_Out_ std::string &Out_Str)
	{
		uint32_t len;
		if(!ReadU32(len))
			return false;
		if(!HasBytes(len))
			return false;
		Out_Str.assign(reinterpret_cast<const char *>(&m_Data[m_Pos]), len);
		m_Pos += len;
		return true;
	}
	_Success_(return != false)
	bool Reader::ReadShortString(_Out_ std::string &Out_Str)
	{
		uint16_t len;
		if(!ReadU16(len))
			return false;
		if(!HasBytes(len))
			return false;
		Out_Str.assign(reinterpret_cast<const char *>(&m_Data[m_Pos]), len);
		m_Pos += len;
		return true;
	}

} // namespace cpon_binary

// ==============================
// cpon_binary_helper の実装
// ==============================

void cpon_binary_helper::SerializeObject(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ const std::shared_ptr<cpon_object> &In_Obj)
{
	cpon_binary::WriteShortString(InOut_Buf, In_Obj->GetObjectName());
	cpon_binary::WriteShortString(InOut_Buf, In_Obj->GetBlockHints());

	const auto &blocks = const_cast<cpon_object *>(In_Obj.get())->GetDataBlocks();
	cpon_binary::WriteU32(InOut_Buf, static_cast<uint32_t>(blocks.size()));

	for(const auto &block : blocks)
	{
		const size_t countPos = InOut_Buf.size();
		cpon_binary::WriteU32(InOut_Buf, 0); // placeholder

		uint32_t itemCount = 0;
		const std::string &hints = In_Obj->GetBlockHints();
		std::string hintWalk = hints;

		if(hints.empty())
		{
			for(const auto &[key, val] : block->m_BlockData)
			{
				cpon_binary::WriteShortString(InOut_Buf, key);
				cpon_binary::SerializeDataItem(InOut_Buf, val);
				++itemCount;
			}
		}
		else
		{
			while(!hintWalk.empty())
			{
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

				const size_t colonPos = entry.find(':');
				if(colonPos == std::string::npos)
					continue;
				const std::string key = entry.substr(0, colonPos);

				auto itr = block->m_BlockData.find(key);
				if(itr == block->m_BlockData.end())
					continue;

				cpon_binary::WriteShortString(InOut_Buf, key);
				cpon_binary::SerializeDataItem(InOut_Buf, itr->second);
				++itemCount;
			}
		}

		InOut_Buf[countPos + 0] = static_cast<uint8_t>(itemCount & 0xFF);
		InOut_Buf[countPos + 1] = static_cast<uint8_t>((itemCount >> 8) & 0xFF);
		InOut_Buf[countPos + 2] = static_cast<uint8_t>((itemCount >> 16) & 0xFF);
		InOut_Buf[countPos + 3] = static_cast<uint8_t>((itemCount >> 24) & 0xFF);
	}
}

bool cpon_binary_helper::DeserializeValue(_In_ cpon_binary::Reader &In_Reader, _Inout_ std::shared_ptr<cpon_block> &InOut_Block, _In_ const std::string &In_Key, _In_ uint8_t In_TypeId)
{
	using TID = cpon_binary::TypeID;
	switch(static_cast<TID>(In_TypeId))
	{
	case TID::String:
	{
		std::string s;
		if(!In_Reader.ReadString(s))
			return false;
		InOut_Block->SetValue(In_Key, s);
		break;
	}
	case TID::Int:
	{
		int32_t v;
		if(!In_Reader.ReadI32(v))
			return false;
		InOut_Block->SetValue(In_Key, static_cast<int>(v));
		break;
	}
	case TID::UInt:
	{
		uint32_t v;
		if(!In_Reader.ReadU32(v))
			return false;
		InOut_Block->SetValue(In_Key, static_cast<unsigned int>(v));
		break;
	}
	case TID::Float:
	{
		float v;
		if(!In_Reader.ReadF32(v))
			return false;
		InOut_Block->SetValue(In_Key, v);
		break;
	}
	case TID::Double:
	{
		double v;
		if(!In_Reader.ReadF64(v))
			return false;
		InOut_Block->SetValue(In_Key, v);
		break;
	}
	case TID::Bool:
	{
		uint8_t v;
		if(!In_Reader.ReadU8(v))
			return false;
		InOut_Block->SetValue(In_Key, v != 0);
		break;
	}
	case TID::ArrayString:
	{
		uint32_t cnt;
		if(!In_Reader.ReadU32(cnt))
			return false;
		std::vector<std::string> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			std::string s;
			if(!In_Reader.ReadString(s))
				return false;
			arr.push_back(std::move(s));
		}
		InOut_Block->SetArray<std::string>(In_Key, arr);
		break;
	}
	case TID::ArrayInt:
	{
		uint32_t cnt;
		if(!In_Reader.ReadU32(cnt))
			return false;
		std::vector<int> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			int32_t v;
			if(!In_Reader.ReadI32(v))
				return false;
			arr.push_back(static_cast<int>(v));
		}
		InOut_Block->SetArray<int>(In_Key, arr);
		break;
	}
	case TID::ArrayUInt:
	{
		uint32_t cnt;
		if(!In_Reader.ReadU32(cnt))
			return false;
		std::vector<unsigned int> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			uint32_t v;
			if(!In_Reader.ReadU32(v))
				return false;
			arr.push_back(static_cast<unsigned int>(v));
		}
		InOut_Block->SetArray<unsigned int>(In_Key, arr);
		break;
	}
	case TID::ArrayFloat:
	{
		uint32_t cnt;
		if(!In_Reader.ReadU32(cnt))
			return false;
		std::vector<float> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			float v;
			if(!In_Reader.ReadF32(v))
				return false;
			arr.push_back(v);
		}
		InOut_Block->SetArray<float>(In_Key, arr);
		break;
	}
	case TID::ArrayDouble:
	{
		uint32_t cnt;
		if(!In_Reader.ReadU32(cnt))
			return false;
		std::vector<double> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			double v;
			if(!In_Reader.ReadF64(v))
				return false;
			arr.push_back(v);
		}
		InOut_Block->SetArray<double>(In_Key, arr);
		break;
	}
	case TID::ArrayBool:
	{
		uint32_t cnt;
		if(!In_Reader.ReadU32(cnt))
			return false;
		std::vector<bool> arr;
		arr.reserve(cnt);
		for(uint32_t i = 0; i < cnt; ++i)
		{
			uint8_t v;
			if(!In_Reader.ReadU8(v))
				return false;
			arr.push_back(v != 0);
		}
		InOut_Block->SetArray<bool>(In_Key, arr);
		break;
	}
	case TID::Object:
	{
		auto nestedObj = InOut_Block->CreateObject(In_Key);

		std::string objName, blockHints;
		if(!In_Reader.ReadShortString(objName))
			return false;
		if(!In_Reader.ReadShortString(blockHints))
			return false;
		nestedObj->SetObjectName(objName);
		nestedObj->SetHints(blockHints);

		uint32_t blockCount;
		if(!In_Reader.ReadU32(blockCount))
			return false;

		for(uint32_t bi = 0; bi < blockCount; ++bi)
		{
			auto nestedBlock = nestedObj->CreateDataBlock();
			uint32_t itemCount;
			if(!In_Reader.ReadU32(itemCount))
				return false;

			for(uint32_t di = 0; di < itemCount; ++di)
			{
				std::string nestedKey;
				if(!In_Reader.ReadShortString(nestedKey))
					return false;
				uint8_t nestedTypeId;
				if(!In_Reader.ReadU8(nestedTypeId))
					return false;
				if(!DeserializeValue(In_Reader, nestedBlock, nestedKey, nestedTypeId))
					return false;
			}
		}
		break;
	}
	default:
		std::cerr << "未知の型ID: 0x" << std::hex << static_cast<int>(In_TypeId) << std::dec << std::endl;
		return false;
	}
	return true;
}