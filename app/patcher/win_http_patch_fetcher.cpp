// LE RÉSEAU DU PATCHER : un GET WinHTTP, lu par morceaux.

#include <array>
#include <cstdio>
#include <fstream>
#include <string_view>
#include <system_error>
#include <vector>

#include "patcher/http_patch_fetcher.h"
#include "web/win_http_handle.h"

namespace moonclient::patcher {
namespace {

using web::Handle;
using web::Mishap;
using web::Widened;

constexpr std::string_view kSecureScheme = "https://";
constexpr const wchar_t* kUserAgent = L"moonclient-patcher";
constexpr int kHttpOk = 200;
constexpr std::size_t kHostCapacity = 256;
constexpr std::size_t kPathCapacity = 2048;

// Les délais de garde, en millisecondes. La réception se compte PAR MORCEAU :
// une archive de cent mégaoctets n'a pas à tenir en quinze secondes, mais un
// serveur muet quinze secondes d'affilée est tombé.
constexpr int kResolveTimeoutMs = 10'000;
constexpr int kConnectTimeoutMs = 10'000;
constexpr int kSendTimeoutMs = 15'000;
constexpr int kReceiveTimeoutMs = 30'000;

// Le suffixe du fichier en cours d'écriture.
constexpr std::wstring_view kPartialSuffix = L".part";

[[nodiscard]] std::unexpected<ro::core::Error> Refused(const std::string& url,
                                                       const std::string& why) {
  return ro::core::Fail(ro::core::ErrorCode::kIoFailure, url + " : " + why);
}

[[nodiscard]] std::unexpected<ro::core::Error> Stopped(const std::string& url) {
  return ro::core::Fail(ro::core::ErrorCode::kIoFailure, url + " : interrompu");
}

// Une requête ouverte, sa réponse reçue et son statut vérifié.
struct OpenedGet {
  Handle session;
  Handle link;
  Handle call;
  std::optional<std::uint64_t> announced_bytes;
};

[[nodiscard]] ro::core::Result<OpenedGet> OpenGet(const std::string& url) {
  // 🔴 HTTPS OU RIEN, avant le premier octet.
  if (!std::string_view(url).starts_with(kSecureScheme)) {
    return Refused(url, "l'adresse doit commencer par https://");
  }
  const std::wstring wide_url = Widened(url);
  std::array<wchar_t, kHostCapacity> host = {};
  std::array<wchar_t, kPathCapacity> path = {};
  URL_COMPONENTS parts = {};
  parts.dwStructSize = sizeof(parts);
  parts.lpszHostName = host.data();
  parts.dwHostNameLength = static_cast<DWORD>(host.size());
  parts.lpszUrlPath = path.data();
  parts.dwUrlPathLength = static_cast<DWORD>(path.size());
  if (WinHttpCrackUrl(wide_url.c_str(), 0, 0, &parts) == FALSE) {
    return Refused(url, "adresse illisible");
  }

  OpenedGet opened;
  opened.session = Handle(WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!opened.session) {
    return Refused(url, Mishap(GetLastError()));
  }
  WinHttpSetTimeouts(opened.session.get(), kResolveTimeoutMs, kConnectTimeoutMs, kSendTimeoutMs,
                     kReceiveTimeoutMs);
  opened.link = Handle(WinHttpConnect(opened.session.get(), host.data(), parts.nPort, 0));
  if (!opened.link) {
    return Refused(url, Mishap(GetLastError()));
  }
  opened.call =
      Handle(WinHttpOpenRequest(opened.link.get(), L"GET", path.data(), nullptr, WINHTTP_NO_REFERER,
                                WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  if (!opened.call) {
    return Refused(url, Mishap(GetLastError()));
  }
  if (WinHttpSendRequest(opened.call.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0) == FALSE ||
      WinHttpReceiveResponse(opened.call.get(), nullptr) == FALSE) {
    return Refused(url, Mishap(GetLastError()));
  }

  DWORD status = 0;
  DWORD status_size = sizeof(status);
  WinHttpQueryHeaders(opened.call.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
  if (static_cast<int>(status) != kHttpOk) {
    return Refused(url, "le serveur répond " + std::to_string(status));
  }
  std::uint64_t length = 0;
  DWORD length_size = sizeof(length);
  if (WinHttpQueryHeaders(
          opened.call.get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64,
          WINHTTP_HEADER_NAME_BY_INDEX, &length, &length_size, WINHTTP_NO_HEADER_INDEX) != FALSE) {
    opened.announced_bytes = length;
  }
  return opened;
}

// Lit tout le corps, morceau par morceau, en le passant à `consume`.
//
// 🔴 UNE ERREUR N'EST PAS LA FIN DU CORPS : les confondre rendrait un fichier
// tronqué comme un fichier complet. Et la borne mord sur le flux, avant que les
// octets ne s'accumulent.
template <typename Consume>
[[nodiscard]] ro::core::Result<void> ReadBody(const std::string& url, OpenedGet& opened,
                                              std::uint64_t limit_bytes, std::stop_token stop,
                                              const FetchProgressSink& on_progress,
                                              Consume&& consume) {
  if (opened.announced_bytes.has_value() && *opened.announced_bytes > limit_bytes) {
    return Refused(url, "le fichier annoncé dépasse la taille permise");
  }
  FetchProgress progress;
  progress.announced_bytes = opened.announced_bytes;
  std::vector<char> chunk;
  while (true) {
    if (stop.stop_requested()) {
      return Stopped(url);
    }
    DWORD available = 0;
    if (WinHttpQueryDataAvailable(opened.call.get(), &available) == FALSE) {
      return Refused(url, Mishap(GetLastError()));
    }
    if (available == 0) {
      break;
    }
    if (progress.received_bytes + available > limit_bytes) {
      return Refused(url, "le fichier dépasse la taille permise");
    }
    chunk.resize(available);
    DWORD read = 0;
    if (WinHttpReadData(opened.call.get(), chunk.data(), available, &read) == FALSE) {
      return Refused(url, Mishap(GetLastError()));
    }
    if (auto kept = consume(std::string_view(chunk.data(), read)); !kept) {
      return kept;
    }
    progress.received_bytes += read;
    if (on_progress) {
      on_progress(progress);
    }
  }
  if (opened.announced_bytes.has_value() && progress.received_bytes != *opened.announced_bytes) {
    return Refused(url, "la réponse est incomplète");
  }
  return {};
}

}  // namespace

ro::core::Result<std::string> HttpPatchFetcher::FetchText(const std::string& url,
                                                          std::uint64_t limit_bytes,
                                                          std::stop_token stop) {
  auto opened = OpenGet(url);
  if (!opened) {
    return std::unexpected(opened.error());
  }
  std::string body;
  auto read = ReadBody(url, *opened, limit_bytes, stop, FetchProgressSink(),
                       [&](std::string_view piece) -> ro::core::Result<void> {
                         body.append(piece);
                         return {};
                       });
  if (!read) {
    return std::unexpected(read.error());
  }
  return body;
}

ro::core::Result<void> HttpPatchFetcher::FetchToFile(const std::string& url,
                                                     const std::filesystem::path& destination,
                                                     std::uint64_t limit_bytes,
                                                     const FetchProgressSink& on_progress,
                                                     std::stop_token stop) {
  auto opened = OpenGet(url);
  if (!opened) {
    return std::unexpected(opened.error());
  }
  std::filesystem::path partial = destination;
  partial += kPartialSuffix;
  auto written = [&]() -> ro::core::Result<void> {
    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!out) {
      return Refused(url, "impossible d'écrire " + partial.string());
    }
    auto read = ReadBody(url, *opened, limit_bytes, stop, on_progress,
                         [&](std::string_view piece) -> ro::core::Result<void> {
                           out.write(piece.data(), static_cast<std::streamsize>(piece.size()));
                           if (!out) {
                             return Refused(url, "écriture interrompue sur " + partial.string());
                           }
                           return {};
                         });
    if (!read) {
      return read;
    }
    out.close();
    if (!out) {
      return Refused(url, "écriture interrompue sur " + partial.string());
    }
    return {};
  }();
  std::error_code ignored;
  if (!written) {
    std::filesystem::remove(partial, ignored);
    return written;
  }
  std::error_code renamed;
  std::filesystem::rename(partial, destination, renamed);
  if (renamed) {
    std::filesystem::remove(partial, ignored);
    return Refused(url, "impossible de nommer " + destination.string() + " : " + renamed.message());
  }
  return {};
}

}  // namespace moonclient::patcher
