// Fuzz the deployment-log JSONL reader (mender::update::deployments::JsonLogMessagesReader),
// which sanitises the on-device deployment log and turns it into the JSON body uploaded by
// DeploymentClient::PushLogs (upstream fix 39f4a3d664). The reader takes a file path, so each
// input is written to a scratch file.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <mender-update/deployments.hpp>

#include "fuzz_common.hpp"

namespace deployments = mender::update::deployments;

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
	fuzz_common::QuietLogs();
	return 0;
}

static void Drain(deployments::JsonLogMessagesReader &r, std::vector<uint8_t> &buf) {
	for (;;) {
		auto n = r.Read(buf.begin(), buf.end());
		if (!n || n.value() == 0) {
			return;
		}
	}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	const std::string log_path = fuzz_common::ScratchDir() + "/deployment.log";
	{
		std::ofstream os(log_path, std::ios::binary | std::ios::trunc);
		os.write(reinterpret_cast<const char *>(data), size);
	}
	{
		deployments::JsonLogMessagesReader reader {log_path};
		if (reader.SanitizeLogs() == mender::common::error::NoError) {
			reader.TotalDataSize();
			// Small buffer so the header/body/closing state machine is split across reads.
			std::vector<uint8_t> buf(7);
			Drain(reader, buf);
			if (reader.Rewind() == mender::common::error::NoError) {
				std::vector<uint8_t> big(4096);
				Drain(reader, big);
			}
		}
	} // destructor removes the ".sanitized" sibling
	std::remove((log_path + ".sanitized").c_str());
	std::remove(log_path.c_str());
	return 0;
}
