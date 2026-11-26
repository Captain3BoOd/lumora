// Structural audit of a .lumora ZIP container. The check order is the binding comment in
// zip_audit.h; docs/zip-audit.md lists the exact bounds. Pure function on a byte buffer: no file
// system, no inflate, no recursion. The only allocation is the result and one record per central
// entry (the entry count is bounded by the central directory size, which is inside the buffer).
#include "zip_audit.h"

#include <cstring>
#include <limits>

namespace lumora::package {
namespace {

constexpr std::uint32_t kEocdSig = 0x06054b50u;
constexpr std::uint32_t kLocatorSig = 0x07064b50u;
constexpr std::uint32_t kZip64Sig = 0x06064b50u;
constexpr std::uint32_t kCentralSig = 0x02014b50u;
constexpr std::uint32_t kLocalSig = 0x04034b50u;

constexpr std::size_t kEocdSize = 22;
constexpr std::size_t kLocatorSize = 20;
constexpr std::size_t kZip64Size = 56;
constexpr std::size_t kCentralSize = 46;
constexpr std::size_t kLocalSize = 30;
constexpr std::uint64_t kMaxCommentWindow = 65535;

constexpr std::uint64_t kSentinel32 = 0xFFFFFFFFull;
constexpr std::uint64_t kSentinel16 = 0xFFFFull;
constexpr std::uint64_t kMax64 = std::numeric_limits<std::uint64_t>::max();

constexpr std::uint16_t kFlagEncrypted = 0x0001;
constexpr std::uint16_t kFlagDeflateOptions = 0x0006;  // bits 1 and 2
constexpr std::uint16_t kFlagStrongEncryption = 0x0040;
constexpr std::uint16_t kFlagUtf8 = 0x0800;
constexpr std::uint16_t kFlagMaskedHeader = 0x2000;
constexpr std::uint16_t kFlagsAllowed =
    kFlagEncrypted | kFlagDeflateOptions | kFlagStrongEncryption | kFlagUtf8 | kFlagMaskedHeader;

std::uint16_t rd16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
std::uint32_t rd32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
std::uint64_t rd64(const std::uint8_t* p) {
  return static_cast<std::uint64_t>(rd32(p)) | (static_cast<std::uint64_t>(rd32(p + 4)) << 32);
}

ZipAuditResult fail(ZipVerdict v, std::size_t index, std::uint64_t offset) {
  ZipAuditResult r;
  r.verdict = v;
  r.entry_index = index;
  r.offset = offset;
  return r;
}

// Walks the extra field blocks. `broken` is set when a block header or body runs past the field.
struct ExtraInfo {
  bool broken = false;
  int zip64_blocks = 0;
  const std::uint8_t* zip64 = nullptr;
  std::size_t zip64_len = 0;
};

ExtraInfo scan_extra(const std::uint8_t* p, std::size_t n) {
  ExtraInfo info;
  std::size_t pos = 0;
  while (pos < n) {
    if (n - pos < 4) {
      info.broken = true;
      return info;
    }
    const std::uint16_t id = rd16(p + pos);
    const std::size_t len = rd16(p + pos + 2);
    pos += 4;
    if (len > n - pos) {
      info.broken = true;
      return info;
    }
    if (id == 0x0001) {
      ++info.zip64_blocks;
      info.zip64 = p + pos;
      info.zip64_len = len;
    }
    pos += len;
  }
  return info;
}

struct Record {
  std::size_t central_offset = 0;
  std::uint16_t name_len = 0;
  std::uint16_t flags = 0;
  std::uint16_t method = 0;
  std::uint16_t time = 0;
  std::uint16_t date = 0;
  std::uint32_t crc = 0;
  std::uint64_t compressed = 0;
  std::uint64_t uncompressed = 0;
  std::uint64_t local_offset = 0;
  std::uint64_t data_offset = 0;
  std::uint64_t data_end = 0;
  bool is_dir = false;
};

}  // namespace

ZipAuditResult audit_zip(const std::uint8_t* data, std::size_t size, const ZipAuditLimits& limits) {
  // 1. Size.
  if (static_cast<std::uint64_t>(size) > limits.max_archive_bytes) {
    return fail(ZipVerdict::TooLarge, 0, 0);
  }
  if (size < kEocdSize || data == nullptr) return fail(ZipVerdict::TooSmall, 0, 0);

  // 2. End record. Scan the last min(size, 22 + 65535) bytes for signatures that leave room for a
  // whole record.
  const std::size_t window = size < kEocdSize + kMaxCommentWindow
                                 ? size
                                 : static_cast<std::size_t>(kEocdSize + kMaxCommentWindow);
  std::size_t sig_count = 0;
  std::size_t first_sig = 0;
  std::size_t second_sig = 0;
  for (std::size_t p = size - window; p + kEocdSize <= size; ++p) {
    if (data[p] == 0x50 && rd32(data + p) == kEocdSig) {
      if (sig_count == 0) first_sig = p;
      if (sig_count == 1) second_sig = p;
      ++sig_count;
      if (sig_count == 2) break;
    }
  }
  if (sig_count == 0) return fail(ZipVerdict::NoEndRecord, 0, 0);
  if (sig_count >= 2) return fail(ZipVerdict::MultipleEndRecords, 0, second_sig);
  const std::size_t eocd = first_sig;
  const std::uint64_t comment_len = rd16(data + eocd + 20);
  const std::uint64_t comment_end = static_cast<std::uint64_t>(eocd) + kEocdSize + comment_len;
  if (comment_end < size) {
    return fail(ZipVerdict::TrailingData, 0, comment_end);
  }
  if (comment_end > size || comment_len > limits.max_comment_bytes) {
    return fail(ZipVerdict::NoEndRecord, 0, eocd);
  }
  const std::uint16_t disk = rd16(data + eocd + 4);
  const std::uint16_t cd_disk = rd16(data + eocd + 6);
  const std::uint16_t entries_disk = rd16(data + eocd + 8);
  const std::uint16_t entries_total = rd16(data + eocd + 10);
  if (disk != 0 || cd_disk != 0 || entries_disk != entries_total) {
    return fail(ZipVerdict::MultiDisk, 0, eocd);
  }

  // 3. ZIP64.
  std::uint64_t count = entries_total;
  std::uint64_t cd_size = rd32(data + eocd + 12);
  std::uint64_t cd_off = rd32(data + eocd + 16);
  std::uint64_t end_pos = eocd;  // where the directory has to end
  const bool sentinel =
      count == kSentinel16 || cd_size == kSentinel32 || cd_off == kSentinel32;
  const bool locator = eocd >= kLocatorSize && rd32(data + eocd - kLocatorSize) == kLocatorSig;
  if (!sentinel) {
    if (locator) return fail(ZipVerdict::Zip64Mismatch, 0, eocd - kLocatorSize);
  } else {
    if (!locator || eocd < kLocatorSize + kZip64Size) {
      return fail(ZipVerdict::Zip64Mismatch, 0, eocd);
    }
    const std::size_t loc = eocd - kLocatorSize;
    const std::size_t rec = loc - kZip64Size;
    if (rd32(data + loc + 4) != 0 || rd32(data + loc + 16) != 1) {
      return fail(ZipVerdict::MultiDisk, 0, loc);
    }
    if (rd64(data + loc + 8) != rec) return fail(ZipVerdict::Zip64Mismatch, 0, loc);
    if (rd32(data + rec) != kZip64Sig || rd64(data + rec + 4) != 44) {
      return fail(ZipVerdict::Zip64Mismatch, 0, rec);
    }
    if (rd32(data + rec + 16) != 0 || rd32(data + rec + 20) != 0 ||
        rd64(data + rec + 24) != rd64(data + rec + 32)) {
      return fail(ZipVerdict::MultiDisk, 0, rec);
    }
    const std::uint64_t count64 = rd64(data + rec + 32);
    const std::uint64_t size64 = rd64(data + rec + 40);
    const std::uint64_t off64 = rd64(data + rec + 48);
    const bool count_ok = count == kSentinel16 ? count64 >= kSentinel16 : count64 == count;
    const bool size_ok = cd_size == kSentinel32 ? size64 >= kSentinel32 : size64 == cd_size;
    const bool off_ok = cd_off == kSentinel32 ? off64 >= kSentinel32 : off64 == cd_off;
    if (!count_ok || !size_ok || !off_ok) return fail(ZipVerdict::Zip64Mismatch, 0, rec);
    count = count64;
    cd_size = size64;
    cd_off = off64;
    end_pos = rec;
  }

  // 4. Entry count and central directory bounds.
  if (count > limits.max_entries) return fail(ZipVerdict::TooManyEntries, 0, eocd);
  if (cd_off > end_pos || cd_size != end_pos - cd_off) {
    return fail(ZipVerdict::CentralDirectoryBounds, 0, eocd);
  }
  {
    std::uint64_t pos = cd_off;
    for (std::uint64_t i = 0; i < count; ++i) {
      if (end_pos - pos < kCentralSize) return fail(ZipVerdict::CentralDirectoryBounds, 0, eocd);
      const std::uint8_t* h = data + pos;
      const std::uint64_t total = kCentralSize + static_cast<std::uint64_t>(rd16(h + 28)) +
                                  rd16(h + 30) + rd16(h + 32);
      if (total > end_pos - pos) return fail(ZipVerdict::CentralDirectoryBounds, 0, eocd);
      pos += total;
    }
    if (pos != end_pos) return fail(ZipVerdict::CentralDirectoryBounds, 0, eocd);
  }

  // 5. Central entries, in order.
  std::vector<Record> records;
  records.reserve(static_cast<std::size_t>(count));
  {
    std::size_t pos = static_cast<std::size_t>(cd_off);
    for (std::uint64_t i = 0; i < count; ++i) {
      const std::uint8_t* h = data + pos;
      const std::size_t index = static_cast<std::size_t>(i);
      const std::uint64_t at = pos;
      if (rd32(h) != kCentralSig) return fail(ZipVerdict::BadCentralHeader, index, at);
      const std::uint16_t made_by = rd16(h + 4);
      const std::uint16_t flags = rd16(h + 8);
      const std::uint16_t method = rd16(h + 10);
      const std::size_t name_len = rd16(h + 28);
      const std::size_t extra_len = rd16(h + 30);
      const std::size_t comment_len_entry = rd16(h + 32);
      if (name_len == 0 || name_len > limits.max_name_bytes || extra_len > limits.max_extra_bytes ||
          comment_len_entry > limits.max_comment_bytes) {
        return fail(ZipVerdict::BadCentralHeader, index, at);
      }
      const ExtraInfo extra = scan_extra(h + kCentralSize + name_len, extra_len);
      if (extra.broken) return fail(ZipVerdict::BadCentralHeader, index, at);
      if (rd16(h + 34) != 0) return fail(ZipVerdict::MultiDisk, index, at);
      if ((flags & (kFlagEncrypted | kFlagStrongEncryption | kFlagMaskedHeader)) != 0) {
        return fail(ZipVerdict::Encrypted, index, at);
      }
      if ((flags & ~kFlagsAllowed) != 0 || ((flags & kFlagDeflateOptions) != 0 && method != 8)) {
        return fail(ZipVerdict::UnsupportedFeature, index, at);
      }
      if (method != 0 && method != 8) return fail(ZipVerdict::UnsupportedMethod, index, at);
      const std::uint32_t external = rd32(h + 38);
      const bool unix_mode = (made_by >> 8) == 3;
      const std::uint32_t type = unix_mode ? ((external >> 16) & 0xF000u) : 0u;
      if (unix_mode && type != 0 && type != 0x8000u && type != 0x4000u) {
        return fail(ZipVerdict::SpecialFile, index, at);
      }
      const bool name_dir = h[kCentralSize + name_len - 1] == '/';
      if ((type == 0x4000u && !name_dir) || (name_dir && type == 0x8000u)) {
        return fail(ZipVerdict::BadCentralHeader, index, at);
      }
      const std::uint64_t crc_size32 = rd32(h + 20);
      const std::uint64_t usize32 = rd32(h + 24);
      const std::uint64_t offset32 = rd32(h + 42);
      if (name_dir && (method != 0 || crc_size32 != 0 || usize32 != 0)) {
        return fail(ZipVerdict::BadSizes, index, at);
      }
      // ZIP64 entry extra: sentinel fields only, in uncompressed, compressed, offset order.
      const bool need_u = usize32 == kSentinel32;
      const bool need_c = crc_size32 == kSentinel32;
      const bool need_o = offset32 == kSentinel32;
      const std::size_t needed = (need_u ? 1u : 0u) + (need_c ? 1u : 0u) + (need_o ? 1u : 0u);
      Record rec;
      rec.central_offset = pos;
      rec.name_len = static_cast<std::uint16_t>(name_len);
      rec.flags = flags;
      rec.method = method;
      rec.time = rd16(h + 12);
      rec.date = rd16(h + 14);
      rec.crc = rd32(h + 16);
      rec.compressed = crc_size32;
      rec.uncompressed = usize32;
      rec.local_offset = offset32;
      rec.is_dir = name_dir;
      if (extra.zip64_blocks > 1 || (needed == 0 && extra.zip64_blocks != 0)) {
        return fail(ZipVerdict::Zip64Mismatch, index, at);
      }
      if (needed != 0) {
        if (extra.zip64_blocks != 1 || extra.zip64_len != needed * 8) {
          return fail(ZipVerdict::Zip64Mismatch, index, at);
        }
        const std::uint8_t* z = extra.zip64;
        if (need_u) {
          rec.uncompressed = rd64(z);
          z += 8;
        }
        if (need_c) {
          rec.compressed = rd64(z);
          z += 8;
        }
        if (need_o) rec.local_offset = rd64(z);
        if ((need_u && rec.uncompressed < kSentinel32) ||
            (need_c && rec.compressed < kSentinel32) ||
            (need_o && rec.local_offset < kSentinel32)) {
          return fail(ZipVerdict::Zip64Mismatch, index, at);
        }
      }
      records.push_back(rec);
      pos += kCentralSize + name_len + extra_len + comment_len_entry;
    }
  }

  // 6. Local headers.
  for (std::size_t i = 0; i < records.size(); ++i) {
    Record& rec = records[i];
    const std::uint8_t* central = data + rec.central_offset;
    const std::uint64_t at = rec.local_offset;
    if (rec.local_offset > size || size - rec.local_offset < kLocalSize) {
      return fail(ZipVerdict::BadLocalHeader, i, at);
    }
    const std::uint8_t* l = data + rec.local_offset;
    if (rd32(l) != kLocalSig) return fail(ZipVerdict::BadLocalHeader, i, at);
    const std::size_t lname = rd16(l + 26);
    const std::size_t lextra = rd16(l + 28);
    if (lname > limits.max_name_bytes || lextra > limits.max_extra_bytes ||
        kLocalSize + lname + lextra > size - rec.local_offset) {
      return fail(ZipVerdict::BadLocalHeader, i, at);
    }
    const ExtraInfo extra = scan_extra(l + kLocalSize + lname, lextra);
    if (extra.broken || extra.zip64_blocks > 1) return fail(ZipVerdict::BadLocalHeader, i, at);
    std::uint64_t lcompressed = rd32(l + 18);
    std::uint64_t luncompressed = rd32(l + 22);
    const bool sent_c = lcompressed == kSentinel32;
    const bool sent_u = luncompressed == kSentinel32;
    if (sent_c != sent_u) return fail(ZipVerdict::BadLocalHeader, i, at);
    if (sent_c) {
      if (extra.zip64_blocks != 1 || extra.zip64_len != 16) {
        return fail(ZipVerdict::BadLocalHeader, i, at);
      }
      luncompressed = rd64(extra.zip64);
      lcompressed = rd64(extra.zip64 + 8);
      if (luncompressed < kSentinel32 || lcompressed < kSentinel32) {
        return fail(ZipVerdict::BadLocalHeader, i, at);
      }
    } else if (extra.zip64_blocks != 0) {
      return fail(ZipVerdict::BadLocalHeader, i, at);
    }
    if (lname != rec.name_len ||
        std::memcmp(l + kLocalSize, central + kCentralSize, rec.name_len) != 0 ||
        rd16(l + 6) != rec.flags || rd16(l + 8) != rec.method || rd32(l + 14) != rec.crc ||
        rd16(l + 10) != rec.time || rd16(l + 12) != rec.date || lcompressed != rec.compressed ||
        luncompressed != rec.uncompressed) {
      return fail(ZipVerdict::HeaderMismatch, i, at);
    }
    rec.data_offset = rec.local_offset + kLocalSize + lname + lextra;
  }

  // 7. Sizes.
  std::uint64_t sum_compressed = 0;
  std::uint64_t sum_uncompressed = 0;
  for (std::size_t i = 0; i < records.size(); ++i) {
    Record& rec = records[i];
    const std::uint64_t at = rec.local_offset;
    if ((rec.method == 0 && rec.compressed != rec.uncompressed) ||
        (rec.method == 8 && rec.compressed == 0)) {
      return fail(ZipVerdict::BadSizes, i, at);
    }
    if (rec.compressed > size - rec.data_offset) return fail(ZipVerdict::BadSizes, i, at);
    rec.data_end = rec.data_offset + rec.compressed;
    if (rec.compressed > kMax64 - sum_compressed || rec.uncompressed > kMax64 - sum_uncompressed) {
      return fail(ZipVerdict::BadSizes, i, at);
    }
    sum_compressed += rec.compressed;
    sum_uncompressed += rec.uncompressed;
  }

  // 8. Layout in central order.
  if (records.empty()) {
    if (cd_off != 0) return fail(ZipVerdict::PrependedData, 0, cd_off);
  } else {
    if (records[0].local_offset != 0) {
      return fail(ZipVerdict::PrependedData, 0, records[0].local_offset);
    }
    std::uint64_t prev_end = 0;
    for (std::size_t i = 0; i < records.size(); ++i) {
      const Record& rec = records[i];
      if (i > 0) {
        if (rec.local_offset <= records[i - 1].local_offset) {
          return fail(ZipVerdict::OutOfOrder, i, rec.local_offset);
        }
        if (rec.local_offset < prev_end) return fail(ZipVerdict::Overlap, i, rec.local_offset);
        if (rec.local_offset > prev_end) return fail(ZipVerdict::Gap, i, prev_end);
      }
      if (rec.data_end > cd_off) return fail(ZipVerdict::Overlap, i, rec.local_offset);
      prev_end = rec.data_end;
    }
    if (prev_end < cd_off) return fail(ZipVerdict::Gap, records.size() - 1, prev_end);
  }

  ZipAuditResult result;
  result.verdict = ZipVerdict::Ok;
  for (const Record& rec : records) {
    if (rec.is_dir) {
      ++result.directory_count;
      continue;
    }
    ZipEntry e;
    const std::uint8_t* central = data + rec.central_offset;
    e.name.assign(reinterpret_cast<const char*>(central + kCentralSize), rec.name_len);
    e.method = rec.method;
    e.crc32 = rec.crc;
    e.compressed_bytes = rec.compressed;
    e.uncompressed_bytes = rec.uncompressed;
    e.local_header_offset = rec.local_offset;
    e.data_offset = rec.data_offset;
    result.entries.push_back(std::move(e));
  }
  return result;
}

std::vector<EntryInfo> to_entry_infos(const std::vector<ZipEntry>& entries) {
  std::vector<EntryInfo> out;
  out.reserve(entries.size());
  for (const ZipEntry& e : entries) {
    EntryInfo info;
    info.name = e.name;
    info.compressed_bytes = e.compressed_bytes;
    info.uncompressed_bytes = e.uncompressed_bytes;
    out.push_back(std::move(info));
  }
  return out;
}

const char* to_string(ZipVerdict v) {
  switch (v) {
    case ZipVerdict::Ok: return "Ok";
    case ZipVerdict::TooLarge: return "TooLarge";
    case ZipVerdict::TooSmall: return "TooSmall";
    case ZipVerdict::NoEndRecord: return "NoEndRecord";
    case ZipVerdict::MultipleEndRecords: return "MultipleEndRecords";
    case ZipVerdict::TrailingData: return "TrailingData";
    case ZipVerdict::PrependedData: return "PrependedData";
    case ZipVerdict::MultiDisk: return "MultiDisk";
    case ZipVerdict::Zip64Mismatch: return "Zip64Mismatch";
    case ZipVerdict::TooManyEntries: return "TooManyEntries";
    case ZipVerdict::CentralDirectoryBounds: return "CentralDirectoryBounds";
    case ZipVerdict::BadCentralHeader: return "BadCentralHeader";
    case ZipVerdict::BadLocalHeader: return "BadLocalHeader";
    case ZipVerdict::HeaderMismatch: return "HeaderMismatch";
    case ZipVerdict::Encrypted: return "Encrypted";
    case ZipVerdict::UnsupportedFeature: return "UnsupportedFeature";
    case ZipVerdict::UnsupportedMethod: return "UnsupportedMethod";
    case ZipVerdict::SpecialFile: return "SpecialFile";
    case ZipVerdict::BadSizes: return "BadSizes";
    case ZipVerdict::Overlap: return "Overlap";
    case ZipVerdict::Gap: return "Gap";
    case ZipVerdict::OutOfOrder: return "OutOfOrder";
  }
  return "Unknown";
}

}  // namespace lumora::package
