#include <string_view>
#include <iomanip>
#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <utility>

#include <misc/flags.hpp>
#include <misc/util/util.hpp>
#include <misc/repl.hpp>
#include <misc/argp.hpp>
#include <backend/eval/eval.hpp>
#include <frontend/parser/parser.hpp>

#ifdef WPP_ENABLE_OVERFLOW_DETECTOR
#include <misc/overflow_detect.hpp>
#endif

int main(int argc, const char* argv[]) {
#ifdef WPP_ENABLE_OVERFLOW_DETECTOR
	WPP_OVERFLOW_DETECTOR_INIT;
#endif
	constexpr auto ver = "alpha-git";
	constexpr auto desc = "A small macro language for producing and manipulating strings.";


	std::string_view outputf;
	std::vector<std::string_view> warnings;
	std::vector<std::string_view> path_dirs;

	bool repl = false;
	bool disable_run = false;
	bool disable_file = false;
	bool disable_colour = false;
	bool inline_reports = false;
	bool force = false;

	std::vector<const char*> positional;

	if (wpp::argparser(
		wpp::Info{ver, desc},
		argc, argv, &positional,
		wpp::Opt{outputf,        "output file",                                       "--output",         "-o"},
		wpp::Opt{warnings,       "toggle warnings",                                   "--warnings",       "-W"},
		wpp::Opt{repl,           "repl mode",                                         "--repl",           "-r"},
		wpp::Opt{disable_run,    "toggle run & pipe intrinsics",                      "--disable-run",    "-R"},
		wpp::Opt{disable_file,   "toggle file & use intrinsics",                      "--disable-file",   "-F"},
		wpp::Opt{disable_colour, "toggle ANSI colour sequences",                      "--disable-colour", "-c"},
		wpp::Opt{inline_reports, "toggle inline reports",                             "--inline-reports", "-i"},
		wpp::Opt{force,          "overwrite file if it exists",                       "--force",          "-f"},
		wpp::Opt{path_dirs,      "specify directories to search when sourcing files", "--search-path",    "-s"}
	))
		return 0;


	wpp::flags_t flags = 0;

	for (const auto& x: warnings) {
		// Set warnings flags.
		if (x == "param-shadow-var")
			flags |= wpp::WARN_PARAM_SHADOW_VAR;

		else if (x == "param-shadow-param")
			flags |= wpp::WARN_PARAM_SHADOW_PARAM;

		else if (x == "func-redefined")
			flags |= wpp::WARN_FUNC_REDEFINED;

		else if (x == "var-redefined")
			flags |= wpp::WARN_VAR_REDEFINED;

		else if (x == "deep-recursion")
			flags |= wpp::WARN_DEEP_RECURSION;

		else if (x == "extra-args")
			flags |= wpp::WARN_EXTRA_ARGS;


		// Unset warning flags.
		else if (x == "no-param-shadow-var")
			flags &= ~wpp::WARN_PARAM_SHADOW_VAR;

		else if (x == "no-param-shadow-param")
			flags &= ~wpp::WARN_PARAM_SHADOW_PARAM;

		else if (x == "no-func-redefined")
			flags &= ~wpp::WARN_FUNC_REDEFINED;

		else if (x == "no-var-redefined")
			flags &= ~wpp::WARN_VAR_REDEFINED;

		else if (x == "no-deep-recursion")
			flags &= ~wpp::WARN_DEEP_RECURSION;

		else if (x == "no-extra-args")
			flags &= ~wpp::WARN_EXTRA_ARGS;


		// Enable all warnings.
		else if (x == "all")
			flags = wpp::WARN_ALL;

		else if (x == "useful")
			flags = wpp::WARN_USEFUL;


		// Unknown warning flag.
		else {
			std::cerr << "error: unrecognized warning '" << x << "'\n";
			return 1;
		}
	}


	if (disable_run)
		flags |= wpp::FLAG_DISABLE_RUN;

	if (disable_file)
		flags |= wpp::FLAG_DISABLE_FILE;

	if (disable_colour)
		flags |= wpp::FLAG_DISABLE_COLOUR;

	if (inline_reports)
		flags |= wpp::FLAG_INLINE_REPORTS;


	// Build search path.
	wpp::SearchPath search_path;
	for (auto& path: path_dirs)
		search_path.emplace_back(path);


	if (repl)
		return wpp::repl();


	if (positional.empty()) {
		std::cerr << "error: no input files\n";
		return 1;
	}


	std::string out;
	const auto initial_path = std::filesystem::current_path();

	for (const auto& fname: positional) {
		// Set current path to path of file.
		const auto path = initial_path / std::filesystem::path{fname};
		std::filesystem::current_path(path.parent_path());

		wpp::Env env{ initial_path, search_path, flags };

		try {
			env.sources.push(path, wpp::read_file(path), wpp::modes::normal);

			wpp::node_t root = wpp::parse(env);

			if (env.state & wpp::ABORT_EVALUATION)
				return 1;

			out += wpp::evaluate(root, env);
		}

		catch (const wpp::Report& e) {
			std::cerr << e.str();
			return 1;
		}

		catch (const wpp::FileNotFoundError&) {
			std::cerr << "error: file '" << fname << "' not found\n";
			return 1;
		}

		catch (const wpp::NotFileError&) {
			std::cerr << "error: '" << fname << "' is not a file\n";
			return 1;
		}

		catch (const wpp::FileReadError&) {
			std::cerr << "error: cannot read '" << fname << "'\n";
			return 1;
		}

		catch (const wpp::SymlinkError&) {
			std::cerr << "error: symlink '" << fname << "' resolves to itself\n";
			return 1;
		}

		std::filesystem::current_path(initial_path);
	}

	if (not outputf.empty()) {
		std::error_code ec;

		if (not force and std::filesystem::exists(outputf, ec)) {
			std::cerr << "error: file '" << outputf << "' exists\n";
			return 1;
		}

		wpp::write_file(outputf, out);
	}

	else
		std::cout << out;

	return 0;
}
