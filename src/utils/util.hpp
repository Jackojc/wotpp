#pragma once

#ifndef WOTPP_UTIL
#define WOTPP_UTIL

#include <utility>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <variant>

#include <structures/position.hpp>

namespace wpp {
	// Concatenate objects with operator<< overload and return string.
	template <typename... Ts>
	inline std::string strcat(Ts&&... args) {
		std::string buf{(sizeof(args) + ...), '\0'};

		std::stringstream ss{buf};
		((ss << std::forward<Ts>(args)), ...);

		return ss.str();
	}
}

namespace wpp {
	// FNV-1a hash.
	// Calculate a hash of a range of bytes.
	template <typename hash_t = uint64_t>
	constexpr auto hash_bytes(const char* begin, const char* const end) {
		hash_t offset_basis = 0;
		hash_t prime = 0;

		if constexpr(sizeof(hash_t) == sizeof(uint64_t)) {
			offset_basis = 14'695'981'039'346'656'037u;
			prime = 1'099'511'628'211u;
		}

		else if constexpr(sizeof(hash_t) == sizeof(uint32_t)) {
			offset_basis = 2'166'136'261u;
			prime = 16'777'619u;
		}

		hash_t hash = offset_basis;

		while (begin != end) {
			hash = (hash ^ static_cast<hash_t>(*begin)) * prime;
			begin++;
		}

		return hash;
	}
}

// Read a file into a string relatively quickly.
namespace wpp {
	inline std::string read_file(const std::string& fname) {
		auto filesize = std::filesystem::file_size(fname);
		std::ifstream is(fname, std::ios::binary);

		auto str = std::string(filesize + 1, '\0');
		is.read(str.data(), static_cast<std::streamsize>(filesize));

		return str;
	}
}

namespace wpp {
	// Get the absolute difference between 2 pointers.
	constexpr ptrdiff_t ptrdiff(const char* a, const char* b) {
		return
			(a > b ? a : b) -
			(a < b ? a : b);
		;
	}
}

namespace wpp {
	// Print an error with position info and exit.
	template <typename... Ts>
	[[noreturn]] inline void error(const Position& pos, Ts&&... args) {
		([&] () -> std::ostream& {
			return (std::cerr << "error @ " << pos << " -> ");
		} () << ... << std::forward<Ts>(args)) << '\n';

		std::exit(1);
	}
}

// A handy wrapper for visit which accepts alternatives as variadic arguments.
namespace wpp {
	template <typename... Ts> struct overloaded: Ts... { using Ts::operator()...; };
	template <typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;

	template <typename V, typename... Ts>
	constexpr decltype(auto) visit(V&& variant, Ts&&... args) {
		return std::visit(wpp::overloaded{
			std::forward<Ts>(args)...
		}, variant);
	}
}

#endif
