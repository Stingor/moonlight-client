#include "ro/core/byte_sink.h"

#include <algorithm>
#include <string>
#include <utility>

namespace ro::core {

Result<void> MemoryByteSink::Append(std::span<const std::uint8_t> bytes) {
  bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
  return {};
}

Result<void> MemoryByteSink::Overwrite(std::uint64_t offset, std::span<const std::uint8_t> bytes) {
  if (offset > bytes_.size() || bytes.size() > bytes_.size() - offset) {
    return Fail(ErrorCode::kInvalidArgument, "réécriture au-delà des octets écrits");
  }
  std::ranges::copy(bytes, bytes_.begin() + static_cast<std::ptrdiff_t>(offset));
  return {};
}

Result<std::unique_ptr<FileByteSink>> FileByteSink::Create(const std::filesystem::path& path) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return Fail(ErrorCode::kIoFailure, "impossible de créer " + path.string());
  }
  return std::unique_ptr<FileByteSink>(new FileByteSink(path, std::move(stream)));
}

Result<void> FileByteSink::Broken() const {
  return Fail(ErrorCode::kIoFailure, "écriture impossible dans " + path_.string());
}

Result<void> FileByteSink::Append(std::span<const std::uint8_t> bytes) {
  stream_.seekp(0, std::ios::end);
  stream_.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
  if (!stream_) {
    return Broken();
  }
  size_ += bytes.size();
  return {};
}

Result<void> FileByteSink::Overwrite(std::uint64_t offset, std::span<const std::uint8_t> bytes) {
  if (offset > size_ || bytes.size() > size_ - offset) {
    return Fail(ErrorCode::kInvalidArgument, "réécriture au-delà des octets écrits");
  }
  stream_.seekp(static_cast<std::streamoff>(offset));
  stream_.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
  if (!stream_) {
    return Broken();
  }
  return {};
}

Result<void> FileByteSink::Close() {
  stream_.close();
  if (!stream_) {
    return Broken();
  }
  return {};
}

}  // namespace ro::core
