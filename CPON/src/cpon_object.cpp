/*+===================================================================
	File: cpon_object.cpp
	Summary: cponのオブジェクトクラス
	Author: ryuu3160
	Date: 2025/12/6 Sat PM 10:08:11 初回作成
===================================================================+*/

// ==============================
//	include
// ==============================
#include "cpon_object.hpp"

cpon_object::cpon_block &cpon_object::operator[](_In_ int In_Index)
{
	if (m_Data.empty())
	{
		m_Data.emplace_back(m_BlockHints);
		++m_DataCount;
		return m_Data.back();
	}

	if (m_Data.size() <= static_cast<size_t>(In_Index))
		return m_Data.back();
	return m_Data[In_Index];
}

void cpon_object::cpon_block::CreateHints(_In_ const std::string_view In_TagName, _In_ DataItem In_Data)
{
	if (m_BlockHintsRef.find(std::string(In_TagName) + ":") != std::string::npos)
		return;

	if (m_BlockHintsRef.empty())
		m_BlockHintsRef = std::string(In_TagName) + ":";
	else
		m_BlockHintsRef += "," + std::string(In_TagName) + ":";

	if (std::holds_alternative<cpon_block::DataValue>(In_Data))
	{
		if (std::holds_alternative<std::string>(std::get<cpon_block::DataValue>(In_Data)))
			m_BlockHintsRef += "string";
		else if (std::holds_alternative<int>(std::get<cpon_block::DataValue>(In_Data)))
			m_BlockHintsRef += "int";
		else if (std::holds_alternative<unsigned int>(std::get<cpon_block::DataValue>(In_Data)))
			m_BlockHintsRef += "uint";
		else if (std::holds_alternative<float>(std::get<cpon_block::DataValue>(In_Data)))
			m_BlockHintsRef += "float";
		else if (std::holds_alternative<double>(std::get<cpon_block::DataValue>(In_Data)))
			m_BlockHintsRef += "double";
		else if (std::holds_alternative<bool>(std::get<cpon_block::DataValue>(In_Data)))
			m_BlockHintsRef += "bool";
	}
	else if (std::holds_alternative<cpon_block::Array>(In_Data))
	{
		auto array = std::get<cpon_block::Array>(In_Data);

		m_BlockHintsRef += "array";
		
		if (VariantArrayCheckType<std::string>(array))
			m_BlockHintsRef += "<string>";
		else if (VariantArrayCheckType<int>(array))
			m_BlockHintsRef += "<int>";
		else if (VariantArrayCheckType<unsigned int>(array))
			m_BlockHintsRef += "<uint>";
		else if (VariantArrayCheckType<float>(array))
			m_BlockHintsRef += "<float>";
		else if (VariantArrayCheckType<double>(array))
			m_BlockHintsRef += "<double>";
		else if (VariantArrayCheckType<bool>(array))
			m_BlockHintsRef += "<bool>";
		else
			m_BlockHintsRef += "<unknown>";
	}
	else if (std::holds_alternative<cpon_block::Object>(In_Data))
	{
		m_BlockHintsRef += "object";
	}
}
