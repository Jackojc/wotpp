#pragma once

#ifndef WOTPP_OVERFLOW_DETECT
#define WOTPP_OVERFLOW_DETECT

#include <cstdint>
#include <cstddef>
#include <memory>

namespace wpp {
	struct OverflowLocalInfo {
		OverflowLocalInfo();

		OverflowLocalInfo(const OverflowLocalInfo&) = delete;
		OverflowLocalInfo(OverflowLocalInfo&&) = delete;
		OverflowLocalInfo& operator=(const OverflowLocalInfo&) = delete;
		OverflowLocalInfo& operator=(OverflowLocalInfo&&) = delete;

		uintptr_t stack_end() const {
			return stack_base - stack_size;
		}

	private:
		uintptr_t stack_base;
		size_t stack_size;

		std::unique_ptr<char[]> alt_stack;
	};

	struct EmergencyLogger {
		const EmergencyLogger &operator<<(const char* str) const;
		const EmergencyLogger &operator<<(uintptr_t n) const;
	};

	thread_local extern OverflowLocalInfo g_overflow_local_info;
} // namespace wpp

#define WPP_OVERFLOW_DETECTOR_INIT do { (void)wpp::g_overflow_local_info.stack_end(); } while(0)

#endif // WOTPP_OVERFLOW_DETECT
