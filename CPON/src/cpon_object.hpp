/*+===================================================================
	File: cpon_object.hpp
	Summary: cponのオブジェクトクラス
	Author: ryuu3160
	Date: 2025/12/6 Sat PM 10:08:06 初回作成
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
#include <unordered_map>
#include <charconv>
#include <variant>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <optional>

#undef GetObject

// ==============================
//	定数定義
// ==============================
namespace
{
	template<typename T>
	concept TypeValue = std::is_same_v<T, std::string> || std::is_same_v<T, int> || std::is_same_v<T, unsigned int> || std::is_same_v<T, float> ||
						std::is_same_v<T, double> || std::is_same_v<T, bool>;
}

class cpon_block
{
	friend class cpon;
	friend class cpon_object;
public:
	using DataValue = std::variant<std::string, int, unsigned int, float, double, bool>;
	using Array = std::variant<std::vector<std::string>, std::vector<int>, std::vector<unsigned int>, std::vector<float>, std::vector<double>, std::vector<bool>>;
	using Object = std::shared_ptr<cpon_object>;

	using DataItem = std::variant<DataValue, Array, Object>;

public:
	cpon_block(_In_ std::string &In_BlockHints, _In_ int In_NestedLevel = 1)
		: m_BlockHintsRef(In_BlockHints), m_NestedLevel(In_NestedLevel)
	{
	}
	~cpon_block()
	{
		m_BlockData.clear();
	}

	/**
	 * @brief 指定したキーに対応する値への参照を返す関数
	 * @tparam T 取得する値の型
	 * @param In_Key 検索するキー
	 * @return キーに対応する値への参照(T&)、存在しないキーでは未定義動作になり得る
	 */
	template<TypeValue T>
	T &GetValue(_In_ const std::string_view In_Key)
	{
		auto itr = m_BlockData.find(std::string(In_Key));
		if(itr != m_BlockData.end())
		{
			if(std::holds_alternative<DataValue>(itr->second))
			{
				auto &value = std::get<DataValue>(itr->second);

				if(std::holds_alternative<T>(value))
					return std::get<T>(value);
				else
				{
					std::cerr << "保持している型と指定した型が違います" << std::endl << "T : " << typeid(T).name() << std::endl
						<< "保持している型 : " << typeid(std::decay_t<decltype(value)>).name() << std::endl;
					return *(T *)nullptr;
				}
			}
			else
				throw std::bad_variant_access();
		}
		else
		{
			std::cerr << "キーが見つかりませんでした : " << In_Key << std::endl;
			return *(T *)nullptr;
		}
	}

	/**
	 * @brief 指定したキーに対応する値へのポインタを返す関数
	 * @tparam T 取得する値の型
	 * @param In_Key 検索するキー
	 * @return キーに対応する値へのポインタ(T*)、存在しないキーや型の不一致の場合はnullptrを返す
	 */
	template<TypeValue T>
	T* GetValuePtr(_In_ const std::string_view In_Key)
	{
		auto itr = m_BlockData.find(std::string(In_Key));
		if(itr != m_BlockData.end())
		{
			if(std::holds_alternative<DataValue>(itr->second))
			{
				auto &value = std::get<DataValue>(itr->second);
				if(std::holds_alternative<T>(value))
					return &(std::get<T>(value));
				else
				{
					std::cerr << "保持している型と指定した型が違います" << std::endl << "T : " << typeid(T).name() << std::endl
						<< "保持している型 : " << typeid(std::decay_t<decltype(value)>).name() << std::endl;
					return nullptr;
				}
			}
			else
				throw std::bad_variant_access();
		}
		else
		{
			std::cerr << "キーが見つかりませんでした : " << In_Key << std::endl;
			return nullptr;
		}
	}

	/**
	 * @brief 指定されたキーに関連付けられた配列への参照を取得する関数
	 * @tparam T 配列の要素の型
	 * @param In_Key 検索する配列のキー
	 * @return 配列への参照を含むoptional、キーが見つからない場合や型が一致しない場合はnulloptを返す
	 */
	template<TypeValue T>
	std::optional<std::reference_wrapper<std::vector<T>>> GetArray(_In_ const std::string_view In_Key)
	{
		auto itr = m_BlockData.find(std::string(In_Key));
		if(itr != m_BlockData.end())
		{
			if(std::holds_alternative<Array>(itr->second))
			{
				auto &array = std::get<Array>(itr->second);
				if(VariantArrayCheckType<T>(array))
				{
					return std::ref(VariantArrayToVector<T>(array));
				}
				else
				{
					std::cerr << "配列が保持している型と指定した型が違います" << std::endl;
					return std::nullopt;
				}
			}
			else
			{
				throw std::bad_variant_access();
			}
		}
		else
		{
			std::cerr << "キーが見つかりませんでした : " << In_Key << std::endl;
			return std::nullopt;
		}
	}

	/**
	* @brief 指定されたキーに関連付けられた配列へのポインタを取得する関数
	* @param In_Key 検索する配列のキー
	* @return キーが存在し、型が一致する場合はstd::vector<T>へのポインタ
	* @return 見つからない場合や型が一致しない場合はnullptr
	* @return 値がArray型でない場合はstd::bad_variant_accessをスローする
	**/
	template<TypeValue T>
	std::vector<T> *GetArrayPtr(_In_ const std::string_view In_Key)
	{
		auto itr = m_BlockData.find(std::string(In_Key));
		if(itr != m_BlockData.end())
		{
			if(std::holds_alternative<Array>(itr->second))
			{
				auto &array = std::get<Array>(itr->second);
				if(VariantArrayCheckType<T>(array))
					return &(VariantArrayToVector<T>(array));
				else
				{
					std::cerr << "配列が保持している型と指定した型が違います" << std::endl;
					return nullptr;
				}
			}
			else
			{
				throw std::bad_variant_access();
			}
		}
		else
		{
			std::cerr << "キーが見つかりませんでした : " << In_Key << std::endl;
			return nullptr;
		}
	}

	/**
	 * @brief 指定されたキーに関連付けて値を設定する関数
	 * @param In_Key 値を設定する対象のキーを表す文字列ビュー
	 * @param In_Value 設定する値
	 */
	void SetValue(_In_ const std::string_view In_Key, _In_ const DataItem &In_Value);

	/**
	 * @brief 指定されたキーに関連付けて、初期値で配列を作成する関数
	 * @tparam T 配列の要素の型
	 * @param In_Key 配列を識別するためのキー
	 * @param In_Value 配列の各要素を初期化するための値
	 * @param In_Count 作成する配列の要素数。デフォルトは1
	 * @return 作成された配列へのポインタ
	 */
	template<TypeValue T>
	std::vector<T> *CreateArray(_In_ const std::string_view In_Key, _In_ T In_Value, _In_ const size_t In_Count = 1)
	{
		std::vector<T> array;
		array.resize(In_Count, In_Value);

		auto res = m_BlockData.try_emplace(std::string(In_Key), array);
		if(!res.second)
			m_BlockData[std::string(In_Key)] = array;
		else
			CreateHints(In_Key, array);
		return &(std::get<std::vector<T>>(std::get<Array>(m_BlockData[std::string(In_Key)])));
	}

	/**
	 * @brief 指定されたキーに関連付けて、配列をデータ構造に設定する関数
	 * @tparam T 配列の要素の型
	 * @param In_Key 配列を識別するためのキー
	 * @param In_Values 格納する値のベクター
	 * @return 格納されたベクターへのポインター
	 */
	template<TypeValue T>
	std::vector<T> *SetArray(_In_ const std::string_view In_Key, _In_ const std::vector<T> &In_Values)
	{
		std::vector<T> array;
		for(const auto &value : In_Values)
		{
			array.push_back(value);
		}

		auto res = m_BlockData.try_emplace(std::string(In_Key), array);
		if(!res.second)
			m_BlockData[std::string(In_Key)] = array;
		else
			CreateHints(In_Key, array);
		return &(std::get<std::vector<T>>(std::get<Array>(m_BlockData[std::string(In_Key)])));
	}

	/**
	 * @brief 指定されたキーに関連付けて、既存の配列をデータ構造に設定する関数
	 * @param In_Key 配列を識別するためのキー
	 * @param In_Array 設定する配列
	 * @return 設定された配列へのポインタ
	 */
	Array *SetArray(_In_ const std::string_view In_Key, _In_ const Array &In_Array);

	/**
	 * @brief 指定されたキーに関連付けて、新しいObjectを作成し、データ構造に設定する関数
	 * @param In_Key 作成するオブジェクトのキー
	 * @return 作成されたObject
	 */
	Object CreateObject(_In_ const std::string_view In_Key);

	/**
	 * @brief 既存のオブジェクトをブロックデータに追加する関数
	 * @param In_Object 追加するオブジェクト
	 * @return 追加されたオブジェクト
	 */
	Object AddObject(_In_ Object In_Object);

	/**
	 * @brief 指定されたキーに関連付けられたオブジェクトを取得する関数
	 * @param In_Key 取得するオブジェクトを識別するキー
	 * @return 指定したキーに対応するObject
	 * @return キーが見つからない場合の挙動は実装依存であり、nullptrを返す可能性がある
	 */
	Object GetObject(_In_ const std::string_view In_Key);

	/**
	 * @brief ブロックデータが空であるかを確認する関数
	 * @return ブロックデータが空である場合はtrue、そうでない場合はfalseを返す
	 */
	[[nodiscard]] bool IsEmpty() const noexcept { return m_BlockData.empty(); }

private:

	// ブロックデータの要素を文字列として取得するためのビジター
	struct GetElementAsStringVisitor
	{
		std::size_t idx;
		template<typename Vec>
		std::optional<std::string> operator()(Vec const &vec) const
		{
			using Elem = typename Vec::value_type;
			if(idx >= vec.size())
				return std::nullopt;
			// vector<bool>のproxy等をElemに代入して扱うことで問題を回避
			Elem val = vec[idx];

			// std::string の場合はそのまま、bool は "true"/"false"、数値はstd::stringに変換
			if constexpr(std::is_same_v<Elem, std::string>)
			{
				return val;
			}
			else if constexpr(std::is_same_v<Elem, bool>)
			{
				// bool の場合は true/false を文字列で返す
				return val ? "true" : "false";
			}
			else if constexpr(std::is_arithmetic_v<Elem>)
			{
				std::string string;
				string.resize(std::numeric_limits<size_t>::digits10 + 2);
				auto res = std::to_chars(string.data(), string.data() + string.size(), val);
				string.resize(res.ptr - string.data());
				return string;
			}
			else
			{
				return std::nullopt;
			}
		}
	};

	/**
	 * @brief タグ名とデータ項目に基づいてヒントを作成する関数
	 * @param In_TagName ヒントを作成するためのタグ名
	 * @param In_Data ヒントに関連付けられるデータ項目
	 */
	void CreateHints(_In_ const std::string_view In_TagName, _In_ DataItem In_Data);

	/**
	 * @brief バリアント配列が指定された型を保持しているかどうかを確認する関数
	 * @tparam T 確認する配列要素の型
	 * @param In_Array チェックする配列
	 * @return 配列が指定された型を保持している場合はtrue、それ以外の場合はfalse
	 */
	template<TypeValue T>
	bool VariantArrayCheckType(_In_ Array In_Array)
	{
		if(std::holds_alternative<std::vector<T>>(In_Array))
		{
			return true;
		}
		return false;
	}

	// 型チェックが済んでいることを前提とする
	template<TypeValue T>
	std::vector<T> &VariantArrayToVector(_In_ Array &In_Array)
	{
		return std::get<std::vector<T>>(In_Array);
	}

private:
	int m_NestedLevel = 0;
	std::string &m_BlockHintsRef;
	std::unordered_map<std::string, DataItem> m_BlockData;
};

/**
 * @brief CPONのオブジェクトクラス
 * @brief 複数のcpon_blockを保持し、オブジェクト名やブロックヒントなどのメタデータを管理する
 */
class cpon_object
{
	friend class cpon;
	friend class cpon_block;
public:
	cpon_object(_In_ int In_NestedLevel = 0)
		: m_NestedLevel(In_NestedLevel)
	{
	}
	~cpon_object()
	{
		ClearData();
	}

	/**
	 * @brief 指定されたインデックスに対応するcpon_blockへの共有ポインタを返す関数
	 * @param In_Index アクセスするブロックのインデックス
	 * @return 指定したインデックスに対応するcpon_blockへの共有ポインタ
	 */
	std::shared_ptr<cpon_block> operator[](_In_ int In_Index);

	/// <summary>
	/// 新しいブロックを作成し、そのシェアポインタを返します
	/// </summary>
	/// <returns>作成されたブロックへの共有所有権を持つstd::shared_ptr</returns>
	
	/**
	 * @brief 新しいcpon_blockを作成し、オブジェクトのデータブロックのベクターに追加する関数
	 * @return 作成されたcpon_blockへの共有ポインタ
	 */
	std::shared_ptr<cpon_block> CreateDataBlock();

	/**
	 * @brief オブジェクトに含まれるデータブロックの数を取得する関数
	 * @return データブロックの数
	 */
	[[nodiscard]] int GetDataCount() const noexcept { return m_DataCount; }

	/**
	 * @brief オブジェクトの名前を取得する関数
	 * @return オブジェクト名への定数参照
	 */
	[[nodiscard]] const std::string &GetObjectName() const noexcept { return m_ObjectName; }

	/**
	 * @brief オブジェクトのブロックヒントを取得する関数
	 * @return ブロックヒントへの定数参照
	 */
	[[nodiscard]] const std::string &GetBlockHints() const noexcept { return m_BlockHints; }

	/**
	 * @brief オブジェクトのデータブロックが空であるかを確認する関数
	 * @return データブロックが空である場合はtrue、そうでない場合はfalseを返す
	 */
	[[nodiscard]] bool IsEmpty() const noexcept { return m_Data.empty(); }

	/**
	 * @brief オブジェクトの名前を設定する関数
	 * @param In_ObjectName 設定するオブジェクト名
	 */
	void SetObjectName(_In_ const std::string_view In_ObjectName) { m_ObjectName = std::string(In_ObjectName); }

	/**
	 * @brief オブジェクトに含まれるすべてのデータブロックをクリアする関数
	 */
	void ClearData() noexcept;

private:

	/**
	 * @brief オブジェクトのブロックヒントを取得する関数
	 * @return ブロックヒントへの定数参照
	 */
	std::string GetHints() const noexcept { return m_BlockHints; }

	/**
	 * @brief オブジェクトのブロックヒントを設定する関数
	 * @param In_Hints 設定するヒント文字列
	 * @return 設定されたヒント文字列
	 */
	std::string SetHints(_In_ const std::string_view In_Hints) noexcept { return m_BlockHints = std::string(In_Hints); }

	/**
	 * @brief オブジェクトのデータブロックの数を設定する関数
	 * @param In_Count 設定するデータブロックの数
	 */
	void SetDataCount(_In_ const int In_Count) noexcept { m_DataCount = In_Count; }

	/**
	 * @brief ブロックのネストレベルをリセットする関数
	 */
	void ResetBlockNestedLevel() noexcept;

	/**
	 * @brief オブジェクトのデータブロックを格納するベクターへの参照を取得する関数
	 * @return データブロックを格納するベクターへの参照
	 */
	std::vector<std::shared_ptr<cpon_block>> &GetDataBlocks() noexcept { return m_Data; }
	
private:
	int m_NestedLevel = 0;
	int m_DataCount = 0;
	std::string m_ObjectName;
	std::string m_BlockHints;
	std::vector<std::shared_ptr<cpon_block>> m_Data;
};
