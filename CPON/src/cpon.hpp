/*+===================================================================
	File: cpon.hpp
	Summary: cpon(C++ Object Notation)のパーサークラス
			 cponは、TONLやTOONを参考にした、C++向けのデータ記述言語です。
	Author: ryuu3160
	Date: 2025/12/6 Sat PM 10:03:46 初回作成
===================================================================+*/
#pragma once

#if defined(_MSC_VER)
#define CPP_STD _MSVC_LANG
#else
#define CPP_STD __cplusplus
#endif

// もしC++20未満の環境であれば、警告文を表示してコンパイルを中止します
#if CPP_STD < 202002L
#error ("C++20以降のコンパイラが必要です。")
#endif

// ==============================
//	include
// ==============================
#include <string>
#include <vector>
#include <span>
#include <cstdint>
#include "cpon_object.hpp"

/**
 * @brief CPONクラス
 */
class cpon
{
public:
	cpon() = default;
	~cpon() = default;

	/**
	 * @brief 指定したインデックスにあるオブジェクトへの参照を返す配列添字演算子
	 * @param In_Index アクセスするオブジェクトのインデックス
	 * @return 指定したインデックスにあるcpon_objectへの参照
	 */
	cpon_object &operator[](_In_ int In_Index);

	/**
	 * @brief 指定した名前に対応するcpon_objectへの参照を返す配列アクセス演算子
	 * @param In_ObjectName 取得するオブジェクトの名前を表す文字列ビュー
	 * @return 指定された名前に対応するcpon_objectへの参照
	 */
	cpon_object &operator[](_In_ std::string_view In_ObjectName);

	/**
	 * @brief 指定された名前のオブジェクトへのシェアポインタを返す関数
	 * @param In_ObjectName 作成するオブジェクトの名前を表す文字列ビュー
	 * @return 指定された名前のcpon_objectへのシェアポインタ(オブジェクトが存在しない場合はnullptrを返す)
	 */
	std::shared_ptr<cpon_object> GetObjectPtr(_In_ std::string_view In_ObjectName);

	/**
	 * @brief 指定された名前のオブジェクトを作成し、そのシェアポインタを返す関数
	 * @param In_ObjectName 作成するオブジェクトの名前を表す文字列ビュー
	 * @return 作成されたcpon_objectへのシェアポインタを返す
	 */
	std::shared_ptr<cpon_object> CreateObject(_In_ const std::string_view In_ObjectName);
	
	/**
	 * @brief 指定された名前のオブジェクトを作成する
	 * @brief 既に存在する場合は、そのオブジェクトを返す
	 * @param In_ObjectName 作成するオブジェクトの名前を表す文字列ビュー
	 * @return 作成された、または既に存在するcpon_objectへのシェアポインタを返す
	 */
	std::shared_ptr<cpon_object> TryCreateObject(_In_ std::string_view In_ObjectName);

	/**
	 * @brief 既に存在するオブジェクトを追加する
	 * @param In_Object 追加するcpon_objectへのシェアポインタ
	 */
	void AddObject(_In_ std::shared_ptr<cpon_object> In_Object) noexcept;
	
	/**
	 * @brief 格納されているオブジェクトの数を取得する
	 * @return 格納されているオブジェクトの数をint型で返す
	 */
	[[nodiscard]] int GetObjectCount() const noexcept { return static_cast<int>(m_Objects.size()); }

	/**
	 * @brief 格納されているすべてのオブジェクトデータをクリアする関数
	 */
	void ClearObjectsData() noexcept;

	/**
	 * @brief 格納されているオブジェクトが空であるかを確認する関数
	 * @return 格納されているオブジェクトが空である場合はtrue、そうでない場合はfalseを返す
	 */
	[[nodiscard]] bool IsEmpty() const noexcept { return m_Objects.empty(); }

	/**
	 * @brief 指定されたファイルパスにデータを書き込む関数
	 * @brief 拡張子が指定されていない場合は、自動的に".cpon"が付加される
	 * @param In_FilePath 書き込み先のファイルパスを表す文字列ビュー
	 * @return 書き込みに成功した場合はtrue、失敗した場合はfalseを返す
	 */
	bool WriteToFile(_In_ const std::string_view In_FilePath);

	/**
	 * @brief 指定されたファイルからデータを読み込む関数
	 * @brief 拡張子が指定されていない場合は、自動的に".cpon"が付加される
	 * @param In_FilePath 読み込むファイルのパスを表す文字列ビュー
	 * @return 読み込みに成功した場合はtrue、失敗した場合はfalseを返す
	 */
	bool LoadFromFile(_In_ const std::string_view In_FilePath);

	// ---------------------------------------------
	// バイナリ関連
	// ---------------------------------------------

	// ==============================
	// シリアライズ (cpon -> バイト列)
	// ==============================

	/**
	 * @brief cponオブジェクト群をバイト列にシリアライズする
	 * @param In_Cpon シリアライズ対象のcponオブジェクト
	 * @return バイト列
	 */
	[[nodiscard]] std::vector<uint8_t> Serialize();

	/**
	 * @brief バイト列をcponオブジェクトにデシリアライズする
	 * @param In_Data デシリアライズ対象のバイト列
	 * @return 成功した場合はtrue
	 */
	[[nodiscard]] bool Deserialize(_In_ std::span<const uint8_t> In_Data);

	// ==============================
	// ファイル入出力
	// ==============================

	/**
	 * @brief cponデータをバイナリファイルに書き込む
	 * @brief 拡張子が指定されていない場合は ".cponb" が付加される
	 * @param In_Cpon 書き込むcponオブジェクト
	 * @param In_FilePath 書き込み先ファイルパス
	 * @return 成功した場合はtrue
	 */
	bool WriteToBinaryFile(_In_ std::string_view In_FilePath);

	/**
	 * @brief バイナリファイルからcponデータを読み込む
	 * @brief 拡張子が指定されていない場合は ".cponb" が付加される
	 * @param In_FilePath 読み込むファイルパス
	 * @return 成功した場合はtrue
	 */
	bool LoadFromBinaryFile(_In_ std::string_view In_FilePath);

private:
	std::vector<std::shared_ptr<cpon_object>> m_Objects;
};
