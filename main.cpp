#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

static const regex QUOTE_RE(R"(\s*#\s*include\s*\"([^\"]*)\"\s*)");
static const regex ANGLE_RE(R"(\s*#\s*include\s*<([^>]*)>\s*)");

path operator""_p(const char* data, std::size_t sz) {
	return path(data, data + sz);
}

// напишите эту функцию
bool PreprocessRecursive(const path& in_file, ostream& out, const vector<path>& include_directories) {
	ifstream in(in_file);
	if (!in.is_open()) return false;

	string line;
	int line_number = 0;
	while (getline(in, line)) {
		++line_number;
		smatch match;
		bool found = false;
		path included_path;

		if (regex_match(line, match, QUOTE_RE)) {
			string filename = match[1];
			path parent_dir = in_file.parent_path();
			if (filesystem::exists(parent_dir / filename)) {
				included_path = parent_dir / filename;
				found = true;
			}
			else {
				for (const auto& dir : include_directories) {
					if (filesystem::exists(dir / filename)) {
						included_path = dir / filename;
						found = true;
						break;
					}
				}
			}

			if (found) {
				if (!PreprocessRecursive(included_path, out, include_directories)) {
					return false;
				}
			}
			else {
				cout << "unknown include file " << filename << " at file " << in_file.string() << " at line " << line_number << endl;
				return false;
			}
		}
		else if (regex_match(line, match, ANGLE_RE)) {
			string filename = match[1];
			for (const auto& dir : include_directories) {
				if (filesystem::exists(dir / filename)) {
					included_path = dir / filename;
					found = true;
					break;
				}
			}

			if (found) {
				if (!PreprocessRecursive(included_path, out, include_directories)) {
					return false;
				}
			}
			else {
				cout << "unknown include file " << filename << " at file " << in_file.string() << " at line " << line_number << endl;
				return false;
			}
		}
		else {
			out << line << "\n";
		}
	}
	return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
	ifstream in(in_file);
	if (!in.is_open()) return false;
	in.close();

	ofstream out(out_file);
	if (!out.is_open()) return false;

	return PreprocessRecursive(in_file, out, include_directories);
}

string GetFileContents(string file) {
	ifstream stream(file);

	// конструируем string по двум итераторам
	return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

void Test() {
	error_code err;
	filesystem::remove_all("sources"_p, err);
	filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
	filesystem::create_directories("sources"_p / "include1"_p, err);
	filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

	{
		ofstream file("sources/a.cpp");
		file << "// this comment before include\n"
			"#include \"dir1/b.h\"\n"
			"// text between b.h and c.h\n"
			"#include \"dir1/d.h\"\n"
			"\n"
			"int SayHello() {\n"
			"    cout << \"hello, world!\" << endl;\n"
			"#   include<dummy.txt>\n"
			"}\n"s;
	}
	{
		ofstream file("sources/dir1/b.h");
		file << "// text from b.h before include\n"
			"#include \"subdir/c.h\"\n"
			"// text from b.h after include"s;
	}
	{
		ofstream file("sources/dir1/subdir/c.h");
		file << "// text from c.h before include\n"
			"#include <std1.h>\n"
			"// text from c.h after include\n"s;
	}
	{
		ofstream file("sources/dir1/d.h");
		file << "// text from d.h before include\n"
			"#include \"lib/std2.h\"\n"
			"// text from d.h after include\n"s;
	}
	{
		ofstream file("sources/include1/std1.h");
		file << "// std1\n"s;
	}
	{
		ofstream file("sources/include2/lib/std2.h");
		file << "// std2\n"s;
	}

	assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
		{ "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

	ostringstream test_out;
	test_out << "// this comment before include\n"
		"// text from b.h before include\n"
		"// text from c.h before include\n"
		"// std1\n"
		"// text from c.h after include\n"
		"// text from b.h after include\n"
		"// text between b.h and c.h\n"
		"// text from d.h before include\n"
		"// std2\n"
		"// text from d.h after include\n"
		"\n"
		"int SayHello() {\n"
		"    cout << \"hello, world!\" << endl;\n"s;

	assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
	Test();
}
