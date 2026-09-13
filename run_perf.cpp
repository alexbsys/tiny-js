/*
 * TinyJS microbenchmarks: lexer drain, tokenizer, interpreter.
 * Not a correctness suite — run from the tiny-js source directory.
 */

#include "TinyJS.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <chrono>
#endif

#ifdef _WIN32
static double now_sec() {
	static LARGE_INTEGER fr = {0};
	LARGE_INTEGER li;
	if (fr.QuadPart == 0)
		QueryPerformanceFrequency(&fr);
	QueryPerformanceCounter(&li);
	return (double)li.QuadPart / (double)fr.QuadPart;
}
#else
static double now_sec() {
	using clock = std::chrono::steady_clock;
	return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}
#endif

struct Row {
	std::string name;
	std::string phase;
	int iters;
	double sec;
	double ops_per_sec;
};

static std::vector<Row> g_rows;
static bool g_json = false;

static void add_row(const char *name, const char *phase, int iters, double sec) {
	Row r;
	r.name = name;
	r.phase = phase;
	r.iters = iters;
	r.sec = sec;
	r.ops_per_sec = (sec > 0.0) ? (double)iters / sec : 0.0;
	g_rows.push_back(r);
	if (!g_json) {
		printf("  %-22s  %-10s  %8d  %8.3f  %12.0f\n",
			name, phase, iters, sec * 1000.0, r.ops_per_sec);
		fflush(stdout);
	}
}

template<typename Fn>
static void bench_auto(const char *name, const char *phase, Fn fn, double min_sec = 0.40) {
	try {
		fn(); // warmup / catch errors early
	} catch (CScriptException *e) {
		fprintf(stderr, "  SKIP %s/%s: %s\n", name, phase, e->toString().c_str());
		delete e;
		return;
	}
	int n = 1;
	double sec = 0.0;
	for (;;) {
		double t0 = now_sec();
		for (int i = 0; i < n; ++i)
			fn();
		sec = now_sec() - t0;
		if (sec >= min_sec || n >= (1 << 20))
			break;
		n *= 2;
	}
	add_row(name, phase, n, sec);
}

static void drain_lexer(const char *code) {
	CScriptLex lex(code);
	while (lex.tk != LEX_EOF)
		lex.match(lex.tk);
}

static void run_tokenizer(const char *code) {
	CScriptTokenizer tok(code);
	(void)tok.tk;
}

static void run_execute(const char *code, const char *file) {
	CTinyJS js;
	js.execute(code, file);
}

static std::string load_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return std::string();
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::string out((size_t)sz, '\0');
	size_t n = fread(&out[0], 1, (size_t)sz, f);
	fclose(f);
	out.resize(n);
	return out;
}

static std::string gen_var_decls(int count) {
	std::string s;
	s.reserve((size_t)count * 28);
	for (int i = 0; i < count; ++i) {
		s += "var foo_";
		s += std::to_string(i);
		s += " = 123;\n";
	}
	return s;
}

static std::string gen_keywords(int count) {
	static const char *kw[] = {
		"if", "else", "do", "while", "for", "in", "break", "continue",
		"function", "return", "var", "let", "const", "with", "true", "false",
		"null", "new", "try", "catch", "finally", "throw", "typeof", "void",
		"delete", "instanceof", "switch", "case", "default", "yield"
	};
	std::string s;
	s.reserve((size_t)count * 16);
	for (int i = 0; i < count; ++i) {
		s += kw[i % (int)(sizeof(kw) / sizeof(kw[0]))];
		s += " ident_";
		s += std::to_string(i);
		s += " ";
		if ((i % 8) == 7)
			s += "\n";
	}
	return s;
}

static std::string gen_comments(int count) {
	std::string s;
	s.reserve((size_t)count * 40);
	for (int i = 0; i < count; ++i) {
		s += "// line comment ";
		s += std::to_string(i);
		s += "\n";
		s += "/* block comment ";
		s += std::to_string(i);
		s += " */\n";
	}
	s += "var x = 1;\n";
	return s;
}

static std::string gen_numbers(int count) {
	std::string s;
	s.reserve((size_t)count * 20);
	s += "var x = 0";
	for (int i = 0; i < count; ++i) {
		s += "+";
		s += std::to_string(i);
		if (i % 3 == 0)
			s += ".5";
		if (i % 7 == 0)
			s += "n";
		if ((i % 16) == 15)
			s += "\n";
	}
	s += ";\n";
	return s;
}

static std::string gen_strings(int count) {
	std::string s;
	s.reserve((size_t)count * 24);
	s += "var s = \"\"";
	for (int i = 0; i < count; ++i) {
		s += " + \"str_";
		s += std::to_string(i);
		s += "\\n\"";
		if ((i % 8) == 7)
			s += "\n";
	}
	s += ";\n";
	return s;
}

static const char *kLoopInt =
	"var s = 0;\n"
	"for (var i = 0; i < 20000; i++) s = s + i;\n"
	"if (s !== 199990000) throw new Error('loop_int');\n";

static const char *kLoopProp =
	"var o = {a:1, b:2, c:3, d:4};\n"
	"var s = 0;\n"
	"for (var i = 0; i < 10000; i++) s = s + o.a + o.b + o.c + o.d;\n"
	"if (s !== 100000) throw new Error('loop_prop');\n";

static const char *kLoopCall =
	"function add(a, b) { return a + b; }\n"
	"var s = 0;\n"
	"for (var i = 0; i < 8000; i++) s = add(s, 1);\n"
	"if (s !== 8000) throw new Error('loop_call');\n";

static const char *kLoopArray =
	"var a = [];\n"
	"for (var i = 0; i < 4000; i++) a.push(i);\n"
	"var s = 0;\n"
	"for (var i = 0; i < a.length; i++) s = s + a[i];\n"
	"if (s !== 7998000) throw new Error('loop_array');\n";

static const char *kLoopString =
	"var s = '';\n"
	"for (var i = 0; i < 1500; i++) s = s + 'x';\n"
	"if (s.length !== 1500) throw new Error('loop_string');\n";

static const char *kLoopMap =
	"var m = new Map();\n"
	"for (var i = 0; i < 800; i++) m.set(i, i);\n"
	"var s = 0;\n"
	"for (var i = 0; i < 800; i++) s = s + m.get(i);\n"
	"if (s !== 319600) throw new Error('loop_map');\n";

static const char *kLoopMapForof =
	"var m = new Map();\n"
	"for (var i = 0; i < 800; i++) m.set(i, i);\n"
	"var s = 0;\n"
	"for (const pair of m) s = s + pair[1];\n"
	"if (s !== 319600) throw new Error('loop_map_forof');\n";

static const char *kLoopOpt =
	"var o = {a:{b:1}};\n"
	"var s = 0;\n"
	"for (var i = 0; i < 8000; i++) s = s + (o?.a?.b ?? 0);\n"
	"if (s !== 8000) throw new Error('loop_opt');\n";

static void bench_file_or_src(const char *name, const char *path, const char *fallback) {
	std::string loaded = load_file(path);
	const char *code = loaded.empty() ? fallback : loaded.c_str();
	const char *file = loaded.empty() ? name : path;

	bench_auto(name, "lexer", [&]() { drain_lexer(code); });
	bench_auto(name, "tokenize", [&]() { run_tokenizer(code); });
	bench_auto(name, "execute", [&]() { run_execute(code, file); }, 0.50);
}

static void print_table_header() {
	printf("  %-22s  %-10s  %8s  %8s  %12s\n", "name", "phase", "iters", "ms", "ops/s");
	printf("  %-22s  %-10s  %8s  %8s  %12s\n",
		"----------------------", "----------", "--------", "--------", "------------");
}

static void print_json() {
	printf("[\n");
	for (size_t i = 0; i < g_rows.size(); ++i) {
		const Row &r = g_rows[i];
		printf("  {\"name\":\"%s\",\"phase\":\"%s\",\"iters\":%d,\"ms\":%.3f,\"ops_per_sec\":%.1f}%s\n",
			r.name.c_str(), r.phase.c_str(), r.iters, r.sec * 1000.0, r.ops_per_sec,
			(i + 1 == g_rows.size()) ? "" : ",");
	}
	printf("]\n");
}

int main(int argc, char **argv) {
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--json") == 0)
			g_json = true;
	}

	if (!g_json) {
		printf("TinyJS performance microbenchmarks (Release timings, QPC)\n");
		print_table_header();
	}

	try {
		std::string ids = gen_var_decls(2500);
		std::string kws = gen_keywords(4000);
		std::string cms = gen_comments(2000);
		std::string nums = gen_numbers(2000);
		std::string strs = gen_strings(800);

		bench_auto("lex_decls", "lexer", [&]() { drain_lexer(ids.c_str()); });
		bench_auto("lex_decls", "tokenize", [&]() { run_tokenizer(ids.c_str()); });

		bench_auto("lex_keywords", "lexer", [&]() { drain_lexer(kws.c_str()); });

		bench_auto("lex_comments", "lexer", [&]() { drain_lexer(cms.c_str()); });
		bench_auto("lex_comments", "tokenize", [&]() { run_tokenizer(cms.c_str()); });

		bench_auto("lex_numbers", "lexer", [&]() { drain_lexer(nums.c_str()); });
		bench_auto("lex_strings", "lexer", [&]() { drain_lexer(strs.c_str()); });

		bench_file_or_src("loop_int", "tests/perf/loop_int.js", kLoopInt);
		bench_file_or_src("loop_prop", "tests/perf/loop_prop.js", kLoopProp);
		bench_file_or_src("loop_call", "tests/perf/loop_call.js", kLoopCall);
		bench_file_or_src("loop_array", "tests/perf/loop_array.js", kLoopArray);
		bench_file_or_src("loop_string", "tests/perf/loop_string.js", kLoopString);
		bench_file_or_src("loop_map", "tests/perf/loop_map.js", kLoopMap);
		bench_file_or_src("loop_map_forof", "tests/perf/loop_map_forof.js", kLoopMapForof);
		bench_file_or_src("loop_opt", "tests/perf/loop_opt.js", kLoopOpt);
	} catch (CScriptException *e) {
		fprintf(stderr, "perf error: %s\n", e->toString().c_str());
		delete e;
		return 1;
	} catch (const std::exception &e) {
		fprintf(stderr, "perf error: %s\n", e.what());
		return 1;
	}

	if (g_json)
		print_json();
	else {
		printf("\nDone. %d rows.\n", (int)g_rows.size());
		printf("phases: lexer = CScriptLex drain, tokenize = CScriptTokenizer, execute = CTinyJS::execute\n");
	}
	return 0;
}
