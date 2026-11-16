// Internal helper: reads and writes the .cpon text format. Not part of the public API.
#pragma once

#include "cpon_object.hpp"
#include "cpon_hints.hpp"
#include <istream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class cpon_text_helper
{
public:
	// Reads the whole stream. On success replaces Out_Objects and returns true.
	// On failure returns false and leaves Out_Objects untouched. Never throws.
	[[nodiscard]] static bool Read(_In_ std::istream &In_Stream, _Out_ std::vector<std::shared_ptr<cpon_object>> &Out_Objects);

private:
	// All lines of the file with a read position
	struct Lines;

	// v1 (files without "#Version"): objects start on the line at the read position
	[[nodiscard]] static bool ReadV1(_In_ Lines &InOut_Lines, _In_ int In_ObjCount, _Out_ std::vector<std::shared_ptr<cpon_object>> &Out_Objects);
	// One block of In_Object; In_Hints are the parsed block hints of In_Object
	[[nodiscard]] static bool ReadV1Block(_In_ Lines &InOut_Lines, _In_ const std::shared_ptr<cpon_object> &In_Object, _In_ const std::vector<cpon_detail::HintEntry> &In_Hints, _In_ int In_Depth);
	// The value after "Key:" of a data line; the line itself was consumed already
	[[nodiscard]] static bool ReadV1Value(_In_ Lines &InOut_Lines, _In_ const std::shared_ptr<cpon_block> &In_Block, _In_ std::string_view In_Key, _In_ std::string_view In_Type, _In_ std::string_view In_Value, _In_ int In_Depth);
};
