// LE TRANSPORT DU LOGIN WEB : ce qui ne dépend d'aucune plateforme — le fil
// détaché, le casier, le corps du formulaire et le refus du clair.

#include "web/http_transport.h"

#include <string_view>
#include <thread>
#include <utility>

namespace moonclient::web {
namespace {

constexpr std::string_view kSecureScheme = "https://";
constexpr std::string_view kPlainScheme = "http://";

// CE QUI N'A PAS BESOIN D'ÊTRE ÉCHAPPÉ dans un corps de formulaire (RFC 3986,
// « unreserved »).
[[nodiscard]] bool Plain(char letter) {
  return (letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z') ||
         (letter >= '0' && letter <= '9') || letter == '-' || letter == '_' || letter == '.' ||
         letter == '~';
}

// ⚠ TOUT LE RESTE PASSE EN POUR-CENT, y compris ce qu'on croit sans danger :
// un mot de passe contient des `&` et des `=`, et les laisser passer
// couperait le champ en deux — le site lirait un mot de passe tronqué et
// refuserait, sur un mot de passe juste.
[[nodiscard]] std::string FormEncoded(std::string_view text) {
  static constexpr std::string_view kHex = "0123456789ABCDEF";
  std::string out;
  out.reserve(text.size());
  for (const char letter : text) {
    if (Plain(letter)) {
      out.push_back(letter);
      continue;
    }
    if (letter == ' ') {
      out.push_back('+');
      continue;
    }
    const auto byte = static_cast<unsigned char>(letter);
    out.push_back('%');
    out.push_back(kHex[byte >> 4U]);
    out.push_back(kHex[byte & 0x0FU]);
  }
  return out;
}

}  // namespace

namespace detail {

std::string FormBody(const WebRequest& request) {
  std::string body;
  for (const auto& field : request.fields) {
    if (!body.empty()) {
      body.push_back('&');
    }
    body += FormEncoded(field.first);
    body.push_back('=');
    body += FormEncoded(field.second);
  }
  return body;
}

bool SchemeAllowed(std::string_view origin, WebScheme scheme, bool& secure) {
  // 🔴 HTTPS OU RIEN : un repli sur du clair mettrait le mot de passe du forum
  // sur le fil. Seul le transport qui a DEMANDÉ le clair l'obtient.
  secure = origin.starts_with(kSecureScheme);
  const bool plain_allowed = scheme == WebScheme::kPlainAllowed && origin.starts_with(kPlainScheme);
  return secure || plain_allowed;
}

}  // namespace detail

void HttpTransport::Begin(const WebRequest& request) {
  if (pending_ != nullptr) {
    return;
  }
  pending_ = std::make_shared<Slot>();
  // ⚠ LE FIL PREND SA PROPRE COPIE DE TOUT : le casier par pointeur compté,
  // l'origine et la requête par valeur. Rien de ce qu'il touche n'appartient à
  // l'appelant, qui peut disparaître pendant qu'il travaille.
  std::thread([slot = pending_, origin = origin_, scheme = scheme_, request]() {
    WebReply reply = detail::FetchOnce(origin, scheme, request);
    {
      const std::lock_guard<std::mutex> held(slot->lock);
      slot->reply = std::move(reply);
    }
    slot->done.store(true, std::memory_order_release);
  }).detach();
}

std::optional<WebReply> HttpTransport::Take() {
  if (pending_ == nullptr || !pending_->done.load(std::memory_order_acquire)) {
    return std::nullopt;
  }
  WebReply reply;
  {
    const std::lock_guard<std::mutex> held(pending_->lock);
    reply = std::move(pending_->reply);
  }
  pending_.reset();
  return reply;
}

void HttpTransport::Abandon() {
  // ⚠ ON LÂCHE LE CASIER, ON N'ATTEND PAS : le fil finit dans sa propre copie,
  // qui meurt avec lui. Attendre figerait la fermeture du client jusqu'au
  // délai de garde.
  pending_.reset();
}

}  // namespace moonclient::web
