// Internal helper: reads and writes the .cpon text format.
#include "cpon_text.hpp"
#include <charconv>
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
		// v2 is not supported yet
		if(First.find("#Version") != std::string_view::npos)
			return false;

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
