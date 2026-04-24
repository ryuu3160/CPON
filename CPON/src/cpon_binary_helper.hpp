/*+===================================================================
	File: cpon_binary_helper.hpp
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
// バイナリファイルフォーマット仕様
// ==============================
// [マジックナンバー : 4B] "CPON"
// [バージョン       : 1B] 0x02
// [XORシード        : 4B] uint32_t 乱数シード (0=スクランブルなし)
// [圧縮前サイズ     : 4B] uint32_t 元データのバイト数
// [圧縮後サイズ     : 4B] uint32_t RLE圧縮後のバイト数
// [データ           : 可変] RLE圧縮 → XORスクランブル済みペイロード
//
// ペイロードのRLEフォーマット:
//   run(連続)	: [0x80 | (count-1) : 1B] [byte  :  1B]   count=1~128
//   lit(非連続): [(count-1)		: 1B] [bytes : N B]   count=1~128
//
// XORスクランブル:
//   xorshift32でシードから乱数列を生成し、各バイトとXORする
//   シード=0 のときはスクランブルなし
//
// 型IDとデータレイアウト (ペイロード内):
//   0x01 string         : [長さ:4B][UTF-8データ]
//   0x02 int            : [4B signed little-endian]
//   0x03 uint           : [4B unsigned little-endian]
//   0x04 float          : [4B IEEE754]
//   0x05 double         : [8B IEEE754]
//   0x06 bool           : [1B] 0=false, 1=true
//   0x10 array<string>  : [要素数:4B]([長さ:4B][UTF-8データ]...)
//   0x11 array<int>     : [要素数:4B]([4B signed]...)
//   0x12 array<uint>    : [要素数:4B]([4B unsigned]...)
//   0x13 array<float>   : [要素数:4B]([4B IEEE754]...)
//   0x14 array<double>  : [要素数:4B]([8B IEEE754]...)
//   0x15 array<bool>    : [要素数:4B]([1B]...)
//   0x20 object         : 再帰的にオブジェクト構造を書き込む

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
	// バージョン0x02: RLE圧縮 + XORスクランブル対応
	inline constexpr uint8_t MAGIC[4] = { 'C', 'P', 'O', 'N' };
	inline constexpr uint8_t VERSION = 0x02;

	// ----------------------------------------
	// バイト列書き込みヘルパー関数宣言
	// ----------------------------------------
	void WriteU8(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ uint8_t In_Value);
	void WriteU16(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ uint16_t In_Value);
	void WriteU32(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ uint32_t In_Value);
	void WriteI32(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ int32_t In_Value);
	void WriteF32(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ float In_Value);
	void WriteF64(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ double In_Value);
	void WriteString(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ const std::string &In_Str);
	void WriteShortString(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ const std::string &In_Str);

	// ----------------------------------------
	// RLE 圧縮 / 展開
	// ----------------------------------------

	/**
	 * @brief バイト列を簡易RLEで圧縮する
	 * @param In_Data 圧縮元データ
	 * @return 圧縮後のバイト列
	 */
	[[nodiscard]] std::vector<uint8_t> RleCompress(_In_ std::span<const uint8_t> In_Data);

	/**
	 * @brief RLE圧縮されたバイト列を展開する
	 * @param In_Data 圧縮データ
	 * @param In_OriginalSize 展開後の期待サイズ(超過チェックに使用)
	 * @param Out_Data 展開結果
	 * @return 成功した場合はtrue
	 */
	_Success_(return != false)
	[[nodiscard]] bool RleDecompress(_In_ std::span<const uint8_t> In_Data, _In_ uint32_t In_OriginalSize, _Out_ std::vector<uint8_t> &Out_Data);

	// ----------------------------------------
	// XOR スクランブル / デスクランブル
	// ----------------------------------------

	/**
	 * @brief xorshift32によるXORスクランブルをin-placeで適用する
	 * @param InOut_Data スクランブル対象バイト列
	 * @param In_Seed XORシード(0のときは何もしない)
	 */
	void XorScramble(_Inout_ std::vector<uint8_t> &InOut_Data, _In_ uint32_t In_Seed);

	/**
	 * @brief XORデスクランブル(スクランブルと同一処理)
	 */
	inline void XorDescramble(_Inout_ std::vector<uint8_t> &InOut_Data, _In_ uint32_t In_Seed)
	{
		XorScramble(InOut_Data, In_Seed);
	}

	// ----------------------------------------
	// リトルエンディアン読み込みヘルパー
	// ----------------------------------------
	class Reader
	{
	public:
		Reader(_In_ std::span<const uint8_t> In_Data, _In_ size_t In_Pos = 0)
			: m_Data(In_Data), m_Pos(In_Pos)
		{
		}

		[[nodiscard]] bool HasBytes(size_t n) const noexcept { return m_Pos + n <= m_Data.size(); }

		_Success_(return != false) bool ReadU8(_Out_ uint8_t &Out_Value);
		_Success_(return != false) bool ReadU16(_Out_ uint16_t &Out_Value);
		_Success_(return != false) bool ReadU32(_Out_ uint32_t &Out_Value);
		_Success_(return != false) bool ReadI32(_Out_ int32_t &Out_Value);
		_Success_(return != false) bool ReadF32(_Out_ float &Out_Value);
		_Success_(return != false) bool ReadF64(_Out_ double &Out_Value);
		_Success_(return != false) bool ReadString(_Out_ std::string &Out_Str); // [長さ:4B][データ]
		_Success_(return != false) bool ReadShortString(_Out_ std::string &Out_Str); // [長さ:2B][データ]

	private:
		std::span<const uint8_t> m_Data;
		size_t m_Pos = 0;
	};

} // namespace cpon_binary

// ==============================
// cpon_binary_helper クラス
// ==============================
class cpon_binary_helper
{
public:
	/**
	 * @brief cpon_objectをバイト列にシリアライズする (ネスト再帰対応)
	 */
	static void SerializeObject(_Inout_ std::vector<uint8_t> &InOut_Buf, _In_ const std::shared_ptr<cpon_object> &In_Obj);

	/**
	 * @brief バイト列から1つのデータ項目をデシリアライズする (ネスト再帰対応)
	 */
	static bool DeserializeValue(_In_ cpon_binary::Reader &In_Reader, _Inout_ std::shared_ptr<cpon_block> &InOut_Block, _In_ const std::string &In_Key, _In_ uint8_t In_TypeId);
};