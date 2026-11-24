// Shared helpers for the mender fuzz harnesses.
#pragma once
#include <cstdlib>
#include <string>
#include <unistd.h>

#include <common/log.hpp>

namespace fuzz_common {

// Process-global log level: set once, never changed by any input.
inline void QuietLogs() {
	mender::common::log::SetLevel(mender::common::log::LogLevel::Fatal);
}

// Private scratch directory under $TMPDIR (fallback /tmp), created once per process.
inline const std::string &ScratchDir() {
	static std::string dir = [] {
		const char *t = getenv("TMPDIR");
		std::string tmpl = std::string(t && *t ? t : "/tmp") + "/mender-fuzz-XXXXXX";
		char *d = mkdtemp(&tmpl[0]);
		if (d == nullptr) {
			abort();
		}
		return std::string(d);
	}();
	return dir;
}

// Public key used for the Signature::Verify pass: upstream's RSA test key, shipped in the image
// tree (read-only is fine). $MAYHEM_SRC overrides the /mayhem root for local runs.
inline std::string VerifyKeyPath() {
	const char *root = getenv("MAYHEM_SRC");
	return std::string(root && *root ? root : "/mayhem")
		   + "/tests/src/common/crypto/testdata/public-key.rsa.pem";
}

} // namespace fuzz_common
