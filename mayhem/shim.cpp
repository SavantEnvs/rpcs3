// Link-time infrastructure for the fuzz harnesses: the few thread/version symbols the parsers' logging and
// locking reach, without pulling the emulator's thread layer (and its JIT) into the build.
#include "stdafx.h"
#include "Utilities/Thread.h"
#include "Utilities/version.h"
#include "rpcs3_version.h"

#include <cstdio>
#include <cstdlib>
#include <string_view>

thread_local thread_base* thread_ctrl::g_tls_this_thread = nullptr;
thread_local void (*thread_ctrl::g_tls_error_callback)() = nullptr;

void thread_ctrl::emergency_exit(std::string_view reason)
{
	std::fprintf(stderr, "emergency_exit: %.*s\n", static_cast<int>(reason.size()), reason.data());
	std::abort();
}

void thread_ctrl::set_native_priority(int)
{
}

u64 thread_ctrl::get_tid()
{
	return 1;
}

bool thread_ctrl::is_main()
{
	return true;
}

std::string thread_ctrl::get_name_cached()
{
	return "fuzz";
}

void thread_base::set_name(std::string)
{
}

namespace rpcs3
{
	const utils::version& get_version()
	{
		static constexpr utils::version version{0, 0, 43, utils::version_type::alpha, 1, "fuzz"};
		return version;
	}
}

#include "Emu/system_utils.hpp"
#include "Emu/System.h"

namespace rpcs3::utils
{
	// No emulator home directory in the harness: RAP lookup finds nothing, like a fresh install.
	std::string get_rap_file_path(const std::string_view&)
	{
		return {};
	}
}

// The emulator object the crypto code reads its user id/boot path from; only the header-defined state is used.
games_config::games_config() {}
games_config::~games_config() {}
Emulator::Emulator() noexcept {}
Emulator::~Emulator() noexcept {}
Emulator Emu;

// Worker-thread entry points referenced by the (never started) serialization thread pool template.
namespace stx
{
	atomic_t<u32> g_launch_retainer{0};
}

void thread_base::initialize(void (*)())
{
}

u64 thread_base::finalize(thread_state) noexcept
{
	return 0;
}

thread_base::native_entry thread_base::finalize(u64) noexcept
{
	std::abort();
}

thread_base::native_entry thread_base::make_trampoline(u64 (*)(thread_base*))
{
	return nullptr;
}

// Read-only window onto another file, as the emulator's module loader makes one (same semantics).
namespace
{
	struct file_view : fs::file_base
	{
		const fs::file& m_file;
		u64 m_off;
		u64 m_max;
		u64 m_pos = 0;

		file_view(const fs::file& file, u64 off, u64 max) : m_file(file), m_off(off), m_max(max) {}

		fs::stat_t get_stat() override
		{
			fs::stat_t stat = m_file.get_stat();
			stat.size = std::min<u64>(stat.size > m_off ? stat.size - m_off : 0, m_max);
			stat.is_writable = false;
			return stat;
		}

		bool trunc(u64) override { return false; }

		u64 read(void* buffer, u64 size) override
		{
			const u64 result = read_at(m_pos, buffer, size);
			m_pos += result;
			return result;
		}

		u64 read_at(u64 offset, void* buffer, u64 size) override
		{
			return m_file.read_at(offset + m_off, buffer, std::min<u64>(size, m_max > offset ? m_max - offset : 0));
		}

		u64 write(const void*, u64) override { return 0; }

		u64 seek(s64 offset, fs::seek_mode whence) override
		{
			const s64 new_pos = whence == fs::seek_set ? offset : whence == fs::seek_cur ? offset + static_cast<s64>(m_pos) : whence == fs::seek_end ? offset + static_cast<s64>(size()) : -1;
			if (new_pos < 0)
			{
				fs::g_tls_error = fs::error::inval;
				return -1;
			}
			m_pos = new_pos;
			return m_pos;
		}

		u64 size() override
		{
			const u64 total = m_file.size();
			return std::min<u64>(total > m_off ? total - m_off : 0, m_max);
		}
	};
}

fs::file make_file_view(const fs::file& file, u64 offset, u64 max_size)
{
	fs::file result;
	result.reset(std::make_unique<file_view>(file, offset, max_size));
	return result;
}

// Normally defined by the Qt front end (rpcs3.cpp); the logging layer only reads it.
atomic_t<bool> g_headless = false;

// The emulator calls logs::set_init() in main(); until then every log message is also stored in a
// process-global vector. Calling it once up front keeps that buffer empty, so no input leaves state behind.
extern "C" int LLVMFuzzerInitialize(int*, char***)
{
	logs::set_init({});
	return 0;
}

// Thread lifecycle: the harnesses never start a worker thread, so these are only link-time references.
thread_base::thread_base(native_entry entry, std::string name) noexcept
	: entry_point(entry)
	, m_tname(make_single<std::string>(std::move(name)))
{
}

thread_base::~thread_base() noexcept
{
}

void thread_base::start()
{
	std::abort();
}

bool thread_base::join(bool) const
{
	return true;
}

void thread_ctrl::wait_for(u64, bool)
{
}
