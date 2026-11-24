// Fuzz the HTTP helpers that read server-/config-controlled strings (src/common/http/http.cpp):
//   BreakDownUrl   - every server URL from the config and every artifact download URI the
//                    deployment server sends is split here before connecting;
//   URLDecode      - percent-decoding;
//   GetRemainingTime - parses the HTTP-date of a server's Retry-After / artifact expiry header.
// The input is used as one string for all three (plus BreakDownUrl with_auth=true).
// GetRemainingTime subtracts the current time from the parsed date; only the parse depends on
// the input, and its result is not used, so each input's outcome depends only on the input.
#include <cstddef>
#include <cstdint>
#include <string>

#include <common/http.hpp>

#include "fuzz_common.hpp"

namespace http = mender::common::http;

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
	fuzz_common::QuietLogs();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	const std::string s(reinterpret_cast<const char *>(data), size);
	http::BrokenDownUrl addr;
	http::BreakDownUrl(s, addr);
	http::BrokenDownUrl addr_auth;
	http::BreakDownUrl(s, addr_auth, true);
	http::URLDecode(s);
	http::GetRemainingTime(s);
	return 0;
}
