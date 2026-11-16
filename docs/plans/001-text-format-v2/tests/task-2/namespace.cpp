	// ---- text fixtures: files with fixed content, loaded through cpon::LoadFromFile ----
	const char *const kFixturePath = "cpon_test_fixture.cpon";

	void WriteRaw(const std::string &In_Text)
	{
		std::ofstream f(kFixturePath, std::ios::out | std::ios::trunc);
		f << In_Text;
	}

	// Loads In_Text as a .cpon file. An exception counts as a failure.
	bool LoadText(const char *In_Name, const std::string &In_Text, cpon &Out_Cpon)
	{
		WriteRaw(In_Text);
		bool ok = false;
		try
		{
			ok = Out_Cpon.LoadFromFile(kFixturePath);
		}
		catch(...)
		{
			Fail(In_Name, "LoadFromFile threw");
		}
		std::remove(kFixturePath);
		return ok;
	}

	// In_Text must be rejected: LoadFromFile returns false, throws nothing and keeps the data it had
	void ExpectRejected(const char *In_Name, const std::string &In_Text)
	{
		cpon c;
		c.CreateObject("Keep")->CreateDataBlock()->SetValue("V", 1);
		WriteRaw(In_Text);
		bool threw = false;
		bool loaded = false;
		try
		{
			loaded = c.LoadFromFile(kFixturePath);
		}
		catch(...)
		{
			threw = true;
		}
		std::remove(kFixturePath);
		if(threw)
			Fail(In_Name, "LoadFromFile threw");
		else if(loaded)
			Fail(In_Name, "malformed text was accepted");
		else if(c.GetObjectCount() != 1 || !c.GetObjectPtr("Keep") || !ChkVal<int>(*c["Keep"][0], "V", 1))
			Fail(In_Name, "a failed load changed the data");
	}

	void Report(const char *In_Name, int In_FailuresBefore, int &InOut_Ran)
	{
		++InOut_Ran;
		std::printf("  %-28s %s\n", In_Name, g_Failures == In_FailuresBefore ? "ok" : "FAILED");
		std::fflush(stdout);
	}

	// ---- v1: files without "#Version", as written by CPON 1.4.x and earlier ----
	void TestV1Fixtures(int &InOut_Ran)
	{
		// Same shape as CPON/output.cpon: nested objects, and a last block without its trailing keys
		{
			const char *name = "v1-sample";
			const int before = g_Failures;
			cpon c;
			const bool ok = LoadText(name,
				"#ObjNum : 1\n"
				"Person[2]{Scores:array<int>,Address:object,Name:string,Age:int,Test:object}:\n"
				"  Scores:[4]85, 90, 95, 100\n"
				"  Address:[1]{City:string}:\n"
				"    City:New York\n"
				"  Name:Alice\n"
				"  Age:30\n"
				"  Test:[1]{Value:int}:\n"
				"    Value:12345\n"
				"\n"
				"  Scores:[3]80, 88, 92\n"
				"  Name:Bob\n"
				"  Age:25\n"
				"\n", c);
			if(!ok || c.GetObjectCount() != 1 || c["Person"].GetDataCount() != 2)
				Fail(name, "load / counts");
			else
			{
				auto b0 = c["Person"][0];
				auto b1 = c["Person"][1];
				auto addr = b0->GetObject("Address");
				auto test = b0->GetObject("Test");
				if(!ChkArr<int>(*b0, "Scores", {85, 90, 95, 100}) || !ChkVal<std::string>(*b0, "Name", "Alice") || !ChkVal<int>(*b0, "Age", 30)
					|| !addr || addr->GetDataCount() != 1 || !ChkVal<std::string>(*(*addr)[0], "City", "New York")
					|| !test || test->GetDataCount() != 1 || !ChkVal<int>(*(*test)[0], "Value", 12345))
					Fail(name, "block 0 values");
				if(!ChkArr<int>(*b1, "Scores", {80, 88, 92}) || !ChkVal<std::string>(*b1, "Name", "Bob") || !ChkVal<int>(*b1, "Age", 25)
					|| b1->GetValuePtr<int>("Value") != nullptr)
					Fail(name, "block 1 values");
			}
			Report(name, before, InOut_Ran);
		}

		// A nested object with two blocks: 1.4.x wrote a blank line between them but could not read it back
		{
			const char *name = "v1-nested-two-blocks";
			const int before = g_Failures;
			cpon c;
			const bool ok = LoadText(name,
				"#ObjNum : 1\n"
				"O[1]{N:object,After:int}:\n"
				"  N:[2]{X:int,Y:int}:\n"
				"    X:1\n"
				"    Y:2\n"
				"\n"
				"    X:3\n"
				"    Y:4\n"
				"  After:9\n"
				"\n", c);
			auto n = ok ? c["O"][0]->GetObject("N") : nullptr;
			if(!n || n->GetDataCount() != 2
				|| !ChkVal<int>(*(*n)[0], "X", 1) || !ChkVal<int>(*(*n)[0], "Y", 2)
				|| !ChkVal<int>(*(*n)[1], "X", 3) || !ChkVal<int>(*(*n)[1], "Y", 4)
				|| !ChkVal<int>(*c["O"][0], "After", 9))
				Fail(name, "values");
			Report(name, before, InOut_Ran);
		}

		// A block that is not the last one lacks its trailing key
		{
			const char *name = "v1-trailing-key-missing";
			const int before = g_Failures;
			cpon c;
			const bool ok = LoadText(name,
				"#ObjNum : 2\n"
				"O[3]{A:int,B:int}:\n"
				"  A:1\n"
				"\n"
				"  A:2\n"
				"  B:3\n"
				"\n"
				"  A:4\n"
				"  B:5\n"
				"\n"
				"P[1]{V:int}:\n"
				"  V:7\n"
				"\n", c);
			if(!ok || c.GetObjectCount() != 2 || c["O"].GetDataCount() != 3)
				Fail(name, "load / counts");
			else if(!ChkVal<int>(*c["O"][0], "A", 1) || c["O"][0]->GetValuePtr<int>("B") != nullptr
				|| !ChkVal<int>(*c["O"][1], "A", 2) || !ChkVal<int>(*c["O"][1], "B", 3)
				|| !ChkVal<int>(*c["O"][2], "A", 4) || !ChkVal<int>(*c["O"][2], "B", 5)
				|| !ChkVal<int>(*c["P"][0], "V", 7))
				Fail(name, "values");
			Report(name, before, InOut_Ran);
		}

		// A nested block lacks its trailing key; the next line belongs to the outer block
		{
			const char *name = "v1-nested-trailing-missing";
			const int before = g_Failures;
			cpon c;
			const bool ok = LoadText(name,
				"#ObjNum : 1\n"
				"O[1]{N:object,After:int}:\n"
				"  N:[1]{X:int,Y:int}:\n"
				"    X:1\n"
				"  After:9\n"
				"\n", c);
			auto n = ok ? c["O"][0]->GetObject("N") : nullptr;
			if(!n || n->GetDataCount() != 1 || !ChkVal<int>(*(*n)[0], "X", 1) || (*n)[0]->GetValuePtr<int>("Y") != nullptr
				|| !ChkVal<int>(*c["O"][0], "After", 9))
				Fail(name, "values");
			Report(name, before, InOut_Ran);
		}

		// 1.4.x wrote nested blocks made through cpon_object::operator[] with the outer indentation:
		// v1 must not depend on indentation
		{
			const char *name = "v1-nested-shallow-indent";
			const int before = g_Failures;
			cpon c;
			const bool ok = LoadText(name,
				"#ObjNum : 1\n"
				"O[1]{N:object,After:int}:\n"
				"  N:[1]{X:int}:\n"
				"  X:1\n"
				"  After:9\n"
				"\n", c);
			auto n = ok ? c["O"][0]->GetObject("N") : nullptr;
			if(!n || n->GetDataCount() != 1 || !ChkVal<int>(*(*n)[0], "X", 1) || !ChkVal<int>(*c["O"][0], "After", 9))
				Fail(name, "values");
			Report(name, before, InOut_Ran);
		}

		// v1 has no escapes (a backslash is a backslash), reads unparsable numbers as 0,
		// and separates array elements with ", "
		{
			const char *name = "v1-values-as-before";
			const int before = g_Failures;
			cpon c;
			const bool ok = LoadText(name,
				"#ObjNum : 1\n"
				"O[1]{Path:string,Bad:int,U:uint,F:float,D:double,B:bool,AS:array<string>,AB:array<bool>,E:array<int>,Empty:string}:\n"
				"  Path:C:\\new\\table\n"
				"  Bad:abc\n"
				"  U:4000000000\n"
				"  F:1.5\n"
				"  D:-0.25\n"
				"  B:true\n"
				"  AS:[3]a, , ccc\n"
				"  AB:[2]true, false\n"
				"  E:[0]\n"
				"  Empty:\n"
				"\n", c);
			if(!ok)
				Fail(name, "load");
			else
			{
				auto b = c["O"][0];
				if(!ChkVal<std::string>(*b, "Path", "C:\\new\\table") || !ChkVal<int>(*b, "Bad", 0) || !ChkVal<unsigned int>(*b, "U", 4000000000u)
					|| !ChkVal<float>(*b, "F", 1.5f) || !ChkVal<double>(*b, "D", -0.25) || !ChkVal<bool>(*b, "B", true)
					|| !ChkArr<std::string>(*b, "AS", {"a", "", "ccc"}) || !ChkArr<bool>(*b, "AB", {true, false})
					|| !ChkArr<int>(*b, "E", {}) || !ChkVal<std::string>(*b, "Empty", ""))
					Fail(name, "values");
			}
			Report(name, before, InOut_Ran);
		}

		// No blank line at the end of the file
		{
			const char *name = "v1-no-trailing-blank";
			const int before = g_Failures;
			cpon c;
			const bool ok = LoadText(name,
				"#ObjNum : 1\n"
				"O[1]{A:int,B:int}:\n"
				"  A:1", c);
			if(!ok || c["O"].GetDataCount() != 1 || !ChkVal<int>(*c["O"][0], "A", 1))
				Fail(name, "values");
			Report(name, before, InOut_Ran);
		}

		// An object count of 0, and an object without blocks
		{
			const char *name = "v1-empty";
			const int before = g_Failures;
			cpon c0;
			if(!LoadText(name, "#ObjNum : 0\n", c0) || c0.GetObjectCount() != 0)
				Fail(name, "0 objects");
			cpon c1;
			if(!LoadText(name, "#ObjNum : 1\nEmpty[0]{}:\n\n", c1) || c1.GetObjectCount() != 1 || c1["Empty"].GetDataCount() != 0)
				Fail(name, "object without blocks");
			Report(name, before, InOut_Ran);
		}
	}

	void TestV1Rejected(int &InOut_Ran)
	{
		const int before = g_Failures;

		ExpectRejected("v1-rej-empty-file", "");
		ExpectRejected("v1-rej-no-header", "hello\n");
		ExpectRejected("v1-rej-no-bracket", "#ObjNum : 1\nbroken}:\n");
		ExpectRejected("v1-rej-no-close-bracket", "#ObjNum : 1\nO[1{A:int}:\n  A:1\n\n");
		ExpectRejected("v1-rej-no-brace", "#ObjNum : 1\nO[1]A:int}:\n  A:1\n\n");
		ExpectRejected("v1-rej-no-header-end", "#ObjNum : 1\nO[1]{A:int}\n  A:1\n\n");
		ExpectRejected("v1-rej-bad-hints", "#ObjNum : 1\nO[1]{A}:\n  A:1\n\n");
		ExpectRejected("v1-rej-missing-object", "#ObjNum : 2\nO[1]{A:int}:\n  A:1\n\n");
		ExpectRejected("v1-rej-unknown-key", "#ObjNum : 1\nO[1]{A:int}:\n  Z:1\n\n");
		ExpectRejected("v1-rej-line-without-colon", "#ObjNum : 1\nO[1]{A:int}:\n  A\n\n");
		ExpectRejected("v1-rej-array-no-close", "#ObjNum : 1\nO[1]{A:array<int>}:\n  A:[2 1, 2\n\n");
		ExpectRejected("v1-rej-array-no-open", "#ObjNum : 1\nO[1]{A:array<int>}:\n  A:1, 2\n\n");
		ExpectRejected("v1-rej-nested-bad-header", "#ObjNum : 1\nO[1]{N:object}:\n  N:[1]{X:int}\n    X:1\n\n");
		// block counts far beyond the content must fail quickly instead of allocating
		ExpectRejected("v1-rej-huge-block-count", "#ObjNum : 1\nO[1000000000]{A:int}:\n  A:1\n\n");
		ExpectRejected("v1-rej-huge-object-count", "#ObjNum : 1000000000\nO[1]{A:int}:\n  A:1\n\n");
		// "#Version" other than 2 is not v1 either
		ExpectRejected("v1-rej-version-3", "#ObjNum : 1 #Version : 3\nO[0]{}:\n\n");

		// nesting deeper than 64 must fail instead of overflowing the stack
		{
			std::string deep = "#ObjNum : 1\nO[1]{N:object}:\n";
			for(int i = 0; i < 1000; ++i)
				deep += "  N:[1]{N:object}:\n";
			ExpectRejected("v1-rej-too-deep", deep);
		}

		Report("v1-rejected", before, InOut_Ran);
	}

