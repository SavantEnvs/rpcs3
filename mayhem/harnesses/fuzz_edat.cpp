#include "stdafx.h"
#include "Crypto/unedat.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	std::vector<u8> buf(data, data + size);
	fs::file f = fs::make_stream<std::vector<u8>>(std::move(buf));

	// Fixed final key: the input never chooses the decryption key.
	const u128 key = u128{0x0123456789abcdefULL} << 64 | 0xfedcba9876543210ULL;
	EDATADecrypter dec(f, key, "fuzz.edat", true);
	if (!dec.ReadHeader(false))
		return -1;

	u8 out[512];
	for (u64 pos = 0; pos < 4096; pos += sizeof(out))
	{
		if (dec.ReadData(pos, out, sizeof(out), false) == 0)
			break;
	}
	return 0;
}
