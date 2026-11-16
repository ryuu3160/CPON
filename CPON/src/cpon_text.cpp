// Internal helper: reads and writes the .cpon text format.
#include "cpon_text.hpp"
#include <algorithm>
#include <charconv>
#include <ostream>
#include <variant>
#include <string>
#include <string_view>

namespace
{
	// Deepest nesting of object blocks (the blocks of a top-level object are depth 1)
	inline constexpr int kMaxDepth = 64;

	// Number read with from_chars; stays 0 when the text cannot be parsed (extra trailing text is ignored)
	template<typename T>
	T ParseNumber(std::string_view In_Text)
	{
		T Value{};
		std::from_chars(In_Text.data(), In_Text.data() + In_Text.size(), Value);
		return Value;
	}

	// Parts of a block header "<prefix>[count]{hints}:"
	struct HeaderInfo
	{
		std::string_view Prefix; // text before '['
		int BlockCount = 0;
		std::string_view Hints;
		std::vector<cpon_detail::HintEntry> Entries;
	};

	// Parses "<prefix>[count]{hints}:<rest>". Returns false when a delimiter is missing or the hints are malformed.
	bool ParseHeader(std::string_view In_Line, HeaderInfo &Out_Info)
	{
		const size_t Open = In_Line.find('[');
		if(Open == std::string_view::npos)
			return false;
		const size_t Close = In_Line.find(']', Open + 1);
		if(Close == std::string_view::npos)
			return false;
		const size_t Brace = In_Line.find('{', Close + 1);
		if(Brace == std::string_view::npos)
			return false;
		const size_t End = In_Line.find("}:", Brace + 1);
		if(End == std::string_view::npos)
			return false;

		Out_Info.Prefix = In_Line.substr(0, Open);
		Out_Info.BlockCount = ParseNumber<int>(In_Line.substr(Open + 1, Close - Open - 1));
		Out_Info.Hints = In_Line.substr(Brace + 1, End - Brace - 1);
		return cpon_detail::ParseHints(Out_Info.Hints, Out_Info.Entries);
	}

	// Splits a data line "<indent>Key:value" at its first ':' (the key starts at the first non-blank character).
	// Returns false when the line has no key.
	bool SplitDataLine(std::string_view In_Line, std::string_view &Out_Key, std::string_view &Out_Value)
	{
		const size_t Start = In_Line.find_first_not_of(" \t");
		if(Start == std::string_view::npos)
			return false;
		const size_t Colon = In_Line.find(':', Start);
		if(Colon == std::string_view::npos)
			return false;
		Out_Key = In_Line.substr(Start, Colon - Start);
		Out_Value = In_Line.substr(Colon + 1);
		return true;
	}

	bool ParseBool(std::string_view In_Text)
	{
		return In_Text.find("true") != std::string_view::npos;
	}

	// Splits the text after "]" at ',' and drops one leading space of each element
	std::vector<std::string_view> SplitElements(std::string_view In_Text)
	{
		std::vector<std::string_view> Out;
		size_t Pos = 0;
		while(true)
		{
			const size_t Comma = In_Text.find(',', Pos);
			std::string_view Elem = In_Text.substr(Pos, Comma == std::string_view::npos ? std::string_view::npos : Comma - Pos);
			if(!Elem.empty() && Elem.front() == ' ')
				Elem.remove_prefix(1);
			Out.push_back(Elem);
			if(Comma == std::string_view::npos)
				break;
			Pos = Comma + 1;
		}
		return Out;
	}

	// The first In_Use elements as numbers (In_Use <= In_Elems.size())
	template<typename T>
	cpon_block::Array MakeNumberArray(const std::vector<std::string_view> &In_Elems, size_t In_Use)
	{
		std::vector<T> Values;
		for(size_t i = 0; i < In_Use; ++i)
			Values.push_back(ParseNumber<T>(In_Elems[i]));
		return cpon_block::Array(std::move(Values));
	}

	// Reads "[n]a, b, c" as an array of In_ElemType. Returns false for a malformed array or an unknown type.
	bool ReadArray(const std::shared_ptr<cpon_block> &In_Block, std::string_view In_Key, std::string_view In_ElemType, std::string_view In_Value)
	{
		const size_t Open = In_Value.find('[');
		if(Open == std::string_view::npos)
			return false;
		const size_t Close = In_Value.find(']', Open + 1);
		if(Close == std::string_view::npos)
			return false;

		const int Count = ParseNumber<int>(In_Value.substr(Open + 1, Close - Open - 1));
		const std::vector<std::string_view> Elems = SplitElements(In_Value.substr(Close + 1));
		const size_t Use = Count > 0 ? static_cast<size_t>(Count) : 0;
		// The count only limits how many elements are used: it is never used to allocate
		if(Use > Elems.size())
			return false;

		cpon_block::Array Array;
		if(In_ElemType == "string")
		{
			std::vector<std::string> Values;
			for(size_t i = 0; i < Use; ++i)
				Values.emplace_back(Elems[i]);
			Array = std::move(Values);
		}
		else if(In_ElemType == "int")
			Array = MakeNumberArray<int>(Elems, Use);
		else if(In_ElemType == "uint")
			Array = MakeNumberArray<unsigned int>(Elems, Use);
		else if(In_ElemType == "float")
			Array = MakeNumberArray<float>(Elems, Use);
		else if(In_ElemType == "double")
			Array = MakeNumberArray<double>(Elems, Use);
		else if(In_ElemType == "bool")
		{
			std::vector<bool> Values;
			for(size_t i = 0; i < Use; ++i)
				Values.push_back(ParseBool(Elems[i]));
			Array = std::move(Values);
		}
		else
			return false;

		In_Block->SetArray(In_Key, Array);
		return true;
	}

	// ---- v2 helpers ----

	// Whole text as a number; false when it is empty, has extra characters or is out of range
	template<typename T>
	bool ParseStrict(std::string_view In_Text, T &Out_Value)
	{
		if(In_Text.empty())
			return false;
		T Value{};
		const auto Res = std::from_chars(In_Text.data(), In_Text.data() + In_Text.size(), Value);
		if(Res.ec != std::errc() || Res.ptr != In_Text.data() + In_Text.size())
			return false;
		Out_Value = Value;
		return true;
	}

	// Non-negative decimal number made of digits only
	bool ParseCount(std::string_view In_Text, int &Out_Value)
	{
		if(In_Text.empty() || In_Text.front() < '0' || In_Text.front() > '9')
			return false;
		return ParseStrict<int>(In_Text, Out_Value);
	}

	bool ParseBoolStrict(std::string_view In_Text, bool &Out_Value)
	{
		if(In_Text == "true")
			Out_Value = true;
		else if(In_Text == "false")
			Out_Value = false;
		else
			return false;
		return true;
	}

	bool IsScalarTypeV2(std::string_view In_Type)
	{
		return In_Type == "string" || In_Type == "int" || In_Type == "uint"
			|| In_Type == "float" || In_Type == "double" || In_Type == "bool";
	}

	// Accepts the scalar types, "object", and "array<scalar>".
	bool IsKnownTypeV2(std::string_view In_Type)
	{
		if(IsScalarTypeV2(In_Type) || In_Type == "object")
			return true;
		constexpr std::string_view kArrayOpen = "array<";
		if(In_Type.size() > kArrayOpen.size() && In_Type.starts_with(kArrayOpen) && In_Type.ends_with('>'))
			return IsScalarTypeV2(In_Type.substr(kArrayOpen.size(), In_Type.size() - kArrayOpen.size() - 1));
		return false;
	}

	// Parses "<prefix>[count]{hints}:" (the whole text). Prefix is the text before the first '['.
	bool ParseHeaderV2(std::string_view In_Line, HeaderInfo &Out_Info)
	{
		const size_t Open = In_Line.find('[');
		if(Open == std::string_view::npos)
			return false;
		const size_t Close = In_Line.find(']', Open + 1);
		if(Close == std::string_view::npos)
			return false;
		int Count = 0;
		if(!ParseCount(In_Line.substr(Open + 1, Close - Open - 1), Count))
			return false;

		const std::string_view Rest = In_Line.substr(Close + 1);
		if(Rest.size() < 3 || Rest.front() != '{' || !Rest.ends_with("}:"))
			return false;

		Out_Info.Prefix = In_Line.substr(0, Open);
		Out_Info.BlockCount = Count;
		Out_Info.Hints = Rest.substr(1, Rest.size() - 3);
		if(!cpon_detail::ParseHints(Out_Info.Hints, Out_Info.Entries))
			return false;
		for(const auto &Entry : Out_Info.Entries)
		{
			if(!IsKnownTypeV2(Entry.Type))
				return false;
		}
		return true;
	}

	int HexDigit(char In_Char)
	{
		if(In_Char >= '0' && In_Char <= '9')
			return In_Char - '0';
		if(In_Char >= 'a' && In_Char <= 'f')
			return In_Char - 'a' + 10;
		if(In_Char >= 'A' && In_Char <= 'F')
			return In_Char - 'A' + 10;
		return -1;
	}

	// Reverts the v2 string escapes. Returns false for an unknown escape or a trailing '\'.
	bool Unescape(std::string_view In_Text, std::string &Out_Text)
	{
		std::string Out;
		for(size_t i = 0; i < In_Text.size(); ++i)
		{
			const char Ch = In_Text[i];
			if(Ch != '\\')
			{
				Out.push_back(Ch);
				continue;
			}
			if(++i >= In_Text.size())
				return false;
			switch(In_Text[i])
			{
			case '\\': Out.push_back('\\'); break;
			case 'n': Out.push_back('\n'); break;
			case 'r': Out.push_back('\r'); break;
			case ',': Out.push_back(','); break;
			case 'x':
			{
				if(i + 2 >= In_Text.size())
					return false;
				const int Hi = HexDigit(In_Text[i + 1]);
				const int Lo = HexDigit(In_Text[i + 2]);
				if(Hi < 0 || Lo < 0)
					return false;
				Out.push_back(static_cast<char>(Hi * 16 + Lo));
				i += 2;
				break;
			}
			default:
				return false;
			}
		}
		Out_Text = std::move(Out);
		return true;
	}

	// Splits at every ',' that is not escaped (a '\' skips the next character)
	std::vector<std::string_view> SplitUnescaped(std::string_view In_Text)
	{
		std::vector<std::string_view> Out;
		size_t Start = 0;
		for(size_t i = 0; i < In_Text.size(); ++i)
		{
			if(In_Text[i] == '\\')
				++i;
			else if(In_Text[i] == ',')
			{
				Out.push_back(In_Text.substr(Start, i - Start));
				Start = i + 1;
			}
		}
		Out.push_back(In_Text.substr(Start));
		return Out;
	}

	template<typename T>
	bool MakeNumberArrayStrict(const std::vector<std::string_view> &In_Elems, cpon_block::Array &Out_Array)
	{
		std::vector<T> Values;
		for(const auto &Elem : In_Elems)
		{
			T Value{};
			if(!ParseStrict<T>(Elem, Value))
				return false;
			Values.push_back(Value);
		}
		Out_Array = std::move(Values);
		return true;
	}

	// Reads "[n]a,b,c" as an array of In_ElemType
	bool ReadArrayV2(const std::shared_ptr<cpon_block> &In_Block, std::string_view In_Key, std::string_view In_ElemType, std::string_view In_Value)
	{
		if(In_Value.empty() || In_Value.front() != '[')
			return false;
		const size_t Close = In_Value.find(']');
		if(Close == std::string_view::npos)
			return false;
		int Count = 0;
		if(!ParseCount(In_Value.substr(1, Close - 1), Count))
			return false;

		const std::string_view Rest = In_Value.substr(Close + 1);
		std::vector<std::string_view> Elems;
		if(Count == 0)
		{
			if(!Rest.empty())
				return false;
		}
		else
		{
			Elems = SplitUnescaped(Rest);
			// The count is only compared: it is never used to allocate
			if(Elems.size() != static_cast<size_t>(Count))
				return false;
		}

		cpon_block::Array Array;
		if(In_ElemType == "string")
		{
			std::vector<std::string> Values;
			for(const auto &Elem : Elems)
			{
				std::string Text;
				if(!Unescape(Elem, Text))
					return false;
				Values.push_back(std::move(Text));
			}
			Array = std::move(Values);
		}
		else if(In_ElemType == "int")
		{
			if(!MakeNumberArrayStrict<int>(Elems, Array))
				return false;
		}
		else if(In_ElemType == "uint")
		{
			if(!MakeNumberArrayStrict<unsigned int>(Elems, Array))
				return false;
		}
		else if(In_ElemType == "float")
		{
			if(!MakeNumberArrayStrict<float>(Elems, Array))
				return false;
		}
		else if(In_ElemType == "double")
		{
			if(!MakeNumberArrayStrict<double>(Elems, Array))
				return false;
		}
		else if(In_ElemType == "bool")
		{
			std::vector<bool> Values;
			for(const auto &Elem : Elems)
			{
				bool Value = false;
				if(!ParseBoolStrict(Elem, Value))
					return false;
				Values.push_back(Value);
			}
			Array = std::move(Values);
		}
		else
			return false;

		In_Block->SetArray(In_Key, Array);
		return true;
	}

	// ---- v2 writing helpers ----

	// Appends the text with the v2 string escapes (byte by byte)
	void AppendEscaped(std::string &InOut_Out, const std::string &In_Text)
	{
		constexpr char kHex[] = "0123456789ABCDEF";
		for(const char Ch : In_Text)
		{
			const unsigned char Byte = static_cast<unsigned char>(Ch);
			if(Ch == '\\')
				InOut_Out += "\\\\";
			else if(Ch == '\n')
				InOut_Out += "\\n";
			else if(Ch == '\r')
				InOut_Out += "\\r";
			else if(Ch == ',')
				InOut_Out += "\\,";
			else if(Byte < 0x20 && Byte != 0x09)
			{
				InOut_Out += "\\x";
				InOut_Out.push_back(kHex[Byte >> 4]);
				InOut_Out.push_back(kHex[Byte & 0x0F]);
			}
			else
				InOut_Out.push_back(Ch);
		}
	}

	// Shortest text that reads back as the same number
	template<typename T>
	void AppendNumber(std::string &InOut_Out, T In_Value)
	{
		char Buf[64];
		const auto Res = std::to_chars(Buf, Buf + sizeof(Buf), In_Value);
		InOut_Out.append(Buf, Res.ptr);
	}

	// Appends one element: string (escaped), bool or number
	template<typename T>
	void AppendElement(std::string &InOut_Out, const T &In_Value)
	{
		if constexpr(std::is_same_v<T, std::string>)
			AppendEscaped(InOut_Out, In_Value);
		else if constexpr(std::is_same_v<T, bool>)
			InOut_Out += In_Value ? "true" : "false";
		else
			AppendNumber(InOut_Out, In_Value);
	}

	// Appends "[n]a,b,c"
	template<typename Vec>
	void AppendArray(std::string &InOut_Out, const Vec &In_Array)
	{
		InOut_Out += '[';
		AppendNumber(InOut_Out, In_Array.size());
		InOut_Out += ']';
		for(size_t i = 0; i < In_Array.size(); ++i)
		{
			if(i > 0)
				InOut_Out += ',';
			// Copy first: an element of std::vector<bool> is a proxy
			const typename Vec::value_type Elem = In_Array[i];
			AppendElement(InOut_Out, Elem);
		}
	}

	// "<prefix>[count]{hints}:" without the line end
	void AppendHeader(std::string &InOut_Out, const std::string &In_Prefix, size_t In_BlockCount, const std::string &In_Hints)
	{
		InOut_Out += In_Prefix;
		InOut_Out += '[';
		AppendNumber(InOut_Out, In_BlockCount);
		InOut_Out += "]{";
		InOut_Out += In_Hints;
		InOut_Out += "}:";
	}
}

struct cpon_text_helper::Lines
{
	std::vector<std::string> Data;
	size_t Pos = 0;

	bool HasLine() const noexcept { return Pos < Data.size(); }
	bool AtBlank() const noexcept { return Pos < Data.size() && Data[Pos].empty(); }
};

bool cpon_text_helper::Read(_In_ std::istream &In_Stream, _Out_ std::vector<std::shared_ptr<cpon_object>> &Out_Objects)
{
	try
	{
		Lines Input;
		std::string Line;
		while(std::getline(In_Stream, Line))
			Input.Data.push_back(std::move(Line));

		if(!Input.HasLine())
			return false;
		const std::string_view First = Input.Data[0];
		constexpr std::string_view kObjNum = "#ObjNum : ";
		const size_t NumPos = First.find(kObjNum);
		if(NumPos == std::string_view::npos)
			return false;
		if(First.find("#Version") != std::string_view::npos)
		{
			// The whole line must be "#ObjNum : <N> #Version : 2"
			constexpr std::string_view kVersion = " #Version : 2";
			if(!First.starts_with(kObjNum) || !First.ends_with(kVersion) || First.size() < kObjNum.size() + kVersion.size())
				return false;
			int Count = 0;
			if(!ParseCount(First.substr(kObjNum.size(), First.size() - kObjNum.size() - kVersion.size()), Count))
				return false;
			Input.Pos = 1;

			std::vector<std::shared_ptr<cpon_object>> Objects;
			if(!ReadV2(Input, Count, Objects))
				return false;
			Out_Objects = std::move(Objects);
			return true;
		}

		const int ObjCount = ParseNumber<int>(First.substr(NumPos + kObjNum.size()));
		Input.Pos = 1;

		std::vector<std::shared_ptr<cpon_object>> Objects;
		if(!ReadV1(Input, ObjCount, Objects))
			return false;
		Out_Objects = std::move(Objects);
		return true;
	}
	catch(...)
	{
		return false;
	}
}

bool cpon_text_helper::Write(_In_ std::ostream &In_Stream, _In_ const std::vector<std::shared_ptr<cpon_object>> &In_Objects)
{
	std::string Head = "#ObjNum : ";
	AppendNumber(Head, In_Objects.size());
	Head += " #Version : 2\n";
	In_Stream << Head;

	for(const auto &Obj : In_Objects)
	{
		if(!Obj)
			return false;
		std::string Line;
		AppendHeader(Line, Obj->m_ObjectName, Obj->m_Data.size(), Obj->m_BlockHints);
		Line += '\n';
		In_Stream << Line;
		if(!WriteBlocks(In_Stream, Obj, 1))
			return false;
		In_Stream << '\n';
	}
	return static_cast<bool>(In_Stream);
}

bool cpon_text_helper::WriteBlocks(_In_ std::ostream &In_Stream, _In_ const std::shared_ptr<cpon_object> &In_Object, _In_ int In_Depth)
{
	if(In_Depth > kMaxDepth)
		return false;
	std::vector<cpon_detail::HintEntry> Hints;
	if(!cpon_detail::ParseHints(In_Object->m_BlockHints, Hints))
		return false;

	const std::string Indent(static_cast<size_t>(In_Depth) * 2, ' ');
	for(size_t b = 0; b < In_Object->m_Data.size(); ++b)
	{
		const auto &Block = In_Object->m_Data[b];
		if(!Block)
			return false;
		for(const auto &Hint : Hints)
		{
			const auto Item = Block->m_BlockData.find(Hint.Key);
			if(Item == Block->m_BlockData.end())
				continue;
			In_Stream << Indent << Hint.Key << ':';
			if(!WriteItem(In_Stream, Item->second, In_Depth))
				return false;
		}
		if(b + 1 < In_Object->m_Data.size())
			In_Stream << '\n';
	}
	return static_cast<bool>(In_Stream);
}

bool cpon_text_helper::WriteItem(_In_ std::ostream &In_Stream, _In_ const cpon_block::DataItem &In_Item, _In_ int In_Depth)
{
	std::string Text;
	if(const auto *Value = std::get_if<cpon_block::DataValue>(&In_Item))
	{
		std::visit([&Text](const auto &InValue) { AppendElement(Text, InValue); }, *Value);
		Text += '\n';
		In_Stream << Text;
		return static_cast<bool>(In_Stream);
	}
	if(const auto *Array = std::get_if<cpon_block::Array>(&In_Item))
	{
		std::visit([&Text](const auto &InArray) { AppendArray(Text, InArray); }, *Array);
		Text += '\n';
		In_Stream << Text;
		return static_cast<bool>(In_Stream);
	}

	const auto &Nested = std::get<cpon_block::Object>(In_Item);
	if(!Nested)
		return false;
	// The name of a nested object is the key of its item, so it is not written again
	AppendHeader(Text, std::string(), Nested->m_Data.size(), Nested->m_BlockHints);
	Text += '\n';
	In_Stream << Text;
	return WriteBlocks(In_Stream, Nested, In_Depth + 1);
}

bool cpon_text_helper::ReadV2(_In_ Lines &InOut_Lines, _In_ int In_ObjCount, _Out_ std::vector<std::shared_ptr<cpon_object>> &Out_Objects)
{
	// Every object needs its own header line, so a huge count fails when the lines run out
	for(int i = 0; i < In_ObjCount; ++i)
	{
		if(!InOut_Lines.HasLine())
			return false;

		HeaderInfo Info;
		if(!ParseHeaderV2(InOut_Lines.Data[InOut_Lines.Pos], Info))
			return false;
		++InOut_Lines.Pos;

		auto Obj = std::make_shared<cpon_object>();
		Obj->SetObjectName(Info.Prefix);
		Obj->SetHints(Info.Hints);
		Out_Objects.push_back(Obj);

		// Every block but the last is followed by a blank line, so a huge count fails when the lines run out
		for(int b = 0; b < Info.BlockCount; ++b)
		{
			if(!ReadV2Block(InOut_Lines, Obj, Info.Entries, 1))
				return false;
			if(b + 1 < Info.BlockCount)
			{
				if(!InOut_Lines.AtBlank())
					return false;
				++InOut_Lines.Pos;
			}
		}

		// The blank line after the last block of a top-level object
		if(InOut_Lines.HasLine())
		{
			if(!InOut_Lines.AtBlank())
				return false;
			++InOut_Lines.Pos;
		}
	}

	while(InOut_Lines.HasLine())
	{
		if(!InOut_Lines.AtBlank())
			return false;
		++InOut_Lines.Pos;
	}
	return true;
}

bool cpon_text_helper::ReadV2Block(_In_ Lines &InOut_Lines, _In_ const std::shared_ptr<cpon_object> &In_Object, _In_ const std::vector<cpon_detail::HintEntry> &In_Hints, _In_ int In_Depth)
{
	if(In_Depth > kMaxDepth)
		return false;

	auto Block = In_Object->CreateDataBlock();
	const size_t Indent = static_cast<size_t>(In_Depth) * 2;
	std::vector<const cpon_detail::HintEntry *> Seen; // keys already read in this block
	while(InOut_Lines.HasLine() && !InOut_Lines.AtBlank())
	{
		const std::string_view Line = InOut_Lines.Data[InOut_Lines.Pos];
		size_t Spaces = Line.find_first_not_of(' ');
		if(Spaces == std::string_view::npos)
			Spaces = Line.size();
		if(Spaces < Indent)
			break; // the line belongs to the enclosing block
		if(Spaces != Indent)
			return false;

		const std::string_view Rest = Line.substr(Indent);
		const size_t Colon = Rest.find(':');
		if(Colon == std::string_view::npos)
			return false;
		const std::string_view Key = Rest.substr(0, Colon);

		const cpon_detail::HintEntry *Found = nullptr;
		for(const auto &Hint : In_Hints)
		{
			if(Hint.Key == Key)
			{
				Found = &Hint;
				break;
			}
		}
		if(!Found)
			return false;
		if(std::find(Seen.begin(), Seen.end(), Found) != Seen.end())
			return false; // duplicate key
		Seen.push_back(Found);

		// Key and Rest point into the line, which stays alive: Lines is never modified while reading
		++InOut_Lines.Pos;
		if(!ReadV2Value(InOut_Lines, Block, Key, Found->Type, Rest.substr(Colon + 1), In_Depth))
			return false;
	}
	return true;
}

bool cpon_text_helper::ReadV2Value(_In_ Lines &InOut_Lines, _In_ const std::shared_ptr<cpon_block> &In_Block, _In_ std::string_view In_Key, _In_ std::string_view In_Type, _In_ std::string_view In_Value, _In_ int In_Depth)
{
	if(In_Type == "string")
	{
		std::string Text;
		if(!Unescape(In_Value, Text))
			return false;
		In_Block->SetValue(In_Key, std::move(Text));
	}
	else if(In_Type == "int")
	{
		int Value = 0;
		if(!ParseStrict<int>(In_Value, Value))
			return false;
		In_Block->SetValue(In_Key, Value);
	}
	else if(In_Type == "uint")
	{
		unsigned int Value = 0;
		if(!ParseStrict<unsigned int>(In_Value, Value))
			return false;
		In_Block->SetValue(In_Key, Value);
	}
	else if(In_Type == "float")
	{
		float Value = 0;
		if(!ParseStrict<float>(In_Value, Value))
			return false;
		In_Block->SetValue(In_Key, Value);
	}
	else if(In_Type == "double")
	{
		double Value = 0;
		if(!ParseStrict<double>(In_Value, Value))
			return false;
		In_Block->SetValue(In_Key, Value);
	}
	else if(In_Type == "bool")
	{
		bool Value = false;
		if(!ParseBoolStrict(In_Value, Value))
			return false;
		In_Block->SetValue(In_Key, Value);
	}
	else if(In_Type.starts_with("array<") && In_Type.ends_with('>'))
		return ReadArrayV2(In_Block, In_Key, In_Type.substr(6, In_Type.size() - 7), In_Value);
	else if(In_Type == "object")
	{
		HeaderInfo Info;
		if(!ParseHeaderV2(In_Value, Info) || !Info.Prefix.empty())
			return false;
		auto Obj = In_Block->CreateObject(In_Key);
		Obj->SetHints(Info.Hints);
		for(int b = 0; b < Info.BlockCount; ++b)
		{
			if(!ReadV2Block(InOut_Lines, Obj, Info.Entries, In_Depth + 1))
				return false;
			if(b + 1 < Info.BlockCount)
			{
				if(!InOut_Lines.AtBlank())
					return false;
				++InOut_Lines.Pos;
			}
		}
	}
	else
		return false;
	return true;
}

bool cpon_text_helper::ReadV1(_In_ Lines &InOut_Lines, _In_ int In_ObjCount, _Out_ std::vector<std::shared_ptr<cpon_object>> &Out_Objects)
{
	// Every object needs its own header line, so a huge count fails when the lines run out
	for(int i = 0; i < In_ObjCount; ++i)
	{
		if(!InOut_Lines.HasLine())
			return false;

		HeaderInfo Info;
		if(!ParseHeader(InOut_Lines.Data[InOut_Lines.Pos], Info))
			return false;
		++InOut_Lines.Pos;

		auto Obj = std::make_shared<cpon_object>();
		Obj->SetObjectName(Info.Prefix);
		Obj->SetHints(Info.Hints);
		Out_Objects.push_back(Obj);

		for(int b = 0; b < Info.BlockCount; ++b)
		{
			if(!ReadV1Block(InOut_Lines, Obj, Info.Entries, 1))
				return false;
		}
	}
	return true;
}

bool cpon_text_helper::ReadV1Block(_In_ Lines &InOut_Lines, _In_ const std::shared_ptr<cpon_object> &In_Object, _In_ const std::vector<cpon_detail::HintEntry> &In_Hints, _In_ int In_Depth)
{
	if(In_Depth > kMaxDepth)
		return false;
	if(!InOut_Lines.HasLine())
		return false;
	if(InOut_Lines.AtBlank())
		++InOut_Lines.Pos;

	auto Block = In_Object->CreateDataBlock();
	size_t HintPos = 0;
	while(InOut_Lines.HasLine() && !InOut_Lines.AtBlank())
	{
		std::string_view Key, Value;
		size_t Found = In_Hints.size();
		if(SplitDataLine(InOut_Lines.Data[InOut_Lines.Pos], Key, Value))
		{
			for(size_t h = HintPos; h < In_Hints.size(); ++h)
			{
				if(In_Hints[h].Key == Key)
				{
					Found = h;
					break;
				}
			}
		}

		if(Found == In_Hints.size())
		{
			if(In_Depth == 1)
				return false;
			break; // the line belongs to the enclosing block
		}

		// Key and Value point into the line, which stays alive: Lines is never modified while reading
		++InOut_Lines.Pos;
		if(!ReadV1Value(InOut_Lines, Block, Key, In_Hints[Found].Type, Value, In_Depth))
			return false;
		HintPos = Found + 1;
	}

	if(In_Depth == 1 && InOut_Lines.AtBlank())
		++InOut_Lines.Pos;
	return true;
}

bool cpon_text_helper::ReadV1Value(_In_ Lines &InOut_Lines, _In_ const std::shared_ptr<cpon_block> &In_Block, _In_ std::string_view In_Key, _In_ std::string_view In_Type, _In_ std::string_view In_Value, _In_ int In_Depth)
{
	if(In_Type == "string")
		In_Block->SetValue(In_Key, std::string(In_Value));
	else if(In_Type == "int")
		In_Block->SetValue(In_Key, ParseNumber<int>(In_Value));
	else if(In_Type == "uint")
		In_Block->SetValue(In_Key, ParseNumber<unsigned int>(In_Value));
	else if(In_Type == "float")
		In_Block->SetValue(In_Key, ParseNumber<float>(In_Value));
	else if(In_Type == "double")
		In_Block->SetValue(In_Key, ParseNumber<double>(In_Value));
	else if(In_Type == "bool")
		In_Block->SetValue(In_Key, ParseBool(In_Value));
	else if(In_Type.starts_with("array<") && In_Type.ends_with('>'))
		return ReadArray(In_Block, In_Key, In_Type.substr(6, In_Type.size() - 7), In_Value);
	else if(In_Type == "object")
	{
		HeaderInfo Info;
		if(!ParseHeader(In_Value, Info))
			return false;
		auto Obj = In_Block->CreateObject(In_Key);
		Obj->SetHints(Info.Hints);
		for(int b = 0; b < Info.BlockCount; ++b)
		{
			const size_t Before = InOut_Lines.Pos;
			if(!ReadV1Block(InOut_Lines, Obj, Info.Entries, In_Depth + 1))
				return false;
			// A block that consumed no line leaves the state unchanged: a huge count would only repeat it
			// (and allocate a block each time), so only the last block may be empty this way
			if(InOut_Lines.Pos == Before && b + 1 < Info.BlockCount)
				return false;
		}
	}
	else
		return false;
	return true;
}
