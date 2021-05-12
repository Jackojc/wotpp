#include <misc/overflow_detect.hpp>

#include <cstring>
#include <cstdlib>

// For mcontext_t register offsets
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/resource.h>

namespace {
	void sig_handler(int sig, siginfo_t* info, void* ctx) {
		wpp::EmergencyLogger emerg;
		auto uctx = static_cast<ucontext_t*>(ctx);

		uintptr_t sp, ip;

		#if defined(__x86_64__)
		sp = uctx->uc_mcontext.gregs[REG_RSP];
		ip = uctx->uc_mcontext.gregs[REG_RIP];
		#elif defined(__aarch64__)
		sp = uctx->uc_mcontext.sp;
		ip = uctx->uc_mcontext.ip;
		#else
		#	error Unknown architecture
		#endif

		static constexpr ptrdiff_t dead_zone_pad = 96 * 1024;
		static constexpr ptrdiff_t stack_access_pad = 4096;

		auto dead_zone_end = wpp::g_overflow_local_info.stack_end() + dead_zone_pad;
		auto dead_zone_start = dead_zone_end - dead_zone_pad;

		bool is_potential_overflow = true;

		// Is not within the dead zone
		if (sp < dead_zone_start || sp > dead_zone_end)
			is_potential_overflow = false;

		// Fault is not accessing the stack
		ptrdiff_t dist = sp - reinterpret_cast<uintptr_t>(info->si_addr);
		if (std::abs(dist) > stack_access_pad)
			is_potential_overflow = false;

		if (!is_potential_overflow) {
			struct sigaction sa;
			sa.sa_flags = 0;
			sa.sa_handler = SIG_DFL;
			sigemptyset(&sa.sa_mask);

			sigaction(sig, &sa, nullptr);
			return;
		}

		emerg << "Potential stack overflow detected!\n";
		emerg << "With sp = 0x" << sp << "\n";
		emerg << "At ip = 0x" << ip << "\n";

		_exit(255);
	}

} // namespace anonymous

namespace wpp {
	thread_local OverflowLocalInfo g_overflow_local_info;

	OverflowLocalInfo::OverflowLocalInfo()
	: alt_stack{std::make_unique<char[]>(SIGSTKSZ)} {
		uintptr_t sp;
		struct rlimit rlim;

		getrlimit(RLIMIT_STACK, &rlim);

		#if defined(__x86_64__)
		asm volatile ("mov %%rsp, %0" : "=r"(sp));
		#elif defined(__aarch64__)
		asm volatile ("mov %0, sp" : "=r"(sp));
		#else
		#	error Unknown architecture
		#endif

		stack_base = sp;
		stack_size = rlim.rlim_cur;

		stack_t ss;
		ss.ss_sp = alt_stack.get();
		ss.ss_size = SIGSTKSZ;
		ss.ss_flags = 0;

		sigaltstack(&ss, nullptr);

		struct sigaction sa;
		sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
		sa.sa_sigaction = sig_handler;
		sigemptyset(&sa.sa_mask);

		sigaction(SIGSEGV, &sa, nullptr);
		sigaction(SIGBUS, &sa, nullptr);
	}

	const EmergencyLogger& EmergencyLogger::operator<<(const char* str) const {
		write(STDERR_FILENO, str, strlen(str));

		return *this;
	}

	const EmergencyLogger& EmergencyLogger::operator<<(uintptr_t n) const {
		constexpr const char *digits = "0123456789abcdef";
		constexpr size_t buffer_size = sizeof(uintptr_t) * 2;

		char buf[buffer_size];

		for (size_t i = 0; i < buffer_size; i++) {
			uint8_t v = (n >> ((buffer_size - i - 1) * 4)) & 0xF;
			buf[i] = digits[v];
		}

		write(STDERR_FILENO, buf, buffer_size);

		return *this;
	}
} // namespace wpp
