// Internal helper: splits a block-hints string into key/type entries.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cpon_detail
{
	struct HintEntry
	{
		std::string Key;
		std::string Type;
	};

	// "Name:string,Scores:array<int>" -> {{"Name","string"},{"Scores","array<int>"}}
	// Out_Entries is replaced. Returns false when an entry has no ':', an empty key or an empty type
	// (this includes a leading or trailing ','). An empty string succeeds with 0 entries.
	[[nodiscard]] inline bool ParseHints(std::string_view In_Hints, std::vector<HintEntry> &Out_Entries)
	{
		Out_Entries.clear();
		if(In_Hints.empty())
			return true;

		size_t pos = 0;
		while(true)
		{
			const size_t comma = In_Hints.find(',', pos);
			const std::string_view entry = In_Hints.substr(pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);

			const size_t colon = entry.find(':');
			if(colon == std::string_view::npos || colon == 0 || colon + 1 == entry.size())
			{
				Out_Entries.clear();
				return false;
			}
			Out_Entries.push_back(HintEntry{ std::string(entry.substr(0, colon)), std::string(entry.substr(colon + 1)) });

			if(comma == std::string_view::npos)
				break;
			pos = comma + 1;
		}
		return true;
	}
}
