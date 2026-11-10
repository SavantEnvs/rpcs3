#include "stdafx.h"
#include "Loader/TAR.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	std::vector<u8> buf(data, data + size);
	fs::file f = fs::make_stream<std::vector<u8>>(std::move(buf));

	tar_object tar(f);
	const std::vector<std::string> names = tar.get_filenames();
	if (names.empty())
		return -1;

	for (const std::string& name : names)
	{
		if (auto ar = tar.get_file(name))
		{
			(void)ar->get_size(); 
		}
	}
	return 0;
}
