// Internal helper: reads and writes the .cpon text format. Not part of the public API.
#pragma once

#include "cpon_object.hpp"
#include "cpon_hints.hpp"
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

class cpon_text_helper
{
public:
	// Writes the v2 text format. Returns false, possibly after writing part of the output, when:
	// an object pointer is null, an object's hints cannot be parsed, nesting is deeper than kMaxDepth,
	// or the stream fails.
	[[nodiscard]] static bool Write(_In_ std::ostream &In_Stream, _In_ const std::vector<std::shared_ptr<cpon_object>> &In_Objects);

	// Reads the whole stream. On success replaces Out_Objects and returns true.
	// On failure returns false and leaves Out_Objects untouched. Never throws.
	[[nodiscard]] static bool Read(_In_ std::istream &In_Stream, _Out_ std::vector<std::shared_ptr<cpon_object>> &Out_Objects);

private:
	// Blocks of In_Object, each written at nesting depth In_Depth (the blocks of a top-level object are depth 1)
	[[nodiscard]] static bool WriteBlocks(_In_ std::ostream &In_Stream, _In_ const std::shared_ptr<cpon_object> &In_Object, _In_ int In_Depth);
	// Value of one data item after "Key:" including the line end (a nested object also writes its blocks)
	[[nodiscard]] static bool WriteItem(_In_ std::ostream &In_Stream, _In_ const cpon_block::DataItem &In_Item, _In_ int In_Depth);

	// All lines of the file with a read position
	struct Lines;

	// v2: objects start on the line at the read position
	[[nodiscard]] static bool ReadV2(_In_ Lines &InOut_Lines, _In_ int In_ObjCount, _Out_ std::vector<std::shared_ptr<cpon_object>> &Out_Objects);
	// One block of In_Object at depth In_Depth; the line after the block is not consumed
	[[nodiscard]] static bool ReadV2Block(_In_ Lines &InOut_Lines, _In_ const std::shared_ptr<cpon_object> &In_Object, _In_ const std::vector<cpon_detail::HintEntry> &In_Hints, _In_ int In_Depth);
	// The value after "Key:" of a data line; the line itself was consumed already
	[[nodiscard]] static bool ReadV2Value(_In_ Lines &InOut_Lines, _In_ const std::shared_ptr<cpon_block> &In_Block, _In_ std::string_view In_Key, _In_ std::string_view In_Type, _In_ std::string_view In_Value, _In_ int In_Depth);

	// v1 (files without "#Version"): objects start on the line at the read position
	[[nodiscard]] static bool ReadV1(_In_ Lines &InOut_Lines, _In_ int In_ObjCount, _Out_ std::vector<std::shared_ptr<cpon_object>> &Out_Objects);
	// One block of In_Object; In_Hints are the parsed block hints of In_Object
	[[nodiscard]] static bool ReadV1Block(_In_ Lines &InOut_Lines, _In_ const std::shared_ptr<cpon_object> &In_Object, _In_ const std::vector<cpon_detail::HintEntry> &In_Hints, _In_ int In_Depth);
	// The value after "Key:" of a data line; the line itself was consumed already
	[[nodiscard]] static bool ReadV1Value(_In_ Lines &InOut_Lines, _In_ const std::shared_ptr<cpon_block> &In_Block, _In_ std::string_view In_Key, _In_ std::string_view In_Type, _In_ std::string_view In_Value, _In_ int In_Depth);
};
