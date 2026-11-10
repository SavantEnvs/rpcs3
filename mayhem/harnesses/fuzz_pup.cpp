#include "stdafx.h"
#include "Loader/PUP.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	std::vector<u8> buf(data, data + size);
	pup_object pup(fs::make_stream<std::vector<u8>>(std::move(buf)));

	if (static_cast<pup_error>(pup) != pup_error::ok)
		return -1;

	for (u64 id = 0x100; id <= 0x601; id++)
	{
		if (fs::file f = pup.get_file(id))
		{
			u8 b[16];
			f.read(b, sizeof(b));
		}
	}
	return 0;
}
