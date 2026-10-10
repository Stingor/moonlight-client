#pragma once

// CE QUE PARTAGENT LES CLIENTS WINHTTP DU DÉPÔT : une poignée qui se referme,
// le passage en UTF-16, et le motif d'un échec.

#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

// clang-format off
#include <windows.h>
#include <winhttp.h>
// clang-format on

namespace moonclient::web {

[[nodiscard]] inline std::wstring Widened(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  const int needed =
      MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), needed);
  return wide;
}

// Le motif d'un échec de WinHTTP, en clair. ⚠ AVEC SON NUMÉRO : les libellés
// que le système rend pour ces codes sont en anglais et souvent vides ; le
// numéro est ce qui permet de nous le rapporter.
[[nodiscard]] inline std::string Mishap(unsigned long code) {
  std::array<char, 64> text = {};
  std::snprintf(text.data(), text.size(), "le site est injoignable (erreur %lu)", code);
  return text.data();
}

// Tient une poignée WinHTTP, et la referme quoi qu'il arrive.
class Handle {
 public:
  Handle() = default;
  explicit Handle(HINTERNET handle) : handle_(handle) {}
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  Handle(Handle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  Handle& operator=(Handle&& other) noexcept {
    if (this != &other) {
      Close();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  ~Handle() { Close(); }

  [[nodiscard]] HINTERNET get() const noexcept { return handle_; }
  explicit operator bool() const noexcept { return handle_ != nullptr; }

 private:
  void Close() {
    if (handle_ != nullptr) {
      WinHttpCloseHandle(handle_);
      handle_ = nullptr;
    }
  }
  HINTERNET handle_ = nullptr;
};

}  // namespace moonclient::web
