#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "ro/core/error.h"

namespace moonclient::patcher {

inline constexpr std::string_view kInstallLockFileName = "moonclient-patch.lock";

// LE VERROU D'UNE INSTALLATION PENDANT QU'ON LA PATCHE.
//
// Un fichier que l'on tient en exclusivité — ouvert SANS PARTAGE sous Windows,
// sous `flock` exclusif sous Linux : un second client qui tente de le prendre
// essuie un refus, et sait qu'un autre patche déjà ce dossier.
//
// 🔴 C'EST LE SYSTÈME QUI LE RELÂCHE, pas nous : il ferme la poignée à la fin
// du processus, même tué. Un verrou qu'on effacerait soi-même survivrait à un
// plantage et bloquerait le patch de toutes les parties suivantes.
class InstallLock {
 public:
  [[nodiscard]] static ro::core::Result<InstallLock> Take(const std::filesystem::path& root);

  InstallLock(const InstallLock&) = delete;
  InstallLock& operator=(const InstallLock&) = delete;
  InstallLock(InstallLock&& other) noexcept;
  InstallLock& operator=(InstallLock&& other) noexcept;
  ~InstallLock();

 private:
  // ⚠ « AUCUNE POIGNÉE » VAUT -1 DES DEUX CÔTÉS : c'est `INVALID_HANDLE_VALUE`
  // sous Windows, et le descripteur invalide sous Linux.
  static constexpr std::intptr_t kNoHandle = -1;

  explicit InstallLock(std::intptr_t handle) : handle_(handle) {}
  void Release() noexcept;

  std::intptr_t handle_ = kNoHandle;
};

}  // namespace moonclient::patcher
