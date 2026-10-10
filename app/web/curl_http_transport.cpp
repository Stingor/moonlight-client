// LA REQUÊTE DU LOGIN WEB PAR LIBCURL.

#include <cstddef>
#include <string>
#include <utility>

#include "web/curl_handle.h"
#include "web/http_transport.h"

namespace moonclient::web::detail {
namespace {

// Le nom que le site voit dans son journal. Il sert à distinguer nos appels de
// ceux des autres clients, qui frappent à la même porte.
constexpr const char* kUserAgent = "moonclient";

// Ce que le fil de lecture remplit.
struct Sink {
  std::string body;
  bool overflowed = false;
};

// 🔴 LA BORNE DE TAILLE MORD ICI, sur le flux : une réponse sans fin
// remplirait la mémoire, et rien au-dessus ne le verrait. Rendre moins que ce
// qu'on a reçu interrompt le transfert.
std::size_t Collect(char* data, std::size_t size, std::size_t count, void* user) {
  auto* sink = static_cast<Sink*>(user);
  const std::size_t bytes = size * count;
  if (sink->body.size() + bytes > kWebBodyLimit) {
    sink->overflowed = true;
    return 0;
  }
  sink->body.append(data, bytes);
  return bytes;
}

}  // namespace

WebReply FetchOnce(const std::string& origin, WebScheme scheme, const WebRequest& request) {
  WebReply reply;
  bool secure = false;
  if (!SchemeAllowed(origin, scheme, secure)) {
    reply.error = "l'adresse du site doit commencer par https://";
    return reply;
  }

  CurlEasy call;
  if (!call) {
    reply.error = "le réseau est indisponible (libcurl)";
    return reply;
  }
  const std::string url = origin + request.path;
  CURL* handle = call.get();
  curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(handle, CURLOPT_USERAGENT, kUserAgent);
  // 🔴 LE PROTOCOLE EST CELUI QUE L'ORIGINE A NOMMÉ, et aucune redirection ne
  // le change : le site du login ne redirige pas, et suivre un 302 vers du
  // clair y mettrait le mot de passe.
  curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, secure ? "https" : "http");
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);
  // ⚠ AUCUN ASSOUPLISSEMENT DE LA VALIDATION : `CURLOPT_SSL_VERIFYPEER` et
  // `CURLOPT_SSL_VERIFYHOST` gardent leur valeur par défaut, qui est stricte.
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(kWebConnectTimeoutMs));
  curl_easy_setopt(
      handle, CURLOPT_TIMEOUT_MS,
      static_cast<long>(kWebResolveTimeoutMs + kWebSendTimeoutMs + kWebReceiveTimeoutMs));
  // Un fil détaché ne doit jamais recevoir de signal de la bibliothèque.
  curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);

  std::string body;
  CurlHeaders headers;
  if (request.parts.empty()) {
    body = FormBody(request);
    if (!headers.Add("Content-Type: application/x-www-form-urlencoded")) {
      reply.error = "mémoire insuffisante pour la requête";
      return reply;
    }
  } else {
    MultipartBody multipart = BuildMultipartBody(request.parts);
    body = std::move(multipart.body);
    if (!headers.Add("Content-Type: multipart/form-data; boundary=" + multipart.boundary)) {
      reply.error = "mémoire insuffisante pour la requête";
      return reply;
    }
  }
  curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers.get());
  curl_easy_setopt(handle, CURLOPT_POST, 1L);
  curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.data());
  curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));

  Sink sink;
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &Collect);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &sink);

  const CURLcode done = curl_easy_perform(handle);
  if (sink.overflowed) {
    reply.error = "le site a répondu bien plus que ce qu'une réponse pèse";
    return reply;
  }
  // 🔴 UNE ERREUR N'EST PAS LA FIN DU CORPS : un corps tronqué ne se rend
  // jamais comme une réponse complète.
  if (done != CURLE_OK) {
    reply.error = CurlMishap(done);
    return reply;
  }
  reply.status = CurlStatus(handle);
  reply.body = std::move(sink.body);
  return reply;
}

}  // namespace moonclient::web::detail
