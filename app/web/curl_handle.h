#pragma once

// CE QUE PARTAGENT LES CLIENTS LIBCURL DU DÉPÔT : une poignée qui se referme,
// l'initialisation unique de la bibliothèque, et le motif d'un échec.
//
// ⚠ LA LIBCURL DE LA DISTRIBUTION, pas une copie livrée : sa TLS et son magasin
// de certificats sont ceux du système, comme ceux de WinHTTP sous Windows.

#include <mutex>
#include <string>
#include <utility>

#include <curl/curl.h>

namespace moonclient::web {

// 🔴 `curl_global_init` UNE FOIS, AVANT TOUT FIL : la bibliothèque ne promet
// pas qu'un premier appel concurrent soit sûr. `call_once` le garantit quel que
// soit le fil qui ouvre la première requête.
inline void EnsureCurlReady() {
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

// Le motif d'un échec de libcurl, en clair. ⚠ AVEC SON NUMÉRO : c'est lui qui
// permet de nous le rapporter, le libellé étant en anglais.
[[nodiscard]] inline std::string CurlMishap(CURLcode code) {
  return "le site est injoignable (curl " + std::to_string(static_cast<int>(code)) + " : " +
         curl_easy_strerror(code) + ")";
}

// Tient une poignée libcurl, et la referme quoi qu'il arrive.
class CurlEasy {
 public:
  CurlEasy() {
    EnsureCurlReady();
    handle_ = curl_easy_init();
  }
  CurlEasy(const CurlEasy&) = delete;
  CurlEasy& operator=(const CurlEasy&) = delete;
  CurlEasy(CurlEasy&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  CurlEasy& operator=(CurlEasy&& other) noexcept {
    if (this != &other) {
      Close();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }
  ~CurlEasy() { Close(); }

  [[nodiscard]] CURL* get() const noexcept { return handle_; }
  explicit operator bool() const noexcept { return handle_ != nullptr; }

 private:
  void Close() {
    if (handle_ != nullptr) {
      curl_easy_cleanup(handle_);
      handle_ = nullptr;
    }
  }
  CURL* handle_ = nullptr;
};

// Tient une liste d'en-têtes, et la libère quoi qu'il arrive.
class CurlHeaders {
 public:
  CurlHeaders() = default;
  CurlHeaders(const CurlHeaders&) = delete;
  CurlHeaders& operator=(const CurlHeaders&) = delete;
  CurlHeaders(CurlHeaders&&) = delete;
  CurlHeaders& operator=(CurlHeaders&&) = delete;
  ~CurlHeaders() { curl_slist_free_all(list_); }

  // Faux si la mémoire manque : la requête ne doit alors pas partir sans lui.
  [[nodiscard]] bool Add(const std::string& line) {
    curl_slist* grown = curl_slist_append(list_, line.c_str());
    if (grown == nullptr) {
      return false;
    }
    list_ = grown;
    return true;
  }
  [[nodiscard]] curl_slist* get() const noexcept { return list_; }

 private:
  curl_slist* list_ = nullptr;
};

// Le code HTTP de la dernière réponse, ou zéro.
[[nodiscard]] inline int CurlStatus(CURL* handle) {
  long status = 0;
  curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
  return static_cast<int>(status);
}

}  // namespace moonclient::web
