// Minimal ZIP container for .lumora packages: STORED and DEFLATE entries, no ZIP64, no encryption, no data descriptors.
// Deflate uses the dependency-free codec in core/package/deflate.h. ZIP64 is not implemented.
// The reader never trusts sizes or offsets: every read is bounds-checked and anything it does not understand is rejected.
// The reader runs the container audit (core/package/zip_audit.h) and the entry policy (core/package/entry_policy.h)
// before it reads any entry; ZipReadResult::audit_verdict names the verdict when one of them rejects the archive.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "scene.h"

namespace lumora::scene {

// Writes entries (sorted by name, as FileMap iterates) into a ZIP byte stream. Fixed timestamps for reproducible output.
// level 0 stores everything; 1 and 6 deflate an entry only when that makes it smaller.
std::vector<std::uint8_t> write_zip(const FileMap& files, int level = 6);
// Same as write_zip(files, 0).
std::vector<std::uint8_t> write_zip_store(const FileMap& files);

struct ZipReadResult {
  bool ok = false;
  std::string error;
  std::string audit_verdict;  // name of the container audit or entry policy verdict when one of them rejected the archive
  FileMap files;
};

ZipReadResult read_zip_store(const std::uint8_t* data, std::size_t size, std::uint64_t max_entries = 10000,
                             std::uint64_t max_total_bytes = 2048ull << 20, std::uint64_t max_ratio = 200);

}  // namespace lumora::scene
