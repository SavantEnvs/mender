// Fuzz the client configuration file loader (mender.conf JSON):
// mender::client_shared::config_parser::MenderConfigFromFile::LoadFile().
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

#include <client_shared/config_parser.hpp>

#include "fuzz_common.hpp"

namespace config_parser = mender::client_shared::config_parser;

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
	fuzz_common::QuietLogs();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	const std::string conf_path = fuzz_common::ScratchDir() + "/mender.conf";
	{
		std::ofstream os(conf_path, std::ios::binary | std::ios::trunc);
		os.write(reinterpret_cast<const char *>(data), size);
	}
	config_parser::MenderConfigFromFile conf;
	conf.LoadFile(conf_path);
	std::remove(conf_path.c_str());
	return 0;
}
