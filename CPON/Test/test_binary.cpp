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
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>
#include "../src/cpon.hpp"

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
