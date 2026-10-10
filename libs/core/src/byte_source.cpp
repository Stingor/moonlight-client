#include "ro/core/byte_source.h"

#include <cstring>
#include <system_error>
#include <utility>

namespace ro::core {

Result<std::vector<std::uint8_t>> ByteSource::ReadBlock(std::uint64_t offset, std::uint64_t count) {
  const std::uint64_t total = size();
  if (offset > total || count > total - offset) {
    return Fail(ErrorCode::kUnexpectedEnd, "plage " + std::to_string(offset) + "+" +
                                               std::to_string(count) + " hors du support de " +
                                               std::to_string(total) + " octets");
  }
  std::vector<std::uint8_t> block(static_cast<std::size_t>(count));
  if (!count) {
    return block;
  }
  if (!Read(offset, block)) {
    return Fail(ErrorCode::kIoFailure,
                "lecture de " + std::to_string(count) + " octets à " + std::to_string(offset));
  }
  return block;
}

bool MemoryByteSource::Read(std::uint64_t offset, std::span<std::uint8_t> out) {
  const std::uint64_t total = bytes_.size();
  if (offset > total || out.size() > total - offset) {
    return false;
  }
  if (!out.empty()) {
    std::memcpy(out.data(), bytes_.data() + offset, out.size());
  }
  return true;
}

Result<std::unique_ptr<FileByteSource>> FileByteSource::Open(const std::filesystem::path& path) {
  std::error_code code;
  const std::uintmax_t file_size = std::filesystem::file_size(path, code);
  if (code) {
    return Fail(ErrorCode::kIoFailure, path.string() + " : " + code.message());
  }

  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return Fail(ErrorCode::kIoFailure, path.string() + " : ouverture impossible");
  }

  // Le constructeur est privé : make_unique ne peut pas l'appeler.
  return std::unique_ptr<FileByteSource>(
      new FileByteSource(path, std::move(stream), static_cast<std::uint64_t>(file_size)));
}

bool FileByteSource::Read(std::uint64_t offset, std::span<std::uint8_t> out) {
  if (offset > size_ || out.size() > size_ - offset) {
    return false;
  }
  if (out.empty()) {
    return true;
  }
  stream_.clear();
  stream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!stream_) {
    return false;
  }
  stream_.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
  return stream_.gcount() == static_cast<std::streamsize>(out.size());
}

}  // namespace ro::core
