// UN SAUT DE L'APERÇU D'UNE ADRESSE : un GET WinHTTP, sans redirection suivie.

#include "web/win_http_get.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/ascii.h"
#include "ro/core/web_address.h"

#include "web/preview_get.h"
#include "web/win_http_handle.h"

namespace moonclient {
namespace {

using web::Handle;
using web::Mishap;
using web::Widened;

constexpr std::string_view kSecureScheme = "https";

// LE NOM QUE LE SITE VOIT. Un nom qui ne ressemble pas à un navigateur se fait
// refuser par bien des hébergeurs d'images ; on reste pourtant IDENTIFIABLE
// plutôt que de se déguiser : « Mozilla/5.0 (compatible; …) » est la forme d'un
// client honnête, et l'adresse dit à un administrateur à qui il a affaire.
constexpr const wchar_t* kUserAgent =
    L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) (compatible; moonclient/1.0; "
    L"+https://moonlight-destiny.fr)";

// Les statuts qui annoncent une redirection : seuls ceux-là rendent leur
// destination.
constexpr int kHttpFirstRedirect = 300;
constexpr int kHttpLastRedirect = 399;

// Ce qu'on lit d'un en-tête de réponse. Une adresse de destination plus longue
// est une réponse qu'on ne suit pas.
constexpr DWORD kMostHeaderBytes = 8U * 1024U;

[[nodiscard]] std::string Narrowed(std::wstring_view wide) {
  if (wide.empty()) {
    return {};
  }
  const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
  if (needed <= 0) {
    return {};
  }
  std::string narrow(static_cast<std::size_t>(needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), narrow.data(), needed,
                      nullptr, nullptr);
  return narrow;
}

// UN EN-TÊTE DE RÉPONSE EN TEXTE, ou rien s'il manque ou déborde.
[[nodiscard]] std::string HeaderText(HINTERNET call, DWORD which) {
  DWORD size_bytes = 0;
  WinHttpQueryHeaders(call, which, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER,
                      &size_bytes, WINHTTP_NO_HEADER_INDEX);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size_bytes == 0 ||
      size_bytes > kMostHeaderBytes) {
    return {};
  }
  std::wstring text(size_bytes / sizeof(wchar_t), L'\0');
  if (WinHttpQueryHeaders(call, which, WINHTTP_HEADER_NAME_BY_INDEX, text.data(), &size_bytes,
                          WINHTTP_NO_HEADER_INDEX) == FALSE) {
    return {};
  }
  text.resize(size_bytes / sizeof(wchar_t));
  return Narrowed(text);
}

}  // namespace

std::optional<WinHttpTarget> CrackJudgedUrl(std::string_view url) {
  // 🔴 HTTPS OU RIEN, avant le premier octet, et sans casse : la boucle des
  // sauts l'a déjà vérifié, et ce transport ne compte pas sur elle.
  const ro::core::WebAddressParts judged = ro::core::ParseWebAddress(url);
  if (!judged.valid || judged.scheme != kSecureScheme) {
    return std::nullopt;
  }

  const std::wstring wide_url = Widened(url);
  // Des tampons à la taille de l'adresse : aucun morceau ne peut la dépasser.
  std::wstring host(wide_url.size() + 1, L'\0');
  std::wstring path(wide_url.size() + 1, L'\0');
  std::wstring extra(wide_url.size() + 1, L'\0');
  URL_COMPONENTS parts = {};
  parts.dwStructSize = sizeof(parts);
  parts.lpszHostName = host.data();
  parts.dwHostNameLength = static_cast<DWORD>(host.size());
  parts.lpszUrlPath = path.data();
  parts.dwUrlPathLength = static_cast<DWORD>(path.size());
  parts.lpszExtraInfo = extra.data();
  parts.dwExtraInfoLength = static_cast<DWORD>(extra.size());
  if (WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &parts) == FALSE ||
      parts.nScheme != INTERNET_SCHEME_HTTPS) {
    return std::nullopt;
  }
  host.resize(parts.dwHostNameLength);
  path.resize(parts.dwUrlPathLength);
  extra.resize(parts.dwExtraInfoLength);

  const std::string cracked_host = ro::core::AsciiLower(Narrowed(host));
  if (judged.host != cracked_host) {
    return std::nullopt;
  }
  // Le fragment reste chez nous : il ne part jamais sur le réseau.
  if (const std::size_t fragment = extra.find(L'#'); fragment != std::wstring::npos) {
    extra.resize(fragment);
  }
  WinHttpTarget target;
  target.host = std::move(host);
  target.port = parts.nPort;
  target.path_and_query = (path.empty() ? std::wstring(L"/") : path) + extra;
  return target;
}

ro::core::PreviewReply PreviewGet(std::string_view url, std::size_t most_bytes) {
  using Clock = std::chrono::steady_clock;
  const Clock::time_point started = Clock::now();
  const auto hop_over = [&] {
    return Clock::now() - started > std::chrono::milliseconds(ro::core::kPreviewMostHopMs);
  };

  ro::core::PreviewReply reply;
  const std::optional<WinHttpTarget> target = CrackJudgedUrl(url);
  if (!target.has_value()) {
    reply.error = "aperçu refusé : adresse illisible, hors HTTPS ou à l'hôte ambigu";
    return reply;
  }

  Handle session(WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                             WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session) {
    reply.error = Mishap(GetLastError());
    return reply;
  }
  WinHttpSetTimeouts(session.get(), ro::core::kPreviewResolveTimeoutMs,
                     ro::core::kPreviewConnectTimeoutMs, ro::core::kPreviewSendTimeoutMs,
                     ro::core::kPreviewReceiveTimeoutMs);

  Handle link(WinHttpConnect(session.get(), target->host.c_str(), target->port, 0));
  if (!link) {
    reply.error = Mishap(GetLastError());
    return reply;
  }
  Handle call(WinHttpOpenRequest(link.get(), L"GET", target->path_and_query.c_str(), nullptr,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 WINHTTP_FLAG_SECURE));
  if (!call) {
    reply.error = Mishap(GetLastError());
    return reply;
  }
  // 🔴🔴 AUCUNE REDIRECTION SUIVIE PAR WINHTTP : il les suit seul par défaut, et
  // une adresse admise qui répond « 302 vers evil.tld » livrerait en silence ce
  // que la liste blanche protège. La destination remonte à la boucle des sauts,
  // qui la rejuge.
  DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
  if (WinHttpSetOption(call.get(), WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)) ==
      FALSE) {
    reply.error = Mishap(GetLastError());
    return reply;
  }
  // ⚠ AUCUN DRAPEAU DE SÉCURITÉ N'EST POSÉ : la validation du certificat par
  // défaut est celle qu'on veut.

  // Sans `Accept`, WinHTTP n'en envoie aucun, et un serveur qui négocie peut
  // rendre autre chose qu'une image.
  const std::wstring accept = L"Accept: " + Widened(ro::core::kPreviewAccept);
  if (WinHttpSendRequest(call.get(), accept.c_str(), static_cast<DWORD>(accept.size()),
                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0) == FALSE ||
      WinHttpReceiveResponse(call.get(), nullptr) == FALSE) {
    reply.error = Mishap(GetLastError());
    return reply;
  }

  DWORD status = 0;
  DWORD status_size = sizeof(status);
  WinHttpQueryHeaders(call.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
  reply.status = static_cast<int>(status);
  reply.content_type = HeaderText(call.get(), WINHTTP_QUERY_CONTENT_TYPE);
  if (reply.status >= kHttpFirstRedirect && reply.status <= kHttpLastRedirect) {
    reply.location = HeaderText(call.get(), WINHTTP_QUERY_LOCATION);
    return reply;
  }
  if (reply.status != ro::core::kHttpOk) {
    return reply;
  }
  std::uint64_t announced = 0;
  DWORD announced_size = sizeof(announced);
  if (WinHttpQueryHeaders(call.get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64,
                          WINHTTP_HEADER_NAME_BY_INDEX, &announced, &announced_size,
                          WINHTTP_NO_HEADER_INDEX) != FALSE) {
    reply.declared_length = announced;
  }
  // Une PAGE ne pèse jamais plus que ce que la boucle des sauts lit d'une page :
  // on n'en télécharge pas huit mégaoctets pour les refuser ensuite.
  // ⚠ `(std::min)` : <windows.h> fait de `min` une macro.
  const std::size_t most_body =
      ro::core::ClassifyContentType(reply.content_type) == ro::core::PreviewBody::kHtml
          ? (std::min)(most_bytes, ro::core::kPreviewMostHtmlBytes)
          : most_bytes;
  // Une taille annoncée au-delà de la borne : rendue sans corps, la boucle
  // refuse sans qu'un octet ait été lu.
  if (reply.declared_length > most_body) {
    return reply;
  }

  std::vector<char> chunk;
  while (true) {
    // 🔴 LE TEMPS DU SAUT EST BORNÉ, pas seulement celui de chaque lecture : un
    // serveur qui rend un octet juste avant chaque délai tiendrait le fil
    // indéfiniment.
    if (hop_over()) {
      reply.error = "aperçu refusé : le site répond trop lentement";
      reply.body.clear();
      return reply;
    }
    DWORD available = 0;
    // 🔴 UNE ERREUR N'EST PAS LA FIN DU CORPS : une image tronquée se rendrait
    // comme entière.
    if (WinHttpQueryDataAvailable(call.get(), &available) == FALSE) {
      reply.error = Mishap(GetLastError());
      reply.body.clear();
      return reply;
    }
    if (available == 0) {
      break;
    }
    // 🔴 LA BORNE MORD SUR LE FLUX, avant que les octets ne s'accumulent : une
    // réponse qui ment sur sa taille, ou n'en dit rien, ne remplit pas la
    // mémoire.
    if (reply.body.size() + available > most_body) {
      reply.error = "aperçu refusé : la réponse dépasse la taille permise";
      reply.body.clear();
      return reply;
    }
    chunk.resize(available);
    DWORD read = 0;
    if (WinHttpReadData(call.get(), chunk.data(), available, &read) == FALSE) {
      reply.error = Mishap(GetLastError());
      reply.body.clear();
      return reply;
    }
    reply.body.append(chunk.data(), read);
  }
  return reply;
}

}  // namespace moonclient
