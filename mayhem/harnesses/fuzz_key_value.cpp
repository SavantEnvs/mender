// Fuzz the key=value parser behind the identity and inventory script output parsers
// (mender::common::key_value_parser::ParseKeyValues / ParseKeyValueMap).
// The input is split into lines exactly as the identity/inventory parsers do with script stdout.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <common/key_value_parser.hpp>

#include "fuzz_common.hpp"

namespace kvp = mender::common::key_value_parser;

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
	fuzz_common::QuietLogs();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	// Byte 0 picks the delimiter for a second pass (the API takes any char); the rest is lines.
	// An empty input still reaches the parser (one empty line, default '=' delimiter).
	const char delim = size ? static_cast<char>(data[0]) : '=';
	std::string s = size ? std::string(reinterpret_cast<const char *>(data + 1), size - 1)
						 : std::string();
	std::vector<std::string> items;
	size_t start = 0;
	for (;;) {
		size_t nl = s.find('\n', start);
		items.push_back(s.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
		if (nl == std::string::npos) {
			break;
		}
		start = nl + 1;
	}
	kvp::ParseKeyValues(items);
	kvp::ParseKeyValueMap(items);
	kvp::ParseKeyValues(items, delim);
	kvp::ParseKeyValueMap(items, delim);
	return 0;
}
