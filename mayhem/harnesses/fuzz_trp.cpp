#include "stdafx.h"
#include "Loader/TRP.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	std::vector<u8> buf(data, data + size);
	fs::file f = fs::make_stream<std::vector<u8>>(std::move(buf));

	TRPLoader trp(f);
	if (!trp.LoadHeader())
		return -1;

	(void)trp.GetRequiredSpace();
	(void)trp.ContainsEntry("TROP.SFM");
	(void)trp.ContainsEntry("ICON0.PNG");
	return 0;
}
