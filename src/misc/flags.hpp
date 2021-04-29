#pragma once

#ifndef WOTPP_FLAGS
#define WOTPP_FLAGS

#include <cstdint>
#include <misc/fwddecl.hpp>

namespace wpp {
	enum: flags_t {
		WARN_PARAM_SHADOW_VAR   = 0b000000000001,
		WARN_PARAM_SHADOW_PARAM = 0b000000000010,
		WARN_FUNC_REDEFINED     = 0b000000000100,
		WARN_VAR_REDEFINED      = 0b000000001000,
		WARN_DEEP_RECURSION     = 0b000000010000,
		WARN_EXTRA_ARGS         = 0b000000100000,
		WARN_ALL                = 0b000000111111,
		WARN_USEFUL             = 0b000000000111,

		FLAG_DISABLE_RUN        = 0b000100000000,
		FLAG_DISABLE_FILE       = 0b001000000000,
		INTERNAL_ERROR_STATE    = 0b100000000000,
	};
}

#endif
