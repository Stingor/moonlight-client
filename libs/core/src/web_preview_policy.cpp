#include "ro/core/web_preview_policy.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/web_address.h"

namespace ro::core {
namespace {

// Les statuts qui annoncent une redirection.
constexpr int kHttpFirstRedirect = 300;
constexpr int kHttpLastRedirect = 399;

constexpr std::string_view kImageMediaPrefix = "image/";
constexpr std::string_view kHtmlMedia = "text/html";
constexpr std::string_view kXhtmlMedia = "application/xhtml+xml";

// LA BALISE OPENGRAPH, et les deux variantes qui portent aussi l'adresse de
// l'image. « og:image:width », lui, porte un nombre : le guillemet qui suit
// le nom l'écarte.
constexpr std::string_view kOgImage = "og:image";
constexpr std::array<std::string_view, 3> kOgImageTails = {"", ":url", ":secure_url"};
constexpr std::string_view kMetaTag = "meta";
constexpr std::string_view kContentAttribute = "content";
// La seule entité vraiment courante dans l'adresse d'une og:image.
constexpr std::string_view kAmpersandEntity = "&amp;";

constexpr std::string_view kSchemeSeparator = "://";
constexpr std::string_view kSecureScheme = "https";

[[nodiscard]] char LowerAscii(char letter) {
  return (letter >= 'A' && letter <= 'Z') ? static_cast<char>(letter - 'A' + 'a') : letter;
}

[[nodiscard]] std::string LoweredAscii(std::string_view text) {
  std::string lowered(text);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), LowerAscii);
  return lowered;
}

[[nodiscard]] bool Blank(char letter) {
  return letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r' || letter == '\f';
}

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  while (!text.empty() && Blank(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && Blank(text.back())) {
    text.remove_suffix(1);
  }
  return text;
}

[[nodiscard]] bool Quote(char letter) { return letter == '"' || letter == '\''; }

// UNE ADRESSE QUI A SON SCHÉMA : des lettres, puis « : », avant tout « / »,
// « ? » ou « # ».
[[nodiscard]] bool HasScheme(std::string_view address) {
  const std::size_t colon = address.find(':');
  if (colon == 0 || colon == std::string_view::npos) {
    return false;
  }
  const std::string_view scheme = address.substr(0, colon);
  return std::all_of(scheme.begin(), scheme.end(), [](char letter) {
    const char lower = LowerAscii(letter);
    return (lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9') || letter == '+' ||
           letter == '-' || letter == '.';
  });
}

// LES SEGMENTS « . » ET « .. » D'UN CHEMIN, résolus. Le chemin commence par « / ».
[[nodiscard]] std::string WithoutDotSegments(std::string_view path) {
  std::vector<std::string_view> kept;
  std::string_view rest = path.substr(1);
  bool ends_with_folder = false;
  while (true) {
    const std::size_t slash = rest.find('/');
    const std::string_view segment = rest.substr(0, slash);
    const bool last = slash == std::string_view::npos;
    if (segment == "..") {
      if (!kept.empty()) {
        kept.pop_back();
      }
      ends_with_folder = last;
    } else if (segment == ".") {
      ends_with_folder = last;
    } else {
      kept.push_back(segment);
      ends_with_folder = false;
    }
    if (last) {
      break;
    }
    rest.remove_prefix(slash + 1);
  }
  std::string resolved;
  for (const std::string_view segment : kept) {
    resolved.push_back('/');
    resolved += segment;
  }
  if (ends_with_folder || resolved.empty()) {
    resolved.push_back('/');
  }
  return resolved;
}

// Remplace chaque `&amp;` par `&`.
[[nodiscard]] std::string WithoutAmpersandEntities(std::string_view text) {
  std::string plain;
  while (true) {
    const std::size_t entity = text.find(kAmpersandEntity);
    plain += text.substr(0, entity);
    if (entity == std::string_view::npos) {
      break;
    }
    plain.push_back('&');
    text.remove_prefix(entity + kAmpersandEntity.size());
  }
  return plain;
}

// LA VALEUR DE L'ATTRIBUT `content` d'une balise, lue dans l'ORIGINAL : une
// adresse peut porter une casse qui compte — les jetons de signature d'un CDN.
// `lowered` est la même balise en minuscules, octet pour octet.
[[nodiscard]] std::optional<std::string_view> ContentOf(std::string_view tag,
                                                        std::string_view lowered) {
  std::size_t from = 0;
  while (true) {
    const std::size_t name = lowered.find(kContentAttribute, from);
    if (name == std::string_view::npos) {
      return std::nullopt;
    }
    from = name + kContentAttribute.size();
    // Un nom d'attribut entier, précédé d'un blanc.
    if (name == 0 || !Blank(lowered[name - 1])) {
      continue;
    }
    std::size_t at = from;
    while (at < lowered.size() && Blank(lowered[at])) {
      ++at;
    }
    if (at >= lowered.size() || lowered[at] != '=') {
      continue;
    }
    ++at;
    while (at < lowered.size() && Blank(lowered[at])) {
      ++at;
    }
    if (at >= lowered.size() || !Quote(lowered[at])) {
      continue;
    }
    const char quote = lowered[at];
    const std::size_t closing = lowered.find(quote, at + 1);
    if (closing == std::string_view::npos) {
      return std::nullopt;
    }
    return tag.substr(at + 1, closing - at - 1);
  }
}

// Le nom d'attribut qui suit `og:image` désigne-t-il l'adresse de l'image ?
[[nodiscard]] bool NamesTheImage(std::string_view after_marker) {
  return std::any_of(kOgImageTails.begin(), kOgImageTails.end(), [&](std::string_view tail) {
    return after_marker.size() > tail.size() && after_marker.starts_with(tail) &&
           Quote(after_marker[tail.size()]);
  });
}

[[nodiscard]] PreviewFetched Refused(std::string why) {
  PreviewFetched fetched;
  fetched.failure = std::move(why);
  return fetched;
}

// LES NOMS QUI NE SORTENT PAS DU RÉSEAU LOCAL, par leur dernière étiquette :
// un aperçu n'a rien à y chercher, et une adresse du chat qui les vise sonde la
// machine ou la box du joueur.
constexpr std::array<std::string_view, 7> kPrivateTopLabels = {
    "localhost", "local", "internal", "lan", "home", "corp", "arpa"};
constexpr std::size_t kLeastPublicLabels = 2;

// UN NOM D'HÔTE PUBLIC : au moins deux étiquettes, une dernière qui commence
// par une lettre — ce qui écarte toute adresse IPv4, même écrite en
// hexadécimal —, pas d'IPv6 entre crochets, et aucun domaine privé.
[[nodiscard]] bool PublicHostName(std::string_view host) {
  if (host.empty() || host.front() == '[') {
    return false;
  }
  const std::size_t last_dot = host.rfind('.');
  if (last_dot == std::string_view::npos ||
      static_cast<std::size_t>(std::count(host.begin(), host.end(), '.')) + 1 <
          kLeastPublicLabels) {
    return false;
  }
  const std::string_view top = host.substr(last_dot + 1);
  if (top.empty() || top.front() < 'a' || top.front() > 'z') {
    return false;
  }
  return std::find(kPrivateTopLabels.begin(), kPrivateTopLabels.end(), top) ==
         kPrivateTopLabels.end();
}

// UN OCTET QUI N'A RIEN À FAIRE DANS UNE ADRESSE À TÉLÉCHARGER : un blanc, un
// contrôle, ou DEL. Un « \r\n » dans une destination de redirection écrirait
// un en-tête de plus.
constexpr unsigned char kLastControlByte = 0x20;
constexpr unsigned char kDeleteByte = 0x7F;

[[nodiscard]] bool ControlByte(char letter) {
  const auto byte = static_cast<unsigned char>(letter);
  return byte <= kLastControlByte || byte == kDeleteByte;
}

// Les avertissements qu'une adresse à télécharger ne peut pas porter : un
// « @ » devant l'hôte, et un hôte qui se lit autrement qu'il ne s'écrit.
constexpr auto kRefusedWarnings =
    static_cast<std::uint8_t>(kWebWarnUserInfo | kWebWarnPunycode | kWebWarnNonAsciiHost);

// UNE ADRESSE QUE L'APERÇU ACCEPTE DE CONTACTER, la liste blanche mise à part.
// Rend l'hôte, ou le motif du refus.
struct JudgedAddress {
  std::string host;
  std::string refusal;
};

[[nodiscard]] JudgedAddress JudgeDownload(std::string_view url) {
  JudgedAddress judged;
  if (std::any_of(url.begin(), url.end(), ControlByte)) {
    judged.refusal = "adresse refusée, elle porte un blanc ou un contrôle";
    return judged;
  }
  WebAddressParts parts = ParseWebAddress(url);
  if (!parts.valid || parts.scheme != kSecureScheme) {
    judged.refusal = "adresse refusée, HTTPS seulement : " + std::string(url);
    return judged;
  }
  if ((WebAddressWarnings(url) & kRefusedWarnings) != 0) {
    judged.refusal = "adresse refusée, son hôte est trompeur : " + std::string(url);
    return judged;
  }
  if (!PublicHostName(parts.host)) {
    judged.refusal = "adresse refusée, hôte local ou numérique : " + parts.host;
    return judged;
  }
  judged.host = std::move(parts.host);
  return judged;
}

}  // namespace

// ── LA LISTE BLANCHE ─────────────────────────────────────────────────────────

PreviewAllowList::PreviewAllowList(std::span<const std::string> user_hosts) {
  for (const std::string& listed : user_hosts) {
    if (user_hosts_.size() >= kMostUserHosts) {
      break;
    }
    std::string host = LoweredAscii(Trimmed(listed));
    if (!IsUsableHostRule(host) ||
        std::find(user_hosts_.begin(), user_hosts_.end(), host) != user_hosts_.end()) {
      continue;
    }
    user_hosts_.push_back(std::move(host));
  }
}

bool PreviewAllowList::AllowsHost(std::string_view host) const {
  if (!PublicHostName(host)) {
    return false;
  }
  const auto matches = [&](std::string_view rule) { return HostMatchesRule(host, rule); };
  return std::any_of(kDefaultPreviewHostRules.begin(), kDefaultPreviewHostRules.end(), matches) ||
         std::any_of(user_hosts_.begin(), user_hosts_.end(), matches);
}

bool PreviewAllowList::AllowsUrl(std::string_view openable) const {
  // 🔴 « https://cdn.discordapp.com@evil.tld » : l'œil lit Discord, le réseau
  // contacte evil.tld. La forme est refusée, quel que soit l'hôte — comme un
  // hôte en punycode ou hors ASCII, qui se lit autrement qu'il ne s'écrit.
  const JudgedAddress judged = JudgeDownload(openable);
  return judged.refusal.empty() && AllowsHost(judged.host);
}

void PreviewAllowList::AllowOnce(std::string_view openable) {
  if (AllowedOnce(openable)) {
    return;
  }
  allowed_once_.emplace_back(openable);
  if (allowed_once_.size() > kMostAllowedOnce) {
    allowed_once_.erase(allowed_once_.begin());
  }
}

bool PreviewAllowList::AllowedOnce(std::string_view openable) const {
  return std::find(allowed_once_.begin(), allowed_once_.end(), openable) != allowed_once_.end();
}

bool PreviewAllowList::Previewable(std::string_view openable) const {
  return AllowsUrl(openable) || AllowedOnce(openable);
}

bool IsMirrorPicture(std::string_view url) {
  return url.size() > kMirrorPicturePrefix.size() && url.starts_with(kMirrorPicturePrefix);
}

// ── CE QUE LE SERVEUR A RENDU ────────────────────────────────────────────────

PreviewBody ClassifyContentType(std::string_view content_type) {
  // « Image/PNG; charset=binary » : le type seul, sans ses paramètres ni sa casse.
  const std::string media = LoweredAscii(Trimmed(content_type.substr(0, content_type.find(';'))));
  const std::string_view media_view = media;
  if (media_view.size() > kImageMediaPrefix.size() && media_view.starts_with(kImageMediaPrefix)) {
    return PreviewBody::kImage;
  }
  if (media_view == kHtmlMedia || media_view == kXhtmlMedia) {
    return PreviewBody::kHtml;
  }
  return PreviewBody::kRefused;
}

std::string ResolveLocation(std::string_view base, std::string_view location) {
  location = Trimmed(location);
  if (HasScheme(location)) {
    return std::string(location);
  }
  const std::size_t separator = base.find(kSchemeSeparator);
  if (separator == std::string_view::npos) {
    return std::string(location);
  }
  const std::string_view scheme = base.substr(0, separator);
  if (location.starts_with("//")) {
    return std::string(scheme) + ":" + std::string(location);
  }
  const std::size_t authority_begin = separator + kSchemeSeparator.size();
  const std::size_t authority_end =
      std::min(base.find_first_of("/?#", authority_begin), base.size());
  const std::string_view origin = base.substr(0, authority_end);
  // Le chemin de la base, sans sa requête ni son fragment.
  const std::size_t query = std::min(base.find_first_of("?#", authority_end), base.size());
  std::string_view base_path = base.substr(authority_end, query - authority_end);
  if (base_path.empty()) {
    base_path = "/";
  }

  if (location.empty()) {
    return std::string(base.substr(0, std::min(base.find('#'), base.size())));
  }
  if (location.front() == '#') {
    return std::string(base.substr(0, std::min(base.find('#'), base.size()))) +
           std::string(location);
  }
  if (location.front() == '?') {
    return std::string(origin) + std::string(base_path) + std::string(location);
  }

  // Le chemin seul se résout ; sa requête et son fragment suivent tels quels.
  const std::size_t location_query = std::min(location.find_first_of("?#"), location.size());
  const std::string_view location_path = location.substr(0, location_query);
  std::string merged;
  if (location_path.starts_with('/')) {
    merged = location_path;
  } else {
    merged =
        std::string(base_path.substr(0, base_path.rfind('/') + 1)) + std::string(location_path);
  }
  return std::string(origin) + WithoutDotSegments(merged) +
         std::string(location.substr(location_query));
}

std::optional<std::string> FindOgImage(std::string_view html, std::string_view page_url) {
  // Les balises OpenGraph vivent dans le <head> : la tête de la page suffit, et
  // on ne parcourt pas des centaines de kilo-octets pour une balise.
  const std::string_view head = html.substr(0, kPreviewHtmlScanBytes);
  const std::string lowered = LoweredAscii(head);
  std::size_t from = 0;
  while (true) {
    const std::size_t marker = lowered.find(kOgImage, from);
    if (marker == std::string::npos) {
      return std::nullopt;
    }
    from = marker + kOgImage.size();
    if (!NamesTheImage(std::string_view(lowered).substr(from))) {
      continue;
    }
    // La balise qui porte le marqueur : `content` peut précéder `property`
    // comme le suivre, mais il est dans la même <meta>.
    const std::size_t open = lowered.rfind('<', marker);
    const std::size_t close = lowered.find('>', marker);
    if (open == std::string::npos || close == std::string::npos) {
      continue;
    }
    const std::string_view lowered_tag = std::string_view(lowered).substr(open, close - open);
    std::string_view name = lowered_tag.substr(1);
    while (!name.empty() && Blank(name.front())) {
      name.remove_prefix(1);
    }
    if (!name.starts_with(kMetaTag)) {
      continue;
    }
    const std::optional<std::string_view> content =
        ContentOf(head.substr(open, close - open), lowered_tag);
    if (!content.has_value()) {
      continue;
    }
    const std::string address = WithoutAmpersandEntities(Trimmed(*content));
    if (address.empty()) {
      continue;
    }
    return ResolveLocation(page_url, address);
  }
}

// ── LA BOUCLE DES SAUTS ──────────────────────────────────────────────────────

PreviewFetched FetchPreviewBytes(std::string_view url, const PreviewAllowList& allow,
                                 const PreviewGet& get) {
  // Un accord pour cette fois vaut pour l'HÔTE de l'adresse accordée : ses
  // sauts y restent, ou vont vers un hôte de la liste.
  const bool consented = allow.AllowedOnce(url);
  const std::string consented_host = consented ? JudgeDownload(url).host : std::string();
  std::string current(url);
  int redirects = 0;
  bool page_followed = false;
  while (true) {
    // 🔴 À CHAQUE SAUT, et pas seulement au premier : une adresse admise qui
    // répond « 302 vers evil.tld » livrerait sinon exactement ce que la liste
    // protège.
    const JudgedAddress judged = JudgeDownload(current);
    if (!judged.refusal.empty()) {
      return Refused(judged.refusal);
    }
    const bool same_as_consented = consented && judged.host == consented_host;
    if (!same_as_consented && !allow.AllowsHost(judged.host)) {
      return Refused("hôte hors de la liste : " + judged.host);
    }

    const PreviewReply reply = get(current, kPreviewMostBytes);
    if (!reply.error.empty()) {
      return Refused(reply.error);
    }
    if (reply.status >= kHttpFirstRedirect && reply.status <= kHttpLastRedirect) {
      if (reply.location.empty()) {
        return Refused("redirection sans destination : " + current);
      }
      if (redirects >= kPreviewMostRedirects) {
        return Refused("trop de redirections : " + std::string(url));
      }
      ++redirects;
      current = ResolveLocation(current, reply.location);
      continue;
    }
    if (reply.status != kHttpOk) {
      return Refused("le serveur répond " + std::to_string(reply.status) + " : " + current);
    }
    if (reply.declared_length > kPreviewMostBytes || reply.body.size() > kPreviewMostBytes) {
      return Refused("réponse trop lourde : " + current);
    }

    switch (ClassifyContentType(reply.content_type)) {
      case PreviewBody::kImage: {
        PreviewFetched fetched;
        fetched.ok = true;
        fetched.final_url = std::move(current);
        fetched.body = reply.body;
        return fetched;
      }
      case PreviewBody::kHtml: {
        // UNE PAGE, et non un fichier : la forme des hébergeurs de GIF. Son
        // og:image se suit UNE fois, jamais de page en page.
        if (page_followed) {
          return Refused("l'image de la page est encore une page : " + current);
        }
        if (reply.body.size() > kPreviewMostHtmlBytes ||
            reply.declared_length > kPreviewMostHtmlBytes) {
          return Refused("page trop lourde : " + current);
        }
        std::optional<std::string> picture = FindOgImage(reply.body, current);
        if (!picture.has_value()) {
          return Refused("pas d'og:image dans la page : " + current);
        }
        page_followed = true;
        current = std::move(*picture);
        continue;
      }
      case PreviewBody::kRefused:
        break;
    }
    return Refused("type refusé « " + reply.content_type + " » : " + current);
  }
}

// ── LE REGISTRE ──────────────────────────────────────────────────────────────

std::vector<PreviewBook::Entry>::iterator PreviewBook::Find(std::string_view key) {
  return std::find_if(entries_.begin(), entries_.end(),
                      [&](const Entry& entry) { return entry.key == key; });
}

std::vector<PreviewBook::Entry>::const_iterator PreviewBook::Find(std::string_view key) const {
  return std::find_if(entries_.begin(), entries_.end(),
                      [&](const Entry& entry) { return entry.key == key; });
}

bool PreviewBook::Start(const std::string& key) {
  if (Find(key) != entries_.end()) {
    // Prête, en vol ou échouée : on ne redemande pas. Un échec reste.
    Touch(key);
    return false;
  }
  if (in_flight_ >= kPreviewMostInFlight) {
    // Rien n'est noté : la prochaine demande retentera.
    return false;
  }
  entries_.push_back(Entry{.key = key, .state = PreviewState::kPending});
  ++in_flight_;
  return true;
}

void PreviewBook::Ready(const std::string& key) {
  const auto found = Find(key);
  if (found == entries_.end() || found->state != PreviewState::kPending) {
    return;
  }
  found->state = PreviewState::kReady;
  --in_flight_;
}

void PreviewBook::Failed(const std::string& key) {
  const auto found = Find(key);
  if (found == entries_.end() || found->state != PreviewState::kPending) {
    return;
  }
  found->state = PreviewState::kFailed;
  --in_flight_;
}

void PreviewBook::Touch(const std::string& key) {
  const auto found = Find(key);
  if (found == entries_.end()) {
    return;
  }
  std::rotate(found, std::next(found), entries_.end());
}

void PreviewBook::Forget(std::string_view key) {
  const auto found = Find(key);
  if (found == entries_.end() || found->state == PreviewState::kPending) {
    return;
  }
  entries_.erase(found);
}

PreviewState PreviewBook::State(std::string_view key) const {
  const auto found = Find(key);
  return found == entries_.end() ? PreviewState::kUnknown : found->state;
}

std::vector<std::string> PreviewBook::Evict() {
  std::vector<std::string> evicted;
  while (entries_.size() > most_entries_) {
    // 🔴 JAMAIS UNE ENTRÉE EN VOL : son fil la rendra, et elle ne trouverait
    // plus où se poser.
    const auto oldest = std::find_if(entries_.begin(), entries_.end(), [](const Entry& entry) {
      return entry.state != PreviewState::kPending;
    });
    if (oldest == entries_.end()) {
      break;
    }
    evicted.push_back(std::move(oldest->key));
    entries_.erase(oldest);
  }
  return evicted;
}

}  // namespace ro::core
