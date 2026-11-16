// ==============================================================
// CPON binary round-trip regression tests
//
// Covers Serialize/Deserialize (RLE + XOR scramble) and
// WriteToBinaryFile/LoadFromBinaryFile.
// Exit code: 0 = all passed, 1 = at least one failure.
//
// Build (see build_test.bat):
//   cl /std:c++20 /EHsc /MDd /D_DEBUG test_binary.cpp ../src/*.cpp   (Debug, iterator debugging on)
//   cl /std:c++20 /EHsc /MD  /DNDEBUG test_binary.cpp ../src/*.cpp   (Release)
//
// This file has its own main(); do not compile it together with main.cpp.
// ==============================================================

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <vector>
#include "../src/cpon.hpp"
#include "../src/cpon_hints.hpp"

namespace
{
	int g_Failures = 0;

	void Fail(const char *In_Test, const char *In_What)
	{
		std::printf("    FAIL [%s] %s\n", In_Test, In_What);
		++g_Failures;
	}

	// ---- value helpers (use Ptr variants: GetValue returns *nullptr when missing) ----
	template<class T>
	bool ChkVal(cpon_block &b, const char *key, const T &exp)
	{
		T *p = b.GetValuePtr<T>(key);
		return p && *p == exp;
	}

	template<class T>
	bool ChkArr(cpon_block &b, const char *key, const std::vector<T> &exp)
	{
		std::vector<T> *p = b.GetArrayPtr<T>(key);
		return p && *p == exp;
	}

	// ---- data generators ----

	// No two adjacent characters are equal -> RLE sees only literals
	std::string LiteralString(size_t n)
	{
		std::string s;
		s.reserve(n);
		for(size_t i = 0; i < n; ++i)
			s.push_back(static_cast<char>('!' + (i % 90)));
		return s;
	}

	std::string RandomBytes(size_t n, uint32_t seed)
	{
		std::mt19937 rng(seed);
		std::string s;
		s.reserve(n);
		for(size_t i = 0; i < n; ++i)
			s.push_back(static_cast<char>(rng() & 0xFF));
		return s;
	}

	std::vector<int> RandomInts(size_t n, uint32_t seed)
	{
		std::mt19937 rng(seed);
		std::vector<int> v(n);
		for(auto &x : v)
			x = static_cast<int>(rng());
		return v;
	}

	// ---- test case ----
	struct Case
	{
		const char *name;
		std::function<void(cpon &)> build;
		std::function<bool(cpon &)> verify;
	};

	bool RoundTripMemory(const Case &c)
	{
		cpon src;
		c.build(src);
		const std::vector<uint8_t> buf = src.Serialize();

		cpon dst;
		if(!dst.Deserialize(buf))
		{
			Fail(c.name, "Deserialize returned false");
			return false;
		}
		if(!c.verify(dst))
		{
			Fail(c.name, "values differ after Serialize/Deserialize");
			return false;
		}

		// second generation: re-serialize the deserialized data
		const std::vector<uint8_t> buf2 = dst.Serialize();
		cpon dst2;
		if(!dst2.Deserialize(buf2) || !c.verify(dst2))
		{
			Fail(c.name, "values differ after second-generation round trip");
			return false;
		}
		return true;
	}

	bool RoundTripFile(const Case &c)
	{
		const char *path = "cpon_test_tmp.cponb";
		cpon src;
		c.build(src);
		if(!src.WriteToBinaryFile(path))
		{
			Fail(c.name, "WriteToBinaryFile failed");
			return false;
		}
		cpon dst;
		const bool loaded = dst.LoadFromBinaryFile(path);
		std::remove(path);
		if(!loaded)
		{
			Fail(c.name, "LoadFromBinaryFile failed");
			return false;
		}
		if(!c.verify(dst))
		{
			Fail(c.name, "values differ after WriteToBinaryFile/LoadFromBinaryFile");
			return false;
		}
		return true;
	}

	bool RoundTripText(const Case &c)
	{
		const char *path = "cpon_test_tmp.cpon";
		cpon src;
		c.build(src);
		if(!src.WriteToFile(path))
		{
			Fail(c.name, "WriteToFile failed");
			return false;
		}
		cpon dst;
		const bool loaded = dst.LoadFromFile(path);
		std::remove(path);
		if(!loaded)
		{
			Fail(c.name, "LoadFromFile failed");
			return false;
		}
		if(!c.verify(dst))
		{
			Fail(c.name, "values differ after WriteToFile/LoadFromFile");
			return false;
		}
		return true;
	}

	// Keys where one name is a substring of another (hint de-duplication and text loading must match whole keys)
	std::vector<Case> MakeKeyCases()
	{
		std::vector<Case> cases;
		cases.push_back(Case{ "key-prefix-has-exit-time",
			[](cpon &c)
			{
				auto b = c.CreateObject("T")->CreateDataBlock();
				b->SetValue("HasExitTime", true);
				b->SetValue("ExitTime", 0.75f);
			},
			[](cpon &c) { return ChkVal<bool>(*c["T"][0], "HasExitTime", true) && ChkVal<float>(*c["T"][0], "ExitTime", 0.75f); } });
		cases.push_back(Case{ "key-suffix-after-longer",
			[](cpon &c)
			{
				auto b = c.CreateObject("T")->CreateDataBlock();
				b->SetValue("ExitTime", 2);
				b->SetValue("Time", 1);
			},
			[](cpon &c) { return ChkVal<int>(*c["T"][0], "ExitTime", 2) && ChkVal<int>(*c["T"][0], "Time", 1); } });
		cases.push_back(Case{ "key-short-after-long",
			[](cpon &c)
			{
				auto b = c.CreateObject("T")->CreateDataBlock();
				b->SetValue("AA", 1);
				b->SetValue("A", 2);
			},
			[](cpon &c) { return ChkVal<int>(*c["T"][0], "AA", 1) && ChkVal<int>(*c["T"][0], "A", 2); } });
		// Block 1 lacks "Time": its "ExitTime" line must not be read as "Time"
		cases.push_back(Case{ "key-missing-suffix-in-block",
			[](cpon &c)
			{
				auto o = c.CreateObject("T");
				auto b0 = o->CreateDataBlock();
				b0->SetValue("Time", 1);
				b0->SetValue("ExitTime", 2);
				o->CreateDataBlock()->SetValue("ExitTime", 5);
			},
			[](cpon &c)
			{
				auto &o = c["T"];
				return o.GetDataCount() == 2 && ChkVal<int>(*o[0], "Time", 1) && ChkVal<int>(*o[0], "ExitTime", 2) &&
					ChkVal<int>(*o[1], "ExitTime", 5) && o[1]->GetValuePtr<int>("Time") == nullptr;
			} });
		// Block 1 lacks "A" and its "B" value contains "A": the value must not be read as key "A"
		cases.push_back(Case{ "key-missing-name-in-value",
			[](cpon &c)
			{
				auto o = c.CreateObject("T");
				auto b0 = o->CreateDataBlock();
				b0->SetValue("A", std::string("first"));
				b0->SetValue("B", std::string("second"));
				o->CreateDataBlock()->SetValue("B", std::string("xAx"));
			},
			[](cpon &c)
			{
				auto &o = c["T"];
				return o.GetDataCount() == 2 && ChkVal<std::string>(*o[0], "A", "first") && ChkVal<std::string>(*o[1], "B", "xAx") &&
					o[1]->GetValuePtr<std::string>("A") == nullptr;
			} });
		return cases;
	}

	// Floating-point values must survive the text format exactly (shortest round-trip text, not 6 digits)
	std::vector<Case> MakeFloatTextCases()
	{
		std::vector<Case> cases;
		cases.push_back(Case{ "text-float-double-precision",
			[](cpon &c)
			{
				auto b = c.CreateObject("T")->CreateDataBlock();
				b->SetValue("F", 0.123456789f);
				b->SetValue("D", 3.141592653589793);
				b->SetValue("Tiny", -2.2250738585072014e-308);
			},
			[](cpon &c)
			{
				return ChkVal<float>(*c["T"][0], "F", 0.123456789f) && ChkVal<double>(*c["T"][0], "D", 3.141592653589793) &&
					ChkVal<double>(*c["T"][0], "Tiny", -2.2250738585072014e-308);
			} });
		// 24 characters each: the longest text a double can need
		cases.push_back(Case{ "text-array-long-double",
			[](cpon &c)
			{
				c.CreateObject("T")->CreateDataBlock()->SetArray("A", std::vector<double>{-2.2250738585072014e-308, -1.2345678901234567e-100});
				c["T"][0]->SetArray("AF", std::vector<float>{-1.17549435e-38f, 0.123456789f});
			},
			[](cpon &c)
			{
				return ChkArr<double>(*c["T"][0], "A", {-2.2250738585072014e-308, -1.2345678901234567e-100}) &&
					ChkArr<float>(*c["T"][0], "AF", {-1.17549435e-38f, 0.123456789f});
			} });
		return cases;
	}

	// A '.' in a directory name is not a file extension: the default extension must still be added
	void TestDefaultExtension(int &InOut_Ran)
	{
		namespace fs = std::filesystem;
		const fs::path dir = "cpon_test_dir.v2";
		fs::create_directories(dir);

		cpon src;
		src.CreateObject("Obj")->CreateDataBlock()->SetValue("Name", std::string("abc"));

		cpon text, binary;
		if(!src.WriteToFile("cpon_test_dir.v2/save") || !fs::exists(dir / "save.cpon"))
			Fail("default-extension", "WriteToFile did not add .cpon");
		else if(!text.LoadFromFile("cpon_test_dir.v2/save") || !ChkVal<std::string>(*text["Obj"][0], "Name", "abc"))
			Fail("default-extension", "LoadFromFile did not add .cpon");
		if(!src.WriteToBinaryFile("cpon_test_dir.v2/save") || !fs::exists(dir / "save.cponb"))
			Fail("default-extension", "WriteToBinaryFile did not add .cponb");
		else if(!binary.LoadFromBinaryFile("cpon_test_dir.v2/save") || !ChkVal<std::string>(*binary["Obj"][0], "Name", "abc"))
			Fail("default-extension", "LoadFromBinaryFile did not add .cponb");
		// an explicit extension is kept as is
		if(!src.WriteToFile("cpon_test_dir.v2/keep.txt") || !fs::exists(dir / "keep.txt"))
			Fail("default-extension", "WriteToFile changed an explicit extension");

		std::error_code ec;
		fs::remove_all(dir, ec);
		++InOut_Ran;
		std::printf("  %-28s %s\n", "default-extension", "checked");
	}

	// Single object "Obj" / single block / single string value "Name"
	Case StringCase(const char *name, const std::string &value)
	{
		return Case{
			name,
			[value](cpon &c)
			{
				auto o = c.CreateObject("Obj");
				o->CreateDataBlock()->SetValue("Name", value);
			},
			[value](cpon &c)
			{
				return c.GetObjectCount() == 1 && c["Obj"].GetDataCount() == 1 && ChkVal<std::string>(*c["Obj"][0], "Name", value);
			} };
	}

	std::vector<Case> MakeCases()
	{
		std::vector<Case> cases;

		// 1. original repro: payload ends with a literal run
		cases.push_back(StringCase("ends-with-literal (repro)", "abcdefg"));

		// 2. payload ends with a run
		cases.push_back(StringCase("ends-with-run", "xyzaaaaaaaa"));

		// 3. all-identical bytes > 128 (run splitting at 128)
		cases.push_back(StringCase("identical-bytes-1000", std::string(1000, 'A')));
		cases.push_back(StringCase("identical-bytes-129", std::string(129, 'B')));
		cases.push_back(StringCase("identical-bytes-128", std::string(128, 'C')));

		// 4. literal longer than 128 bytes (literal splitting at 128), ending in a literal
		cases.push_back(StringCase("literal-300", LiteralString(300)));
		cases.push_back(StringCase("literal-128", LiteralString(128)));
		cases.push_back(StringCase("literal-129", LiteralString(129)));

		// 4b. empty string as the very last item of the payload (Reader::ReadString at end of data)
		cases.push_back(StringCase("empty-string-at-end", ""));

		// 5. empty object list
		cases.push_back(Case{
			"empty-object-list",
			[](cpon &) {},
			[](cpon &c) { return c.GetObjectCount() == 0; } });

		// 5b. object without blocks
		cases.push_back(Case{
			"object-without-blocks",
			[](cpon &c) { c.CreateObject("Empty"); },
			[](cpon &c) { return c.GetObjectCount() == 1 && c["Empty"].GetDataCount() == 0; } });

		// 6. nested objects + every value / array type
		cases.push_back(Case{
			"nested-all-types",
			[](cpon &c)
			{
				auto root = c.CreateObject("Root");
				auto b0 = root->CreateDataBlock();
				b0->SetValue("S", std::string("hello"));
				b0->SetValue("I", -42);
				b0->SetValue("U", 4000000000u);
				b0->SetValue("F", 1.5f);
				b0->SetValue("D", 3.141592653589793);
				b0->SetValue("B", true);
				b0->SetArray("AS", std::vector<std::string>{"a", "", "ccc"});
				b0->SetArray("AI", std::vector<int>{1, -2, 3});
				b0->SetArray("AU", std::vector<unsigned int>{0u, 4000000000u});
				b0->SetArray("AF", std::vector<float>{0.5f, -1.25f});
				b0->SetArray("AD", std::vector<double>{1e300, -1e-300});
				b0->SetArray("AB", std::vector<bool>{true, false, true});
				b0->SetArray("EmptyI", std::vector<int>{});

				auto child = b0->CreateObject("Child");
				auto cb = child->CreateDataBlock();
				cb->SetValue("X", 7);
				cb->SetValue("Y", std::string("why"));
				auto grand = cb->CreateObject("Grand");
				auto gb = grand->CreateDataBlock();
				gb->SetValue("Z", false);
				gb->SetArray("ZI", std::vector<int>{9, 9, 9, 9, 9});

				auto b1 = root->CreateDataBlock();
				b1->SetValue("S", std::string("second"));
				b1->SetValue("I", 2);

				auto second = c.CreateObject("Second");
				second->CreateDataBlock()->SetValue("Only", 123456);
			},
			[](cpon &c)
			{
				if(c.GetObjectCount() != 2) return false;
				cpon_object &root = c["Root"];
				if(root.GetDataCount() != 2) return false;
				auto b0 = root[0];
				bool ok = ChkVal<std::string>(*b0, "S", "hello")
					&& ChkVal<int>(*b0, "I", -42)
					&& ChkVal<unsigned int>(*b0, "U", 4000000000u)
					&& ChkVal<float>(*b0, "F", 1.5f)
					&& ChkVal<double>(*b0, "D", 3.141592653589793)
					&& ChkVal<bool>(*b0, "B", true)
					&& ChkArr<std::string>(*b0, "AS", {"a", "", "ccc"})
					&& ChkArr<int>(*b0, "AI", {1, -2, 3})
					&& ChkArr<unsigned int>(*b0, "AU", {0u, 4000000000u})
					&& ChkArr<float>(*b0, "AF", {0.5f, -1.25f})
					&& ChkArr<double>(*b0, "AD", {1e300, -1e-300})
					&& ChkArr<bool>(*b0, "AB", {true, false, true})
					&& ChkArr<int>(*b0, "EmptyI", {});
				if(!ok) return false;

				auto child = b0->GetObject("Child");
				if(!child || child->GetDataCount() != 1) return false;
				auto cb = (*child)[0];
				if(!ChkVal<int>(*cb, "X", 7) || !ChkVal<std::string>(*cb, "Y", "why")) return false;
				auto grand = cb->GetObject("Grand");
				if(!grand || grand->GetDataCount() != 1) return false;
				auto gb = (*grand)[0];
				if(!ChkVal<bool>(*gb, "Z", false) || !ChkArr<int>(*gb, "ZI", {9, 9, 9, 9, 9})) return false;

				auto b1 = root[1];
				if(!ChkVal<std::string>(*b1, "S", "second") || !ChkVal<int>(*b1, "I", 2)) return false;

				cpon_object &second = c["Second"];
				return second.GetDataCount() == 1 && ChkVal<int>(*second[0], "Only", 123456);
			} });

		// 7. large (~100k) random-ish payload plus long run plus many blocks
		cases.push_back(Case{
			"large-100k",
			[](cpon &c)
			{
				auto big = c.CreateObject("Big");
				auto b = big->CreateDataBlock();
				b->SetValue("R", RandomBytes(100000, 12345));
				b->SetValue("Q", std::string(30000, 'q'));
				b->SetArray("AI", RandomInts(25000, 777));

				auto many = c.CreateObject("Many");
				for(int i = 0; i < 2000; ++i)
				{
					auto mb = many->CreateDataBlock();
					mb->SetValue("Id", i);
					mb->SetValue("Name", std::string("name") + std::to_string(i % 7));
				}
			},
			[](cpon &c)
			{
				if(c.GetObjectCount() != 2) return false;
				auto b = c["Big"][0];
				if(!ChkVal<std::string>(*b, "R", RandomBytes(100000, 12345))
					|| !ChkVal<std::string>(*b, "Q", std::string(30000, 'q'))
					|| !ChkArr<int>(*b, "AI", RandomInts(25000, 777)))
					return false;
				cpon_object &many = c["Many"];
				if(many.GetDataCount() != 2000) return false;
				for(int i = 0; i < 2000; ++i)
				{
					auto mb = many[i];
					if(!ChkVal<int>(*mb, "Id", i) || !ChkVal<std::string>(*mb, "Name", std::string("name") + std::to_string(i % 7)))
						return false;
				}
				return true;
			} });

		return cases;
	}
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

		// A nested object whose middle block is empty: the writer leaves two blank lines there
		{
			const char *name = "v1-nested-empty-middle";
			const int before = g_Failures;
			cpon c;
			const bool ok = LoadText(name,
				"#ObjNum : 1\n"
				"O[1]{N:object,After:int}:\n"
				"  N:[3]{X:int}:\n"
				"    X:1\n"
				"\n"
				"\n"
				"    X:3\n"
				"  After:9\n"
				"\n", c);
			auto n = ok ? c["O"][0]->GetObject("N") : nullptr;
			if(!n || n->GetDataCount() != 3
				|| !ChkVal<int>(*(*n)[0], "X", 1) || !(*n)[1]->IsEmpty() || !ChkVal<int>(*(*n)[2], "X", 3)
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
		ExpectRejected("v1-rej-nested-huge-block-count", "#ObjNum : 1\nO[1]{N:object,After:int}:\n  N:[1000000000]{X:int}:\n    X:1\n  After:9\n\n");
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

} // namespace

int main()
{
	const std::vector<Case> cases = MakeCases();

	int ran = 0;
	for(const auto &c : cases)
	{
		const int before = g_Failures;
		RoundTripMemory(c);
		RoundTripFile(c);
		++ran;
		std::printf("  %-28s %s\n", c.name, g_Failures == before ? "ok" : "FAILED");
		std::fflush(stdout);
	}

	std::vector<Case> textCases = MakeKeyCases();
	for(auto &c : MakeFloatTextCases())
		textCases.push_back(std::move(c));

	for(const auto &c : textCases)
	{
		const int before = g_Failures;
		RoundTripMemory(c);
		RoundTripFile(c);
		RoundTripText(c);
		++ran;
		std::printf("  %-28s %s\n", c.name, g_Failures == before ? "ok" : "FAILED");
		std::fflush(stdout);
	}

	TestDefaultExtension(ran);

	TestParseHints(ran);

	TestV1Fixtures(ran);
	TestV1Rejected(ran);

	// Corrupt / truncated input must be rejected, not crash
	{
		cpon src;
		src.CreateObject("Obj")->CreateDataBlock()->SetValue("Name", std::string("abcdefg"));
		std::vector<uint8_t> buf = src.Serialize();
		buf.resize(buf.size() - 3);
		cpon dst;
		if(dst.Deserialize(buf))
			Fail("truncated-input", "Deserialize accepted truncated data");
		++ran;
		std::printf("  %-28s %s\n", "truncated-input", "checked");
	}

	std::printf("%d test cases, %d failure(s)\n", ran, g_Failures);
	return g_Failures == 0 ? 0 : 1;
}
