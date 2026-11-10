#include "stdafx.h"
#include "Loader/ELF.h"

template <typename T>
static bool try_open(const fs::file& f, bs_t<elf_opt> opts)
{
	T obj;
	if (obj.open(f, 0, opts) != elf_error::ok)
		return false;
	// Round-trip: exercises save() over whatever the loader accepted.
	(void)obj.save();
	return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	std::vector<u8> buf(data, data + size);
	fs::file f = fs::make_stream<std::vector<u8>>(std::move(buf));

	bool any = false;
	for (bs_t<elf_opt> opts : {bs_t<elf_opt>{}, bs_t<elf_opt>{elf_opt::no_data}, bs_t<elf_opt>{elf_opt::no_sections}})
	{
		any |= try_open<ppu_exec_object>(f, opts);
		any |= try_open<ppu_prx_object>(f, opts);
		any |= try_open<ppu_rel_object>(f, opts);
		any |= try_open<spu_exec_object>(f, opts);
	}
	return any ? 0 : -1;
}
