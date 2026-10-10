// LA REQUÊTE DU LOGIN WEB PAR WINHTTP.

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "web/http_transport.h"
#include "web/win_http_handle.h"

namespace moonclient::web::detail {
namespace {

// Le nom que le site voit dans son journal. Il sert à distinguer nos appels de
// ceux des autres clients, qui frappent à la même porte.
constexpr const wchar_t* kUserAgent = L"moonclient";

}  // namespace

WebReply FetchOnce(const std::string& origin, WebScheme scheme, const WebRequest& request) {
  WebReply reply;
  bool secure = false;
  if (!SchemeAllowed(origin, scheme, secure)) {
    reply.error = "l'adresse du site doit commencer par https://";
    return reply;
  }

  const std::wstring url = Widened(origin + request.path);
  std::array<wchar_t, 256> host = {};
  std::array<wchar_t, 2048> path = {};
  URL_COMPONENTS parts = {};
  parts.dwStructSize = sizeof(parts);
  parts.lpszHostName = host.data();
  parts.dwHostNameLength = static_cast<DWORD>(host.size());
  parts.lpszUrlPath = path.data();
  parts.dwUrlPathLength = static_cast<DWORD>(path.size());
  if (WinHttpCrackUrl(url.c_str(), 0, 0, &parts) == FALSE) {
    reply.error = "l'adresse du site n'est pas lisible";
    return reply;
  }

  Handle session(WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                             WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session) {
    reply.error = Mishap(GetLastError());
    return reply;
  }
  WinHttpSetTimeouts(session.get(), kWebResolveTimeoutMs, kWebConnectTimeoutMs, kWebSendTimeoutMs,
                     kWebReceiveTimeoutMs);

  Handle link(WinHttpConnect(session.get(), host.data(), parts.nPort, 0));
  if (!link) {
    reply.error = Mishap(GetLastError());
    return reply;
  }

  Handle call(WinHttpOpenRequest(link.get(), L"POST", path.data(), nullptr, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
  if (!call) {
    reply.error = Mishap(GetLastError());
    return reply;
  }
  // ⚠ AUCUN DRAPEAU DE SÉCURITÉ N'EST POSÉ ICI, et c'est délibéré : la
  // validation par défaut est celle qu'on veut. Voir l'en-tête.

  std::string body;
  std::wstring header;
  if (request.parts.empty()) {
    body = FormBody(request);
    header = L"Content-Type: application/x-www-form-urlencoded";
  } else {
    MultipartBody multipart = BuildMultipartBody(request.parts);
    body = std::move(multipart.body);
    header = L"Content-Type: multipart/form-data; boundary=" + Widened(multipart.boundary);
  }
  if (WinHttpSendRequest(call.get(), header.c_str(), static_cast<DWORD>(-1),
                         const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
                         static_cast<DWORD>(body.size()), 0) == FALSE ||
      WinHttpReceiveResponse(call.get(), nullptr) == FALSE) {
    reply.error = Mishap(GetLastError());
    return reply;
  }

  DWORD status = 0;
  DWORD status_size = sizeof(status);
  WinHttpQueryHeaders(call.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
  reply.status = static_cast<int>(status);

  while (true) {
    DWORD available = 0;
    // 🔴 UNE ERREUR N'EST PAS LA FIN DU CORPS : les confondre rendrait un corps
    // tronqué comme une réponse complète, et l'appelant le croirait.
    if (WinHttpQueryDataAvailable(call.get(), &available) == FALSE) {
      reply.error = Mishap(GetLastError());
      return reply;
    }
    if (available == 0) {
      break;
    }
    // 🔴 LA BORNE DE TAILLE MORD ICI, sur le flux : une réponse sans fin
    // remplirait la mémoire, et rien au-dessus ne le verrait.
    if (reply.body.size() + available > kWebBodyLimit) {
      reply.error = "le site a répondu bien plus que ce qu'une réponse pèse";
      return reply;
    }
    std::vector<char> chunk(available);
    DWORD read = 0;
    if (WinHttpReadData(call.get(), chunk.data(), available, &read) == FALSE) {
      reply.error = Mishap(GetLastError());
      return reply;
    }
    reply.body.append(chunk.data(), read);
  }
  return reply;
}

}  // namespace moonclient::web::detail
