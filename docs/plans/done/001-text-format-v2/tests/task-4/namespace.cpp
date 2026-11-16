	// ---- v2: malformed text must be rejected (false, no exception, data kept) ----
	void TestV2Rejected(int &InOut_Ran)
	{
		const int before = g_Failures;
		const std::string H = "#ObjNum : 1 #Version : 2\n";

		// header line
		ExpectRejected("v2-rej-version-3", "#ObjNum : 1 #Version : 3\nO[0]{}:\n\n");
		ExpectRejected("v2-rej-version-garbage", "#ObjNum : 1 #Version : 2x\nO[0]{}:\n\n");
		ExpectRejected("v2-rej-version-missing-number", "#ObjNum : 1 #Version : \nO[0]{}:\n\n");
		ExpectRejected("v2-rej-objnum-negative", "#ObjNum : -1 #Version : 2\n");
		ExpectRejected("v2-rej-objnum-not-number", "#ObjNum : x #Version : 2\n");
		ExpectRejected("v2-rej-fewer-objects", "#ObjNum : 2 #Version : 2\nO[0]{}:\n\n");
		ExpectRejected("v2-rej-extra-content", H + "O[0]{}:\n\ngarbage\n");
		ExpectRejected("v2-rej-more-objects", H + "O[0]{}:\n\nP[0]{}:\n\n");
		ExpectRejected("v2-rej-no-blank-between-objects", "#ObjNum : 2 #Version : 2\nO[1]{A:int}:\n  A:1\nP[0]{}:\n\n");

		// object header
		ExpectRejected("v2-rej-header-no-end", H + "O[1]{A:int}\n  A:1\n\n");
		ExpectRejected("v2-rej-header-text-after-end", H + "O[1]{A:int}:x\n  A:1\n\n");
		ExpectRejected("v2-rej-header-no-close-bracket", H + "O[1{A:int}:\n  A:1\n\n");
		ExpectRejected("v2-rej-header-no-brace", H + "O[1]A:int}:\n  A:1\n\n");
		ExpectRejected("v2-rej-block-count-negative", H + "O[-1]{}:\n\n");
		ExpectRejected("v2-rej-block-count-not-number", H + "O[x]{}:\n\n");
		ExpectRejected("v2-rej-block-count-huge", H + "O[1000000000]{A:int}:\n  A:1\n\n");
		ExpectRejected("v2-rej-hints-no-colon", H + "O[1]{A}:\n  A:1\n\n");
		ExpectRejected("v2-rej-type-unknown", H + "O[1]{A:foo}:\n  A:1\n\n");
		ExpectRejected("v2-rej-array-type-unknown", H + "O[1]{A:array<foo>}:\n  A:[1]1\n\n");

		// block lines
		ExpectRejected("v2-rej-indent-deeper", H + "O[1]{A:int}:\n    A:1\n\n");
		ExpectRejected("v2-rej-indent-odd", H + "O[1]{A:int}:\n   A:1\n\n");
		ExpectRejected("v2-rej-indent-shallow", H + "O[1]{A:int}:\n A:1\n\n");
		ExpectRejected("v2-rej-indent-tab", H + "O[1]{A:int}:\n\tA:1\n\n");
		ExpectRejected("v2-rej-unknown-key", H + "O[1]{A:int}:\n  Z:1\n\n");
		ExpectRejected("v2-rej-duplicate-key", H + "O[1]{A:int}:\n  A:1\n  A:2\n\n");
		ExpectRejected("v2-rej-line-without-colon", H + "O[1]{A:int}:\n  A\n\n");
		ExpectRejected("v2-rej-too-many-blocks", H + "O[1]{A:int}:\n  A:1\n\n  A:2\n\n");
		ExpectRejected("v2-rej-nested-separator-missing", H + "O[1]{N:object,After:int}:\n  N:[2]{X:int}:\n    X:1\n  After:9\n\n");
		ExpectRejected("v2-rej-nested-bad-header", H + "O[1]{N:object}:\n  N:[1]{X:int}\n    X:1\n\n");
		ExpectRejected("v2-rej-nested-not-header", H + "O[1]{N:object}:\n  N:5\n\n");

		// numbers and bool
		ExpectRejected("v2-rej-int-trailing", H + "O[1]{A:int}:\n  A:12x\n\n");
		ExpectRejected("v2-rej-int-empty", H + "O[1]{A:int}:\n  A:\n\n");
		ExpectRejected("v2-rej-int-leading-space", H + "O[1]{A:int}:\n  A: 1\n\n");
		ExpectRejected("v2-rej-int-overflow", H + "O[1]{A:int}:\n  A:99999999999\n\n");
		ExpectRejected("v2-rej-int-fraction", H + "O[1]{A:int}:\n  A:1.5\n\n");
		ExpectRejected("v2-rej-uint-negative", H + "O[1]{U:uint}:\n  U:-1\n\n");
		ExpectRejected("v2-rej-float-trailing", H + "O[1]{F:float}:\n  F:1.5abc\n\n");
		ExpectRejected("v2-rej-double-empty", H + "O[1]{D:double}:\n  D:\n\n");
		ExpectRejected("v2-rej-bool-word", H + "O[1]{B:bool}:\n  B:yes\n\n");
		ExpectRejected("v2-rej-bool-case", H + "O[1]{B:bool}:\n  B:True\n\n");
		ExpectRejected("v2-rej-bool-contains", H + "O[1]{B:bool}:\n  B:untrue\n\n");

		// arrays
		ExpectRejected("v2-rej-array-too-few", H + "O[1]{A:array<int>}:\n  A:[3]1,2\n\n");
		ExpectRejected("v2-rej-array-too-many", H + "O[1]{A:array<int>}:\n  A:[1]1,2\n\n");
		ExpectRejected("v2-rej-array-zero-with-content", H + "O[1]{A:array<int>}:\n  A:[0]1\n\n");
		ExpectRejected("v2-rej-array-count-not-number", H + "O[1]{A:array<int>}:\n  A:[x]1\n\n");
		ExpectRejected("v2-rej-array-count-negative", H + "O[1]{A:array<int>}:\n  A:[-1]\n\n");
		ExpectRejected("v2-rej-array-count-huge", H + "O[1]{A:array<int>}:\n  A:[1000000000]1\n\n");
		ExpectRejected("v2-rej-array-no-bracket", H + "O[1]{A:array<int>}:\n  A:1,2\n\n");
		ExpectRejected("v2-rej-array-no-close", H + "O[1]{A:array<int>}:\n  A:[2 1,2\n\n");
		ExpectRejected("v2-rej-array-bad-element", H + "O[1]{A:array<int>}:\n  A:[2]1,x\n\n");
		ExpectRejected("v2-rej-array-space-element", H + "O[1]{A:array<int>}:\n  A:[2]1, 2\n\n");
		ExpectRejected("v2-rej-array-bool-element", H + "O[1]{A:array<bool>}:\n  A:[1]yes\n\n");
		ExpectRejected("v2-rej-array-string-count", H + "O[1]{A:array<string>}:\n  A:[1]a,b\n\n");

		// escapes
		ExpectRejected("v2-rej-escape-unknown", H + "O[1]{S:string}:\n  S:a\\qb\n\n");
		ExpectRejected("v2-rej-escape-at-end", H + "O[1]{S:string}:\n  S:abc\\\n\n");
		ExpectRejected("v2-rej-escape-hex-short", H + "O[1]{S:string}:\n  S:a\\x1\n\n");
		ExpectRejected("v2-rej-escape-hex-bad", H + "O[1]{S:string}:\n  S:a\\xZZ\n\n");
		ExpectRejected("v2-rej-escape-in-array", H + "O[1]{A:array<string>}:\n  A:[1]a\\q\n\n");

		// nesting deeper than 64
		{
			std::string text = H + "O[1]{N:object}:\n";
			for(int d = 1; d < 65; ++d)
				text += std::string(static_cast<size_t>(2 * d), ' ') + (d < 64 ? "N:[1]{N:object}:\n" : "N:[1]{V:int}:\n");
			text += std::string(2 * 65, ' ') + "V:5\n\n";
			ExpectRejected("v2-rej-depth-65", text);
		}

		Report("v2-rejected", before, InOut_Ran);
	}

	// WriteToFile refuses data it could not read back
	void TestV2WriteRefused(int &InOut_Ran)
	{
		const char *name = "v2-write-refused";
		const int before = g_Failures;
		const char *path = "cpon_test_refused.cpon";

		// 64 levels can be written and read back, 65 cannot be written
		for(int depth : {64, 65})
		{
			cpon c;
			auto block = c.CreateObject("O")->CreateDataBlock();   // depth 1
			for(int d = 1; d < depth; ++d)
				block = block->CreateObject("N")->CreateDataBlock();
			block->SetValue("V", 5);

			if(depth == 64)
			{
				cpon r;
				if(!c.WriteToFile(path) || !r.LoadFromFile(path))
					Fail(name, "depth 64 must be written and read back");
			}
			else
			{
				// a refused write must leave an existing file as it was
				{
					std::ofstream f(path, std::ios::out | std::ios::trunc);
					f << "keep";
				}
				if(c.WriteToFile(path))
					Fail(name, "depth 65 must not be written");
				else if(ReadWholeTextFile(path) != "keep")
					Fail(name, "a refused write changed the existing file");
			}
			std::remove(path);
		}
		Report(name, before, InOut_Ran);
	}

