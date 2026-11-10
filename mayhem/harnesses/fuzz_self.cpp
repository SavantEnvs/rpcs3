#include "stdafx.h"
#include "Crypto/unself.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	std::vector<u8> buf(data, data + size);
	fs::file f = fs::make_stream<std::vector<u8>>(std::move(buf));

	// Fixed 16-byte klicensee: only used by the NPDRM-encrypted path, never derived from the input.
	static const u8 klic[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};

	bool any = false;
	for (const bool elf32 : {false, true})
	{
		SELFDecrypter dec(f);
		if (!dec.LoadHeaders(elf32))
			continue;
		any = true;
		if (!dec.LoadMetadata(klic))
			continue;
		if (!dec.DecryptData())
			continue;
		(void)dec.MakeElf(elf32);
	}
	return any ? 0 : -1;
}
