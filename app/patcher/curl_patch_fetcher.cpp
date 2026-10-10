// LE RÉSEAU DU PATCHER : un GET libcurl, lu par morceaux.

#include <cstdint>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "patcher/http_patch_fetcher.h"
#include "web/curl_handle.h"

namespace moonclient::patcher {
namespace {

using web::CurlEasy;
using web::CurlMishap;
using web::CurlStatus;

constexpr std::string_view kSecureScheme = "https://";
constexpr const char* kUserAgent = "moonclient-patcher";
constexpr int kHttpOk = 200;

// Les délais de garde. La réception se juge PAR DÉBIT : une archive de cent
// mégaoctets n'a pas à tenir en quinze secondes, mais un serveur muet trente
// secondes d'affilée est tombé.
constexpr long kConnectTimeoutMs = 20'000;
constexpr long kStalledAfterSeconds = 30;
constexpr long kStalledBelowBytesPerSecond = 1;

// Les redirections suivies, toutes en HTTPS : un miroir de téléchargement en
// demande parfois une ou deux.
constexpr long kMostRedirects = 10;

// Le suffixe du fichier en cours d'écriture.
constexpr std::string_view kPartialSuffix = ".part";

[[nodiscard]] std::unexpected<ro::core::Error> Refused(const std::string& url,
                                                       const std::string& why) {
  return ro::core::Fail(ro::core::ErrorCode::kIoFailure, url + " : " + why);
}

// CE QUE LES RAPPELS DE LIBCURL TIENNENT ET REMPLISSENT.
struct Transfer {
  CURL* handle = nullptr;
  std::uint64_t limit_bytes = 0;
  std::stop_token stop;
  const FetchProgressSink* on_progress = nullptr;
  std::function<bool(std::string_view)> consume;

  FetchProgress progress;
  bool judged = false;
  int status = 0;
  // Ce qui a interrompu le transfert, en clair, ou vide.
  std::string refusal;
};

// AU PREMIER OCTET DU CORPS, les en-têtes sont tous là.
void JudgeBody(Transfer& transfer) {
  transfer.judged = true;
  transfer.status = CurlStatus(transfer.handle);
  if (transfer.status != kHttpOk) {
    transfer.refusal = "le serveur répond " + std::to_string(transfer.status);
    return;
  }
  curl_off_t announced = -1;
  if (curl_easy_getinfo(transfer.handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &announced) ==
          CURLE_OK &&
      announced >= 0) {
    transfer.progress.announced_bytes = static_cast<std::uint64_t>(announced);
    if (*transfer.progress.announced_bytes > transfer.limit_bytes) {
      transfer.refusal = "le fichier annoncé dépasse la taille permise";
    }
  }
}

// 🔴 LA BORNE MORD SUR LE FLUX, avant que les octets ne s'accumulent. Rendre
// moins que ce qu'on a reçu interrompt le transfert.
std::size_t TakeBody(char* data, std::size_t size, std::size_t count, void* user) {
  auto* transfer = static_cast<Transfer*>(user);
  const std::size_t bytes = size * count;
  if (!transfer->judged) {
    JudgeBody(*transfer);
  }
  if (!transfer->refusal.empty()) {
    return 0;
  }
  if (transfer->progress.received_bytes + bytes > transfer->limit_bytes) {
    transfer->refusal = "le fichier dépasse la taille permise";
    return 0;
  }
  if (!transfer->consume(std::string_view(data, bytes))) {
    return 0;
  }
  transfer->progress.received_bytes += bytes;
  if (transfer->on_progress != nullptr && *transfer->on_progress) {
    (*transfer->on_progress)(transfer->progress);
  }
  return bytes;
}

// Appelé régulièrement pendant le transfert : c'est là que l'arrêt se voit.
int Watch(void* user, curl_off_t /*download_total*/, curl_off_t /*download_now*/,
          curl_off_t /*upload_total*/, curl_off_t /*upload_now*/) {
  const auto* transfer = static_cast<const Transfer*>(user);
  return transfer->stop.stop_requested() ? 1 : 0;
}

// Le GET entier, le corps passé à `consume` morceau par morceau.
//
// 🔴 UNE ERREUR N'EST PAS LA FIN DU CORPS : les confondre rendrait un fichier
// tronqué comme un fichier complet.
[[nodiscard]] ro::core::Result<void> Get(const std::string& url, std::uint64_t limit_bytes,
                                         std::stop_token stop, const FetchProgressSink& on_progress,
                                         std::function<bool(std::string_view)> consume,
                                         const std::function<std::string()>& consume_failure) {
  // 🔴 HTTPS OU RIEN, avant le premier octet.
  if (!std::string_view(url).starts_with(kSecureScheme)) {
    return Refused(url, "l'adresse doit commencer par https://");
  }
  CurlEasy call;
  if (!call) {
    return Refused(url, "le réseau est indisponible (libcurl)");
  }
  CURL* handle = call.get();
  Transfer transfer;
  transfer.handle = handle;
  transfer.limit_bytes = limit_bytes;
  transfer.stop = stop;
  transfer.on_progress = &on_progress;
  transfer.consume = std::move(consume);

  curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(handle, CURLOPT_USERAGENT, kUserAgent);
  curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "https");
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(handle, CURLOPT_MAXREDIRS, kMostRedirects);
  // ⚠ UNE REDIRECTION VERS DU CLAIR EST REFUSÉE.
  curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, "https");
  // ⚠ AUCUN ASSOUPLISSEMENT DE LA VALIDATION DU CERTIFICAT.
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, kConnectTimeoutMs);
  curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, kStalledAfterSeconds);
  curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, kStalledBelowBytesPerSecond);
  curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &TakeBody);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &transfer);
  curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, &Watch);
  curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &transfer);
  curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);

  const CURLcode done = curl_easy_perform(handle);
  if (stop.stop_requested()) {
    return Refused(url, "interrompu");
  }
  if (!transfer.refusal.empty()) {
    return Refused(url, transfer.refusal);
  }
  if (done == CURLE_WRITE_ERROR) {
    return Refused(url, consume_failure());
  }
  if (done != CURLE_OK) {
    return Refused(url, CurlMishap(done));
  }
  // Une réponse sans corps n'a jamais appelé le rappel du corps.
  if (!transfer.judged) {
    JudgeBody(transfer);
    if (!transfer.refusal.empty()) {
      return Refused(url, transfer.refusal);
    }
  }
  if (transfer.progress.announced_bytes.has_value() &&
      transfer.progress.received_bytes != *transfer.progress.announced_bytes) {
    return Refused(url, "la réponse est incomplète");
  }
  return {};
}

}  // namespace

ro::core::Result<std::string> HttpPatchFetcher::FetchText(const std::string& url,
                                                          std::uint64_t limit_bytes,
                                                          std::stop_token stop) {
  std::string body;
  auto read = Get(
      url, limit_bytes, stop, FetchProgressSink(),
      [&body](std::string_view piece) {
        body.append(piece);
        return true;
      },
      [] { return std::string("mémoire insuffisante"); });
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
  std::filesystem::path partial = destination;
  partial += kPartialSuffix;
  auto written = [&]() -> ro::core::Result<void> {
    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!out) {
      return Refused(url, "impossible d'écrire " + partial.string());
    }
    auto read = Get(
        url, limit_bytes, stop, on_progress,
        [&out](std::string_view piece) {
          out.write(piece.data(), static_cast<std::streamsize>(piece.size()));
          return static_cast<bool>(out);
        },
        [&partial] { return "écriture interrompue sur " + partial.string(); });
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
  // ⚠ LE FICHIER N'APPARAÎT QU'ENTIER : le nom attendu ne se pose qu'une fois
  // le dernier octet reçu.
  std::error_code renamed;
  std::filesystem::rename(partial, destination, renamed);
  if (renamed) {
    std::filesystem::remove(partial, ignored);
    return Refused(url, "impossible de nommer " + destination.string() + " : " + renamed.message());
  }
  return {};
}

}  // namespace moonclient::patcher
