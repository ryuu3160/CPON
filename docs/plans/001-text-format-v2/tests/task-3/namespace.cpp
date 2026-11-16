	// ---- v2: structures and strings the text format must carry (run through memory, binary file and text file) ----
	std::vector<Case> MakeTextV2Cases()
	{
		std::vector<Case> cases;

		// Scalar strings with characters that need escaping, and characters that must be kept as they are
		{
			const std::vector<std::string> values = {
				"a\nb", "a\r\nb", "\n", "back\\slash", "C:\\new\\table", "ends-with-backslash\\", "\\",
				"a,b", ",", "", " leading", "trailing ", "  ", "x:y", "[2]{a}:", "true", "tab\there",
				std::string("nul\0inside", 10), "\x1a" "ctrl-z", "\x01\x1f\x7f", "\\n is not a newline", "\\x41"
			};
			cases.push_back(Case{ "v2-string-escapes",
				[values](cpon &c)
				{
					auto b = c.CreateObject("T")->CreateDataBlock();
					for(size_t i = 0; i < values.size(); ++i)
						b->SetValue("S" + std::to_string(i), values[i]);
				},
				[values](cpon &c)
				{
					for(size_t i = 0; i < values.size(); ++i)
					{
						if(!ChkVal<std::string>(*c["T"][0], ("S" + std::to_string(i)).c_str(), values[i]))
							return false;
					}
					return true;
				} });
		}

		// String arrays: commas, empty elements, spaces at the ends, backslashes next to separators
		cases.push_back(Case{ "v2-string-arrays",
			[](cpon &c)
			{
				auto b = c.CreateObject("T")->CreateDataBlock();
				b->SetArray("Comma", std::vector<std::string>{"a,b", "c"});
				b->SetArray("OneEmpty", std::vector<std::string>{""});
				b->SetArray("TwoEmpty", std::vector<std::string>{"", ""});
				b->SetArray("Spaces", std::vector<std::string>{" a", "b ", " "});
				b->SetArray("Backslash", std::vector<std::string>{"a\\", ",b", "\\,", "line\nbreak"});
				b->SetArray("None", std::vector<std::string>{});
				b->SetArray("Bools", std::vector<bool>{true, false});
				b->SetArray("NoInts", std::vector<int>{});
			},
			[](cpon &c)
			{
				auto b = c["T"][0];
				return ChkArr<std::string>(*b, "Comma", {"a,b", "c"})
					&& ChkArr<std::string>(*b, "OneEmpty", {""})
					&& ChkArr<std::string>(*b, "TwoEmpty", {"", ""})
					&& ChkArr<std::string>(*b, "Spaces", {" a", "b ", " "})
					&& ChkArr<std::string>(*b, "Backslash", {"a\\", ",b", "\\,", "line\nbreak"})
					&& ChkArr<std::string>(*b, "None", {})
					&& ChkArr<bool>(*b, "Bools", {true, false})
					&& ChkArr<int>(*b, "NoInts", {});
			} });

		// A nested object with two blocks, followed by another key of the outer block
		cases.push_back(Case{ "v2-nested-two-blocks",
			[](cpon &c)
			{
				auto b = c.CreateObject("O")->CreateDataBlock();
				auto n = b->CreateObject("N");
				auto n0 = n->CreateDataBlock();
				n0->SetValue("X", 1);
				n0->SetValue("Y", 2);
				auto n1 = n->CreateDataBlock();
				n1->SetValue("X", 3);
				n1->SetValue("Y", 4);
				b->SetValue("After", 9);
			},
			[](cpon &c)
			{
				auto n = c["O"][0]->GetObject("N");
				return n && n->GetDataCount() == 2
					&& ChkVal<int>(*(*n)[0], "X", 1) && ChkVal<int>(*(*n)[0], "Y", 2)
					&& ChkVal<int>(*(*n)[1], "X", 3) && ChkVal<int>(*(*n)[1], "Y", 4)
					&& ChkVal<int>(*c["O"][0], "After", 9);
			} });

		// Blocks that lack trailing keys, first keys, or every key
		cases.push_back(Case{ "v2-missing-keys",
			[](cpon &c)
			{
				auto o = c.CreateObject("O");
				auto b0 = o->CreateDataBlock();
				b0->SetValue("A", 1);
				b0->SetValue("B", 2);
				b0->SetValue("C", 3);
				o->CreateDataBlock()->SetValue("A", 4);   // trailing keys missing, not the last block
				o->CreateDataBlock();                      // no keys at all
				o->CreateDataBlock()->SetValue("C", 5);   // leading keys missing
				o->CreateDataBlock();                      // last block without keys
				c.CreateObject("P")->CreateDataBlock()->SetValue("V", 7);
			},
			[](cpon &c)
			{
				if(c.GetObjectCount() != 2 || c["O"].GetDataCount() != 5)
					return false;
				auto &o = c["O"];
				return ChkVal<int>(*o[0], "A", 1) && ChkVal<int>(*o[0], "B", 2) && ChkVal<int>(*o[0], "C", 3)
					&& ChkVal<int>(*o[1], "A", 4) && o[1]->GetValuePtr<int>("B") == nullptr && o[1]->GetValuePtr<int>("C") == nullptr
					&& o[2]->IsEmpty()
					&& ChkVal<int>(*o[3], "C", 5) && o[3]->GetValuePtr<int>("A") == nullptr
					&& o[4]->IsEmpty()
					&& ChkVal<int>(*c["P"][0], "V", 7);
			} });

		// The nested block lacks "Y" and the outer block has its own "Y" right after it:
		// only indentation tells them apart
		cases.push_back(Case{ "v2-nested-same-key-outside",
			[](cpon &c)
			{
				auto o = c.CreateObject("O");
				auto b0 = o->CreateDataBlock();
				auto n = b0->CreateObject("N");
				auto n0 = n->CreateDataBlock();
				n0->SetValue("X", 1);
				n0->SetValue("Y", 2);
				n->CreateDataBlock()->SetValue("X", 3);   // last nested block: "Y" missing
				b0->SetValue("Y", 7);
				// second outer block: the nested object has blocks without keys
				auto b1 = o->CreateDataBlock();
				auto m = b1->CreateObject("N");
				m->CreateDataBlock();
				m->CreateDataBlock();
				b1->SetValue("Y", 8);
			},
			[](cpon &c)
			{
				if(c["O"].GetDataCount() != 2)
					return false;
				auto n = c["O"][0]->GetObject("N");
				auto m = c["O"][1]->GetObject("N");
				return n && n->GetDataCount() == 2
					&& ChkVal<int>(*(*n)[0], "X", 1) && ChkVal<int>(*(*n)[0], "Y", 2)
					&& ChkVal<int>(*(*n)[1], "X", 3) && (*n)[1]->GetValuePtr<int>("Y") == nullptr
					&& ChkVal<int>(*c["O"][0], "Y", 7)
					&& m && m->GetDataCount() == 2 && (*m)[0]->IsEmpty() && (*m)[1]->IsEmpty()
					&& ChkVal<int>(*c["O"][1], "Y", 8);
			} });

		// Objects whose stored nesting level differs from their real depth:
		// added with AddObject after being built, or filled through operator[]
		cases.push_back(Case{ "v2-depth-from-structure",
			[](cpon &c)
			{
				auto inner = std::make_shared<cpon_object>();
				inner->SetObjectName("Inner");
				auto ib = inner->CreateDataBlock();
				ib->SetValue("V", 1);
				ib->CreateObject("Deep")->CreateDataBlock()->SetValue("W", 2);

				auto b = c.CreateObject("O")->CreateDataBlock();
				b->AddObject(inner);
				(*b->CreateObject("ViaIndex"))[0]->SetValue("X", 3);
				b->SetValue("After", 9);
			},
			[](cpon &c)
			{
				auto b = c["O"][0];
				auto inner = b->GetObject("Inner");
				if(!inner || inner->GetDataCount() != 1 || !ChkVal<int>(*(*inner)[0], "V", 1))
					return false;
				auto deep = (*inner)[0]->GetObject("Deep");
				auto viaIndex = b->GetObject("ViaIndex");
				return deep && deep->GetDataCount() == 1 && ChkVal<int>(*(*deep)[0], "W", 2)
					&& viaIndex && viaIndex->GetDataCount() == 1 && ChkVal<int>(*(*viaIndex)[0], "X", 3)
					&& ChkVal<int>(*b, "After", 9);
			} });

		// An object with an empty name, and several objects without blocks
		cases.push_back(Case{ "v2-empty-name-and-objects",
			[](cpon &c)
			{
				c.CreateObject("");
				c.CreateObject("A");
				c.CreateObject("B")->CreateDataBlock()->SetValue("V", 1);
			},
			[](cpon &c)
			{
				return c.GetObjectCount() == 3 && c[0].GetObjectName().empty() && c[0].GetDataCount() == 0
					&& c["A"].GetDataCount() == 0 && ChkVal<int>(*c["B"][0], "V", 1);
			} });

		return cases;
	}

	std::string ReadWholeTextFile(const char *In_Path)
	{
		std::ifstream f(In_Path, std::ios::in);
		std::ostringstream ss;
		ss << f.rdbuf();
		return ss.str();
	}

	// The exact text WriteToFile produces (pins the v2 format)
	void TestV2ExactOutput(int &InOut_Ran)
	{
		const char *name = "v2-exact-output";
		const int before = g_Failures;
		const char *path = "cpon_test_exact.cpon";

		cpon c;
		auto o = c.CreateObject("Person");
		auto b0 = o->CreateDataBlock();
		b0->SetArray("Scores", std::vector<int>{85, 90, 95});
		auto addr = b0->CreateObject("Address");
		addr->CreateDataBlock()->SetValue("City", std::string("New York"));
		addr->CreateDataBlock()->SetValue("City", std::string("Osaka"));
		b0->SetValue("Name", std::string("Alice\nSmith"));
		b0->SetValue("Ratio", 0.5);
		b0->SetValue("Ok", true);
		auto b1 = o->CreateDataBlock();
		b1->SetValue("Name", std::string("Bob\x01"));
		b1->SetArray("Tags", std::vector<std::string>{"a,b", "", " c"});
		c.CreateObject("Empty");

		const std::string expected =
			"#ObjNum : 2 #Version : 2\n"
			"Person[2]{Scores:array<int>,Address:object,Name:string,Ratio:double,Ok:bool,Tags:array<string>}:\n"
			"  Scores:[3]85,90,95\n"
			"  Address:[2]{City:string}:\n"
			"    City:New York\n"
			"\n"
			"    City:Osaka\n"
			"  Name:Alice\\nSmith\n"
			"  Ratio:0.5\n"
			"  Ok:true\n"
			"\n"
			"  Name:Bob\\x01\n"
			"  Tags:[3]a\\,b,, c\n"
			"\n"
			"Empty[0]{}:\n"
			"\n";

		if(!c.WriteToFile(path))
			Fail(name, "WriteToFile failed");
		else if(ReadWholeTextFile(path) != expected)
			Fail(name, "file content differs from the v2 format");
		std::remove(path);
		Report(name, before, InOut_Ran);
	}

	// Hand-written v2 text: what the reader must accept beyond the writer's own output
	void TestV2Fixtures(int &InOut_Ran)
	{
		// lowercase hex, an unescaped comma in a scalar string, no blank line at the end of the file
		{
			const char *name = "v2-fixture-lenient";
			const int before = g_Failures;
			cpon c;
			const bool ok = LoadText(name,
				"#ObjNum : 1 #Version : 2\n"
				"O[2]{S:string,T:string,A:array<string>}:\n"
				"  S:a\\x1ab\n"
				"  T:x,y\n"
				"\n"
				"  A:[2]p\\\\,q", c);
			if(!ok || c["O"].GetDataCount() != 2
				|| !ChkVal<std::string>(*c["O"][0], "S", "a\x1a" "b") || !ChkVal<std::string>(*c["O"][0], "T", "x,y")
				|| !ChkArr<std::string>(*c["O"][1], "A", {"p\\", "q"}))
				Fail(name, "values");
			Report(name, before, InOut_Ran);
		}

		// keys in a different order than the hints, and extra blank lines after the last object
		{
			const char *name = "v2-fixture-key-order";
			const int before = g_Failures;
			cpon c;
			const bool ok = LoadText(name,
				"#ObjNum : 1 #Version : 2\n"
				"O[1]{A:int,B:int}:\n"
				"  B:2\n"
				"  A:1\n"
				"\n"
				"\n"
				"\n", c);
			if(!ok || !ChkVal<int>(*c["O"][0], "A", 1) || !ChkVal<int>(*c["O"][0], "B", 2))
				Fail(name, "values");
			Report(name, before, InOut_Ran);
		}

		// nesting exactly 64 deep is accepted (the top-level object's blocks are depth 1)
		{
			const char *name = "v2-fixture-depth-64";
			const int before = g_Failures;
			std::string text = "#ObjNum : 1 #Version : 2\nO[1]{N:object}:\n";
			for(int d = 1; d < 64; ++d)
				text += std::string(static_cast<size_t>(2 * d), ' ') + (d < 63 ? "N:[1]{N:object}:\n" : "N:[1]{V:int}:\n");
			text += std::string(2 * 64, ' ') + "V:5\n\n";
			cpon c;
			if(!LoadText(name, text, c))
				Fail(name, "depth 64 rejected");
			else
			{
				auto block = c["O"][0];
				for(int d = 1; d < 64 && block; ++d)
				{
					auto n = block->GetObject("N");
					block = (n && n->GetDataCount() == 1) ? (*n)[0] : nullptr;
				}
				if(!block || !ChkVal<int>(*block, "V", 5))
					Fail(name, "value at depth 64");
			}
			Report(name, before, InOut_Ran);
		}
	}

