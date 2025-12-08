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
		using Array = std::vector<DataValue>;

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
				if (std::holds_alternative<DataValue>(itr->second))
				{
					return std::get<T>(std::get<DataValue>(itr->second));
				}
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
					//return std::get<std::vector<T>>(std::get<Array>(itr->second));

					const Array &array = std::get<Array>(itr->second);

					std::vector<T> result;
					for (const auto &item : array)
					{
						if (std::holds_alternative<T>(item))
						{
							result.push_back(std::get<T>(item));
						}
						else
						{
							throw std::bad_variant_access();
						}
					}
					return result;
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
		Array *CreateArray(_In_ const std::string_view In_Key, _In_ T In_Value, _In_ const size_t In_Count = 1)
		{
			Array array;
			array.resize(In_Count, In_Value);
			m_BlockData[std::string(In_Key)] = array;
			CreateHints(In_Key, array);
			return &(std::get<Array>(m_BlockData[std::string(In_Key)]));
		}

		template<TypeValue T>
		Array *CreateArray(_In_ const std::string_view In_Key, _In_ const std::vector<T> &In_Values)
		{
			Array array;
			for (const auto &value : In_Values)
			{
				array.push_back(value);
			}
			m_BlockData[std::string(In_Key)] = array;
			CreateHints(In_Key, array);
			return &(std::get<Array>(m_BlockData[std::string(In_Key)]));
		}

		Array *CreateArray(_In_ const std::string_view In_Key, _In_ const Array &In_Values)
		{
			m_BlockData[std::string(In_Key)] = In_Values;
			CreateHints(In_Key, In_Values);
			return &(std::get<Array>(m_BlockData[std::string(In_Key)]));
		}

	private:
		void CreateHints(_In_ const std::string_view In_TagName, _In_ DataItem In_Data);
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
