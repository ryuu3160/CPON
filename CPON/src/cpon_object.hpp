/*+===================================================================
	File: cpon_object.hpp
	Summary: cponのオブジェクトクラス
	Author: ryuu3160
	Date: 2025/12/6 Sat PM 10:08:06 初回作成
===================================================================+*/
#pragma once

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
// ==============================
//	定数定義
// ==============================
namespace
{
	template<typename T>
	concept TypeValue = std::is_same_v<T, std::string> || std::is_same_v<T, int> || std::is_same_v<T, unsigned int> || std::is_same_v<T, float> ||
						std::is_same_v<T, double> || std::is_same_v<T, bool>;
}

/// <summary>
/// cpon_objectクラス
/// </summary>
class cpon_object
{
	friend class cpon;
public:
	class cpon_block
	{
		friend class cpon;
		friend class cpon_object;
	public:
		using DataValue = std::variant<std::string, int, unsigned int, float, double, bool>;
		using Array = std::variant<std::vector<std::string>, std::vector<int>, std::vector<unsigned int>, std::vector<float>, std::vector<double>, std::vector<bool>>;

		using DataItem = std::variant<DataValue, Array>;

	public:
		cpon_block(_In_ std::string &In_BlockHints)
			: m_BlockHintsRef(In_BlockHints)
		{
		}
		~cpon_block() = default;

		/// <summary>
		/// 指定したキーに対応する値への参照を返します
		/// </summary>
		/// <param name="[In_Key]">検索するキー。std::string_view 型で渡します</param>
		/// <returns>キーに対応する値への参照 (T&)。存在しないキーでは未定義動作になり得ます</returns>
		template<TypeValue T>
		T &GetValue(_In_ const std::string_view In_Key)
		{
			auto itr = m_BlockData.find(std::string(In_Key));
			if (itr != m_BlockData.end())
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

		template<TypeValue T>
		std::vector<T> &GetArray(_In_ const std::string_view In_Key)
		{
			auto itr = m_BlockData.find(std::string(In_Key));
			if (itr != m_BlockData.end())
			{
				if (std::holds_alternative<Array>(itr->second))
				{
					auto &array = std::get<Array>(itr->second);
					if(VariantArrayCheckType<T>(array))
						return VariantArrayToVector<T>(array);
					else
					{
						std::cerr << "配列が保持している型と指定した型が違います" << std::endl;
						return *(std::vector<T> *)nullptr;
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
				return *(std::vector<T> *)nullptr;
			}
		}

		void SetValue(_In_ const std::string_view In_Key, _In_ const DataItem &In_Value)
		{
			auto result = m_BlockData.try_emplace(std::string(In_Key), In_Value);
			if(!result.second)
				m_BlockData[std::string(In_Key)] = In_Value;

			CreateHints(In_Key, In_Value);
		}

		template<TypeValue T>
		std::vector<T> *CreateArray(_In_ const std::string_view In_Key, _In_ T In_Value, _In_ const size_t In_Count = 1)
		{
			std::vector<T> array;
			array.resize(In_Count, In_Value);
			m_BlockData[std::string(In_Key)] = array;
			CreateHints(In_Key, array);
			return &(std::get<std::vector<T>>(std::get<Array>(m_BlockData[std::string(In_Key)])));
		}

		template<TypeValue T>
		std::vector<T> *CreateArray(_In_ const std::string_view In_Key, _In_ const std::vector<T> &In_Values)
		{
			std::vector<T> array;
			for (const auto &value : In_Values)
			{
				array.push_back(value);
			}
			m_BlockData[std::string(In_Key)] = array;
			CreateHints(In_Key, array);
			return &(std::get<std::vector<T>>(std::get<Array>(m_BlockData[std::string(In_Key)])));
		}

		Array *CreateArray(_In_ const std::string_view In_Key, _In_ const Array &In_Values)
		{
			m_BlockData[std::string(In_Key)] = In_Values;
			CreateHints(In_Key, In_Values);
			return &(std::get<Array>(m_BlockData[std::string(In_Key)]));
		}

	private:
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


		void CreateHints(_In_ const std::string_view In_TagName, _In_ DataItem In_Data);

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
		std::string &m_BlockHintsRef;
		std::unordered_map<std::string, DataItem> m_BlockData;
	};

public:
	cpon_object() = default;
	~cpon_object() = default;

	cpon_block &operator[](_In_ int In_Index);

	cpon_block &CreateDataBlock()
	{
		m_Data.emplace_back(m_BlockHints);
		++m_DataCount;
		return m_Data.back();
	}

	[[nodiscard]] int GetDataCount() const noexcept { return m_DataCount; }
	[[nodiscard]] const std::string &GetObjectName() const noexcept { return m_ObjectName; }
	[[nodiscard]] const std::string &GetBlockHints() const noexcept { return m_BlockHints; }

	void SetObjectName(_In_ const std::string_view In_ObjectName) { m_ObjectName = std::string(In_ObjectName); }

private:

	std::string GetHints() const noexcept { return m_BlockHints; }
	std::string SetHints(_In_ const std::string_view In_Hints) noexcept { return m_BlockHints = std::string(In_Hints); }
	void SetDataCount(_In_ const int In_Count) noexcept { m_DataCount = In_Count; }

	std::vector<cpon_block> &GetDataBlocks() noexcept { return m_Data; }
	
	int m_DataCount = 0;
	std::string m_ObjectName;
	std::string m_BlockHints;
	std::vector<cpon_block> m_Data;
};
