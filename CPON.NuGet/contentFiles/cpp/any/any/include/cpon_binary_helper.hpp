/*+===================================================================
	File: cpon_binary.hpp
	Summary: CPONのバイナリ変換機能
			 テキスト形式のCPONデータをバイナリ形式に相互変換し、
			 バイナリファイルへの入出力を行う
	Author: 青木雄一郎
	Date: 2026/04/23 Thu AM 10:16:15 初回作成
===================================================================+*/
#pragma once

// ==============================
//	include
// ==============================
#include "cpon.hpp"
#include <vector>
#include <cstdint>
#include <string>
#include <span>

// ==============================
//  前方宣言
// ==============================
class cpon_object;
class cpon_block;
class cpon_binary_helper;

// ==============================
// バイナリフォーマット仕様
// ==============================
// [マジックナンバー : 4B] "CPON"
// [バージョン      : 1B] 現在は 0x01
// [オブジェクト数  : 4B] uint32_t (リトルエンディアン)
// 以下、オブジェクト数分繰り返し:
//   [オブジェクト名長 : 2B] uint16_t
//   [オブジェクト名   : N B] UTF-8文字列
//   [ブロックヒント長 : 2B] uint16_t
//   [ブロックヒント   : N B] UTF-8文字列
//   [ブロック数       : 4B] uint32_t
//   以下、ブロック数分繰り返し:
//     [データ項目数 : 4B] uint32_t
//     以下、データ項目数分繰り返し:
//       [キー長   : 2B] uint16_t
//       [キー     : N B] UTF-8文字列
//       [型ID     : 1B] CponBinaryTypeID参照
//       [値       : 可変長]
//
// 型IDとデータレイアウト:
//   0x01 string  : [長さ:4B][UTF-8データ]
//   0x02 int     : [4B signed little-endian]
//   0x03 uint    : [4B unsigned little-endian]
//   0x04 float   : [4B IEEE754]
//   0x05 double  : [8B IEEE754]
//   0x06 bool    : [1B] 0=false, 1=true
//   0x10 array<string>  : [要素数:4B]([長さ:4B][UTF-8データ]...)
//   0x11 array<int>     : [要素数:4B]([4B signed]...)
//   0x12 array<uint>    : [要素数:4B]([4B unsigned]...)
//   0x13 array<float>   : [要素数:4B]([4B IEEE754]...)
//   0x14 array<double>  : [要素数:4B]([8B IEEE754]...)
//   0x15 array<bool>    : [要素数:4B]([1B]...)
//   0x20 object  : 再帰的にオブジェクト構造を書き込む (ネストオブジェクト用)

namespace cpon_binary
{
	// ==============================
	// 型ID定義
	// ==============================
	enum class TypeID : uint8_t
	{
		String = 0x01,
		Int = 0x02,
		UInt = 0x03,
		Float = 0x04,
		Double = 0x05,
		Bool = 0x06,
		ArrayString = 0x10,
		ArrayInt = 0x11,
		ArrayUInt = 0x12,
		ArrayFloat = 0x13,
		ArrayDouble = 0x14,
		ArrayBool = 0x15,
		Object = 0x20,
	};

	// マジックナンバーとバージョン
	inline constexpr uint8_t MAGIC[4] = { 'C', 'P', 'O', 'N' };
	inline constexpr uint8_t VERSION = 0x01;

	// ----------------------------------------
	// ヘルパー関数宣言
	// ----------------------------------------

	void WriteU8(std::vector<uint8_t> &buf, uint8_t v);
	void WriteU16(std::vector<uint8_t> &buf, uint16_t v);
	void WriteU32(std::vector<uint8_t> &buf, uint32_t v);
	void WriteI32(std::vector<uint8_t> &buf, int32_t v);
	void WriteF32(std::vector<uint8_t> &buf, float v);
	void WriteF64(std::vector<uint8_t> &buf, double v);
	void WriteString(std::vector<uint8_t> &buf, const std::string &s);
	void WriteShortString(std::vector<uint8_t> &buf, const std::string &s);

	// ----------------------------------------
	// リトルエンディアン読み込みヘルパー
	// ----------------------------------------

	class Reader
	{
	public:
		Reader(std::span<const uint8_t> In_Data, size_t In_Pos = 0) : data(In_Data), pos(In_Pos) {}
		inline bool HasBytes(size_t n) const noexcept { return pos + n <= data.size(); }
		bool ReadU8(uint8_t &out);
		bool ReadU16(uint16_t &out);
		bool ReadU32(uint32_t &out);
		bool ReadI32(int32_t &out);
		bool ReadF32(float &out);
		bool ReadF64(double &out);
		// 文字列: [長さ:4B][データ]
		bool ReadString(std::string &out);
		// 短い文字列: [長さ:2B][データ]
		bool ReadShortString(std::string &out);
	private:
		std::span<const uint8_t> data;
		size_t pos = 0;
	};

}

class cpon_binary_helper
{
public:
	static void SerializeObject(std::vector<uint8_t> &buf, const std::shared_ptr<cpon_object> &obj);
	static bool DeserializeValue(cpon_binary::Reader &r, std::shared_ptr<cpon_block> &block, const std::string &key, uint8_t typeId);
};
