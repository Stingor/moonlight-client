#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::core {

// UNE SORTIE D'OCTETS, miroir de `ByteSource`.
//
// C'est ce qui permet à une couche qui ÉCRIT un format de ne rien savoir du
// disque : elle écrit ici, et l'appelant décide où cela va — un fichier, ou un
// tampon en mémoire pour un essai.
//
// ⚠ ON AJOUTE À LA FIN, et on ne revient en arrière que pour RÉÉCRIRE des
// octets déjà écrits — l'en-tête d'une archive, qui ne se connaît qu'une fois
// tout le reste posé.
class ByteSink {
 public:
  virtual ~ByteSink() = default;
  ByteSink(const ByteSink&) = delete;
  ByteSink& operator=(const ByteSink&) = delete;
  ByteSink(ByteSink&&) = delete;
  ByteSink& operator=(ByteSink&&) = delete;

  [[nodiscard]] virtual Result<void> Append(std::span<const std::uint8_t> bytes) = 0;
  // Réécrit à `offset` des octets DÉJÀ écrits. Écrire au-delà est une erreur.
  [[nodiscard]] virtual Result<void> Overwrite(std::uint64_t offset,
                                               std::span<const std::uint8_t> bytes) = 0;
  [[nodiscard]] virtual std::uint64_t size() const = 0;

 protected:
  ByteSink() = default;
};

// En mémoire : pour les essais, et pour ce qui tient dans un tampon.
class MemoryByteSink final : public ByteSink {
 public:
  MemoryByteSink() = default;

  [[nodiscard]] Result<void> Append(std::span<const std::uint8_t> bytes) override;
  [[nodiscard]] Result<void> Overwrite(std::uint64_t offset,
                                       std::span<const std::uint8_t> bytes) override;
  [[nodiscard]] std::uint64_t size() const override { return bytes_.size(); }

  [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }

 private:
  std::vector<std::uint8_t> bytes_;
};

// Dans un fichier, créé ou vidé à l'ouverture.
//
// ⚠ `Close` DIT SI TOUT EST ARRIVÉ SUR LE DISQUE. Un flux qui échoue à la
// fermeture — disque plein au dernier tampon — a perdu la fin du fichier ;
// sans cet appel, rien ne le dirait.
class FileByteSink final : public ByteSink {
 public:
  [[nodiscard]] static Result<std::unique_ptr<FileByteSink>> Create(
      const std::filesystem::path& path);

  [[nodiscard]] Result<void> Append(std::span<const std::uint8_t> bytes) override;
  [[nodiscard]] Result<void> Overwrite(std::uint64_t offset,
                                       std::span<const std::uint8_t> bytes) override;
  [[nodiscard]] std::uint64_t size() const override { return size_; }

  [[nodiscard]] Result<void> Close();

 private:
  FileByteSink(std::filesystem::path path, std::ofstream stream)
      : path_(std::move(path)), stream_(std::move(stream)) {}

  [[nodiscard]] Result<void> Broken() const;

  std::filesystem::path path_;
  std::ofstream stream_;
  std::uint64_t size_ = 0;
};

}  // namespace ro::core
