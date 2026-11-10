#include "stdafx.h"
#include "Loader/PSF.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	std::vector<u8> buf(data, data + size);
	fs::file f = fs::make_stream<std::vector<u8>>(std::move(buf));
	const auto res = psf::load(f, "fuzz.sfo");
	if (res.errc != psf::error::ok)
		return -1;
	psf::check_registry(res.sfo);
	psf::save_object(res.sfo);
	return 0;
}
