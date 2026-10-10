#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::core {

// Source d'octets adressable, sans supposer d'où ils viennent.
//
// C'est cette abstraction qui rend les parseurs de `formats` testables et
// fuzzables : un test construit une archive synthétique EN MÉMOIRE et la parse,
// sans fixture sur disque et sans dépendre d'un GRF réel que l'on n'a pas le
// droit de versionner. Le fuzzer, lui, alimente directement un
// MemoryByteSource.
class ByteSource {
 public:
  virtual ~ByteSource() = default;
  ByteSource(const ByteSource&) = delete;
  ByteSource& operator=(const ByteSource&) = delete;
  ByteSource(ByteSource&&) = delete;
  ByteSource& operator=(ByteSource&&) = delete;

  [[nodiscard]] virtual std::uint64_t size() const = 0;

  // Lit EXACTEMENT out.size() octets à `offset`. Faux si la plage sort du
  // support ou si le support a échoué — jamais de lecture partielle silencieuse.
  [[nodiscard]] virtual bool Read(std::uint64_t offset, std::span<std::uint8_t> out) = 0;

  // Alloue puis lit. `count` est confronté à la taille du support AVANT
  // l'allocation : une taille corrompue dans un en-tête ne doit pas se traduire
  // par une demande de plusieurs gibioctets.
  [[nodiscard]] Result<std::vector<std::uint8_t>> ReadBlock(std::uint64_t offset,
                                                            std::uint64_t count);

 protected:
  ByteSource() = default;
};

// Source possédant ses octets. Possédante et non une vue : une vue invite au
// dangling dès qu'un parseur survit au tampon qui l'a nourri.
class MemoryByteSource final : public ByteSource {
 public:
  explicit MemoryByteSource(std::vector<std::uint8_t> bytes) noexcept : bytes_(std::move(bytes)) {}

  [[nodiscard]] std::uint64_t size() const override { return bytes_.size(); }
  [[nodiscard]] bool Read(std::uint64_t offset, std::span<std::uint8_t> out) override;

 private:
  std::vector<std::uint8_t> bytes_;
};

// Source sur fichier. Le flux reste ouvert : un GRF sert des milliers de
// lectures, le rouvrir à chaque fois coûterait plus que tout le reste.
class FileByteSource final : public ByteSource {
 public:
  [[nodiscard]] static Result<std::unique_ptr<FileByteSource>> Open(
      const std::filesystem::path& path);

  [[nodiscard]] std::uint64_t size() const override { return size_; }
  [[nodiscard]] bool Read(std::uint64_t offset, std::span<std::uint8_t> out) override;

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

 private:
  FileByteSource(std::filesystem::path path, std::ifstream stream, std::uint64_t size)
      : path_(std::move(path)), stream_(std::move(stream)), size_(size) {}

  std::filesystem::path path_;
  std::ifstream stream_;
  std::uint64_t size_ = 0;
};

}  // namespace ro::core
