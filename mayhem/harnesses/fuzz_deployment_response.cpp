// Fuzz the deployment-check server response: the JSON body the Mender server returns for
// GET /api/devices/v{1,2}/deployments/device/deployments/next. mender-update's daemon loads it
// (json::Load) and hands it to ApiResponseJsonToStateData (src/mender-update/daemon/states.cpp),
// the first code that reads the server-controlled fields (id, artifact.source.uri/expire).
#include <cstddef>
#include <cstdint>
#include <string>

#include <common/json.hpp>
#include <mender-update/daemon/context.hpp>

#include "fuzz_common.hpp"

namespace json = mender::common::json;
namespace upd_daemon = mender::update::daemon;

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
	fuzz_common::QuietLogs();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	auto j = json::Load(std::string(reinterpret_cast<const char *>(data), size));
	if (!j) {
		return 0;
	}
	auto sd = upd_daemon::ApiResponseJsonToStateData(j.value());
	if (sd) {
		// Touch what the daemon goes on to use from the parsed state data.
		volatile size_t n = sd.value().update_info.id.size()
							+ sd.value().update_info.artifact.source.uri.size()
							+ sd.value().update_info.artifact.source.expire.size();
		(void) n;
	}
	return 0;
}
