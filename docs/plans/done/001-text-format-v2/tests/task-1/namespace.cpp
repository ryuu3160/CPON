	// ---- cpon_detail::ParseHints (cpon_hints.hpp) ----
	void TestParseHints(int &InOut_Ran)
	{
		const char *name = "parse-hints";
		std::vector<cpon_detail::HintEntry> e;

		if(!cpon_detail::ParseHints("", e) || !e.empty())
			Fail(name, "empty string must succeed with 0 entries");

		if(!cpon_detail::ParseHints("Name:string,Scores:array<int>,Addr:object", e) || e.size() != 3
			|| e[0].Key != "Name" || e[0].Type != "string"
			|| e[1].Key != "Scores" || e[1].Type != "array<int>"
			|| e[2].Key != "Addr" || e[2].Type != "object")
			Fail(name, "three entries");

		// the output vector is replaced, not appended to
		if(!cpon_detail::ParseHints("A:int", e) || e.size() != 1 || e[0].Key != "A" || e[0].Type != "int")
			Fail(name, "single entry / output not replaced");

		if(cpon_detail::ParseHints("A:int,B", e))
			Fail(name, "entry without ':' accepted");
		if(cpon_detail::ParseHints(":int", e))
			Fail(name, "empty key accepted");
		if(cpon_detail::ParseHints("A:", e))
			Fail(name, "empty type accepted");
		if(cpon_detail::ParseHints("A:int,", e))
			Fail(name, "trailing comma accepted");
		if(cpon_detail::ParseHints(",A:int", e))
			Fail(name, "leading comma accepted");

		++InOut_Ran;
		std::printf("  %-28s %s\n", name, "checked");
	}

