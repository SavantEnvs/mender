// Fuzz the Mender Artifact (.mender) parser through its public entry point:
// mender::artifact::Parse() over the whole file, then every payload and every payload file,
// read to the end (this verifies the per-file sha256 from the manifest).
// Reaches: outer tar (libarchive wrapper), version, manifest (upstream fix 1a7e1f961e),
// manifest.sig, header.tar[.gz|.xz|.zst] (header-info, type-info, meta-data, scripts), payloads.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <artifact/artifact.hpp>
#include <artifact/config.hpp>
#include <common/io.hpp>
#include <common/path.hpp>

#include "fuzz_common.hpp"

namespace artifact = mender::artifact;
namespace io = mender::common::io;

extern "C" int LLVMFuzzerInitialize(int *, char ***) {
	fuzz_common::QuietLogs();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	// State scripts in the header are written under this directory; give each input a fresh one.
	const std::string scripts = fuzz_common::ScratchDir() + "/scripts";
	mender::common::path::DeleteRecursively(scripts);

	io::StringReader reader {std::string(reinterpret_cast<const char *>(data), size)};
	artifact::config::ParserConfig conf {
		scripts, 3, {}, artifact::config::Signature::Skip};

	// Second, header-only pass with signature verification on, against a fixed public key
	// (upstream's own test key): reaches the "signed artifact required" check, manifest.sig
	// base64 decode, key loading and crypto::VerifySign. A fuzzed signature never verifies, so
	// this pass ends at Parse(); the payload walk below uses the Skip pass.
	{
		io::StringReader vreader {std::string(reinterpret_cast<const char *>(data), size)};
		artifact::config::ParserConfig vconf {
			scripts, 3, {fuzz_common::VerifyKeyPath()}, artifact::config::Signature::Verify};
		artifact::Parse(vreader, vconf);
		mender::common::path::DeleteRecursively(scripts);
	}

	auto ex_artifact = artifact::Parse(reader, conf);
	if (!ex_artifact) {
		return 0;
	}
	auto &art = ex_artifact.value();
	for (size_t i = 0; i < art.header.subHeaders.size(); i++) {
		auto view = artifact::View(art, i);
		if (view) {
			view.value().header.GetProvides();
			view.value().header.GetDepends();
		}
	}

	// Bounded walk: a real artifact (format v3) carries one payload (data/0000.tar), and the
	// parser rejects a second one; the caps only bound per-input work on hostile archives so a
	// single input can't spend the exec timeout iterating thousands of tiny entries. 4 payloads
	// x 16 files is well past anything the seeds or a valid artifact contain.
	std::vector<uint8_t> buf(4096);
	for (int p = 0; p < 4; p++) {
		auto ex_payload = art.Next();
		if (!ex_payload) {
			break;
		}
		auto &payload = ex_payload.value();
		for (int f = 0; f < 16; f++) {
			auto ex_file = payload.Next();
			if (!ex_file) {
				break;
			}
			auto &file = ex_file.value();
			file.Name();
			file.Size();
			for (;;) {
				auto n = file.Read(buf.begin(), buf.end());
				if (!n || n.value() == 0) {
					break;
				}
			}
		}
	}
	mender::common::path::DeleteRecursively(scripts);
	return 0;
}
