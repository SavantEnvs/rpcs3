// Known-answer probes for the code the fuzz targets exercise. Each prints "KAT <name> ok" on success;
// mayhem/test.sh counts those lines, so a run that never reaches the parsers cannot pass.
#include "stdafx.h"
#include "Loader/PSF.h"
#include "Loader/TAR.h"
#include "Crypto/sha256.h"

#include <cstdio>
#include <cstring>

static int g_failed = 0;

static void check(const char* name, bool ok)
{
	if (ok)
	{
		std::printf("KAT %s ok\n", name);
	}
	else
	{
		std::printf("KAT %s FAILED\n", name);
		g_failed++;
	}
}

static fs::file stream_of(std::vector<u8> bytes)
{
	return fs::make_stream<std::vector<u8>>(std::move(bytes));
}

static void kat_psf()
{
	psf::registry reg;
	psf::assign(reg, "CATEGORY", psf::string(4, "HG"));
	psf::assign(reg, "PARENTAL_LEVEL", psf::entry(5u));
	psf::assign(reg, "TITLE", psf::string(128, "Known Answer Game"));
	psf::assign(reg, "TITLE_ID", psf::string(16, "BLUS30001"));

	const std::vector<u8> blob = psf::save_object(reg);
	check("psf-magic", blob.size() > 20 && blob[0] == 0 && blob[1] == 'P' && blob[2] == 'S' && blob[3] == 'F' && blob[4] == 1 && blob[5] == 1);

	const auto res = psf::load(stream_of(blob), "kat.sfo");
	check("psf-roundtrip-ok", res.errc == psf::error::ok && res.sfo.size() == 4);
	check("psf-title-id", psf::get_string(res.sfo, "TITLE_ID") == "BLUS30001");
	check("psf-title", psf::get_string(res.sfo, "TITLE") == "Known Answer Game");
	check("psf-parental-level", psf::get_integer(res.sfo, "PARENTAL_LEVEL") == 5);

	std::vector<u8> bad = blob;
	bad[1] = 'X';
	check("psf-reject-magic", psf::load(stream_of(bad), "kat.sfo").errc == psf::error::not_psf);
	check("psf-reject-empty", psf::load(stream_of({}), "kat.sfo").errc != psf::error::ok);
}

static void kat_sha256()
{
	u8 out[32]{};
	const char abc[] = "abc";
	mbedtls_sha256_ret(reinterpret_cast<const u8*>(abc), 3, out, 0);
	static const u8 expect[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
		0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
	check("sha256-abc", std::memcmp(out, expect, 32) == 0);
}

static void kat_tar()
{
	std::vector<u8> tar(1536, 0);
	std::memcpy(tar.data(), "hello.txt", 9);
	std::memcpy(tar.data() + 124, "00000000010", 11); // size 8 (octal 10)
	tar[156] = '0';
	std::memcpy(tar.data() + 257, "ustar\0", 6);
	std::memcpy(tar.data() + 512, "ABCDEFGH", 8);

	fs::file f = stream_of(tar);
	tar_object obj(f);
	const std::vector<std::string> names = obj.get_filenames();
	check("tar-one-entry", names.size() == 1 && names[0] == "hello.txt");

	std::vector<u8> junk(1024, 0x41);
	fs::file jf = stream_of(junk);
	tar_object jobj(jf);
	check("tar-reject-junk", jobj.get_filenames().empty());
}

int main()
{
	kat_psf();
	kat_sha256();
	kat_tar();
	return g_failed ? 1 : 0;
}
