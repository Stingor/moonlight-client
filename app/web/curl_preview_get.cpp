// UN SAUT DE L'APERÇU D'UNE ADRESSE : un GET libcurl, sans redirection suivie.

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/web_address.h"

#include "web/curl_handle.h"
#include "web/preview_get.h"

namespace moonclient {
namespace {

using web::CurlEasy;
using web::CurlHeaders;
using web::CurlMishap;
using web::CurlStatus;

constexpr std::string_view kSecureScheme = "https";

// LE NOM QUE LE SITE VOIT. Un nom qui ne ressemble pas à un navigateur se fait
// refuser par bien des hébergeurs d'images ; on reste pourtant IDENTIFIABLE
// plutôt que de se déguiser : « Mozilla/5.0 (compatible; …) » est la forme d'un
// client honnête, et l'adresse dit à un administrateur à qui il a affaire.
constexpr const char* kUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64) (compatible; moonclient/1.0; +https://moonlight-destiny.fr)";

// Les statuts qui annoncent une redirection : seuls ceux-là rendent leur
// destination.
constexpr int kHttpFirstRedirect = 300;
constexpr int kHttpLastRedirect = 399;

// L'en-tête de destination, tel que le serveur l'écrit.
constexpr std::string_view kLocationHeader = "location:";

// Ce qu'on lit d'un en-tête de réponse. Une adresse de destination plus longue
// est une réponse qu'on ne suit pas.
constexpr std::size_t kMostHeaderBytes = 8U * 1024U;

struct UrlFree {
  void operator()(CURLU* url) const { curl_url_cleanup(url); }
};
using ParsedUrl = std::unique_ptr<CURLU, UrlFree>;

struct TextFree {
  void operator()(char* text) const { curl_free(text); }
};
using CurlText = std::unique_ptr<char, TextFree>;

// 🔴 L'ADRESSE DÉCOUPÉE PAR LIBCURL, SEULEMENT SI SON HÔTE EST CELUI QUE LA
// LISTE BLANCHE A JUGÉ. Deux lecteurs d'adresse divergent sur les formes
// tordues ; quand ils divergent, rien ne part. Et c'est CE découpage-là qui
// part sur le réseau (`CURLOPT_CURLU`), pas une seconde lecture de la chaîne.
[[nodiscard]] ParsedUrl JudgedUrl(std::string_view url) {
  const ro::core::WebAddressParts judged = ro::core::ParseWebAddress(url);
  if (!judged.valid || judged.scheme != kSecureScheme) {
    return nullptr;
  }
  ParsedUrl parsed(curl_url());
  if (!parsed) {
    return nullptr;
  }
  const std::string text(url);
  if (curl_url_set(parsed.get(), CURLUPART_URL, text.c_str(), 0) != CURLUE_OK) {
    return nullptr;
  }
  char* scheme = nullptr;
  char* host = nullptr;
  if (curl_url_get(parsed.get(), CURLUPART_SCHEME, &scheme, 0) != CURLUE_OK) {
    return nullptr;
  }
  const CurlText held_scheme(scheme);
  if (curl_url_get(parsed.get(), CURLUPART_HOST, &host, 0) != CURLUE_OK) {
    return nullptr;
  }
  const CurlText held_host(host);
  if (ro::core::AsciiLower(held_scheme.get()) != kSecureScheme ||
      ro::core::AsciiLower(held_host.get()) != judged.host) {
    return nullptr;
  }
  return parsed;
}

// CE QUE LES RAPPELS DE LIBCURL REMPLISSENT.
struct Transfer {
  CURL* handle = nullptr;
  std::size_t most_bytes = 0;
  ro::core::PreviewReply reply;
  // Vrai dès le premier octet du corps : la décision de le lire est prise.
  bool judged = false;
  // Le corps n'est pas lu, sans que ce soit une panne : redirection, statut
  // autre que 200, taille annoncée au-delà de la borne.
  bool declined = false;
  bool overflowed = false;
};

std::size_t TakeHeader(char* data, std::size_t size, std::size_t count, void* user) {
  auto* transfer = static_cast<Transfer*>(user);
  const std::size_t bytes = size * count;
  const std::string_view line(data, bytes);
  if (line.size() > kLocationHeader.size() && line.size() <= kMostHeaderBytes &&
      ro::core::AsciiLower(line.substr(0, kLocationHeader.size())) == kLocationHeader) {
    std::string_view value = line.substr(kLocationHeader.size());
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
      value.remove_prefix(1);
    }
    while (!value.empty() &&
           (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) {
      value.remove_suffix(1);
    }
    transfer->reply.location = std::string(value);
  }
  return bytes;
}

// AU PREMIER OCTET DU CORPS, les en-têtes sont tous là : on décide de le lire.
void JudgeBody(Transfer& transfer) {
  transfer.judged = true;
  ro::core::PreviewReply& reply = transfer.reply;
  reply.status = CurlStatus(transfer.handle);
  char* content_type = nullptr;
  if (curl_easy_getinfo(transfer.handle, CURLINFO_CONTENT_TYPE, &content_type) == CURLE_OK &&
      content_type != nullptr) {
    reply.content_type = content_type;
  }
  if (reply.status != ro::core::kHttpOk) {
    transfer.declined = true;
    return;
  }
  curl_off_t announced = -1;
  if (curl_easy_getinfo(transfer.handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &announced) ==
          CURLE_OK &&
      announced >= 0) {
    reply.declared_length = static_cast<std::uint64_t>(announced);
  }
  // Une PAGE ne pèse jamais plus que ce que la boucle des sauts lit d'une page :
  // on n'en télécharge pas huit mégaoctets pour les refuser ensuite.
  if (ro::core::ClassifyContentType(reply.content_type) == ro::core::PreviewBody::kHtml) {
    transfer.most_bytes = std::min(transfer.most_bytes, ro::core::kPreviewMostHtmlBytes);
  }
  // Une taille annoncée au-delà de la borne : rendue sans corps, la boucle
  // refuse sans qu'un octet ait été gardé.
  if (reply.declared_length > transfer.most_bytes) {
    transfer.declined = true;
  }
}

// 🔴 LA BORNE MORD SUR LE FLUX, avant que les octets ne s'accumulent : une
// réponse qui ment sur sa taille, ou n'en dit rien, ne remplit pas la mémoire.
// Rendre moins que ce qu'on a reçu interrompt le transfert.
std::size_t TakeBody(char* data, std::size_t size, std::size_t count, void* user) {
  auto* transfer = static_cast<Transfer*>(user);
  const std::size_t bytes = size * count;
  if (!transfer->judged) {
    JudgeBody(*transfer);
  }
  if (transfer->declined) {
    return 0;
  }
  if (transfer->reply.body.size() + bytes > transfer->most_bytes) {
    transfer->overflowed = true;
    return 0;
  }
  transfer->reply.body.append(data, bytes);
  return bytes;
}

}  // namespace

ro::core::PreviewReply PreviewGet(std::string_view url, std::size_t most_bytes) {
  ro::core::PreviewReply refused;
  const ParsedUrl target = JudgedUrl(url);
  if (!target) {
    refused.error = "aperçu refusé : adresse illisible, hors HTTPS ou à l'hôte ambigu";
    return refused;
  }
  CurlEasy call;
  if (!call) {
    refused.error = "aperçu refusé : le réseau est indisponible (libcurl)";
    return refused;
  }
  CURL* handle = call.get();
  curl_easy_setopt(handle, CURLOPT_CURLU, target.get());
  curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "https");
  // 🔴🔴 AUCUNE REDIRECTION SUIVIE PAR LIBCURL : une adresse admise qui répond
  // « 302 vers evil.tld » livrerait en silence ce que la liste blanche protège.
  // La destination remonte à la boucle des sauts, qui la rejuge.
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);
  // ⚠ AUCUN ASSOUPLISSEMENT DE LA VALIDATION DU CERTIFICAT.
  curl_easy_setopt(handle, CURLOPT_USERAGENT, kUserAgent);
  curl_easy_setopt(
      handle, CURLOPT_CONNECTTIMEOUT_MS,
      static_cast<long>(ro::core::kPreviewResolveTimeoutMs + ro::core::kPreviewConnectTimeoutMs));
  // 🔴 LE TEMPS DU SAUT EST BORNÉ, pas seulement celui de chaque lecture : un
  // serveur qui rend un octet juste avant chaque délai tiendrait le fil
  // indéfiniment.
  curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, static_cast<long>(ro::core::kPreviewMostHopMs));
  curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);

  CurlHeaders headers;
  // Sans `Accept`, un serveur qui négocie peut rendre autre chose qu'une image.
  if (!headers.Add("Accept: " + std::string(ro::core::kPreviewAccept))) {
    refused.error = "aperçu refusé : mémoire insuffisante";
    return refused;
  }
  curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers.get());

  Transfer transfer;
  transfer.handle = handle;
  transfer.most_bytes = most_bytes;
  curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, &TakeHeader);
  curl_easy_setopt(handle, CURLOPT_HEADERDATA, &transfer);
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &TakeBody);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &transfer);

  const CURLcode done = curl_easy_perform(handle);
  // Une réponse sans corps n'a jamais appelé le rappel du corps.
  if (!transfer.judged && done == CURLE_OK) {
    JudgeBody(transfer);
  }
  ro::core::PreviewReply& reply = transfer.reply;
  const bool redirected = reply.status >= kHttpFirstRedirect && reply.status <= kHttpLastRedirect;
  if (!redirected) {
    reply.location.clear();
  }
  if (transfer.overflowed) {
    reply.error = "aperçu refusé : la réponse dépasse la taille permise";
    reply.body.clear();
    return reply;
  }
  if (transfer.declined) {
    reply.body.clear();
    return reply;
  }
  // 🔴 UNE ERREUR N'EST PAS LA FIN DU CORPS : une image tronquée se rendrait
  // comme entière.
  if (done != CURLE_OK) {
    reply.error = done == CURLE_OPERATION_TIMEDOUT
                      ? std::string("aperçu refusé : le site répond trop lentement")
                      : CurlMishap(done);
    reply.body.clear();
    return reply;
  }
  return reply;
}

}  // namespace moonclient
