#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::core {

// ── CE QUE L'APERÇU D'UNE ADRESSE A LE DROIT DE TÉLÉCHARGER ─────────────────
//
// 🔴 UN APERÇU EST UNE REQUÊTE QUE LE JOUEUR N'A PAS DEMANDÉE : survoler une
// adresse fait voir au serveur d'en face qu'on est en ligne, et depuis où. Seuls
// les hôtes de la liste — ceux d'usine et ceux que le joueur a admis — se
// téléchargent d'eux-mêmes ; un autre attend un accord explicite.
//
// Tout est pur ici : la boucle des sauts reçoit son transport en argument, et
// le cache ne tient aucune texture.

// Les hôtes admis d'usine. Une règle qui commence par '.' vaut pour le suffixe.
inline constexpr std::array<std::string_view, 7> kDefaultPreviewHostRules = {
    ".discordapp.com", ".discordapp.net",       ".discord.com",        ".imgur.com",
    "imgur.com",       ".moonlight-destiny.fr", "moonlight-destiny.fr"};
// Le miroir des images du relai Discord : seules ses adresses se montrent en
// vignette dans le fil.
inline constexpr std::string_view kMirrorPicturePrefix =
    "https://moonlight-destiny.fr/images/relay/";
inline constexpr std::string_view kPreviewAccept =
    "image/avif,image/webp,image/*,text/html;q=0.8,*/*;q=0.5";
inline constexpr int kPreviewMostRedirects = 3;
inline constexpr std::size_t kPreviewMostBytes = 8U * 1024U * 1024U;
inline constexpr std::size_t kPreviewMostHtmlBytes = 256U * 1024U;
inline constexpr std::size_t kPreviewHtmlScanBytes = 64U * 1024U;
inline constexpr int kPreviewResolveTimeoutMs = 5000;
inline constexpr int kPreviewConnectTimeoutMs = 5000;
inline constexpr int kPreviewSendTimeoutMs = 8000;
inline constexpr int kPreviewReceiveTimeoutMs = 8000;
// LE TEMPS D'UN SAUT, TOUT COMPRIS : les délais ci-dessus valent par
// opération, et un serveur qui rend un octet toutes les sept secondes ne les
// dépasse jamais.
inline constexpr int kPreviewMostHopMs = 20000;
inline constexpr int kPreviewMostInFlight = 3;
inline constexpr std::size_t kPreviewCacheEntries = 24;
inline constexpr std::uint32_t kPreviewHoverDelayMs = 400;
inline constexpr std::size_t kMostUserHosts = 64;
// Les adresses accordées une fois que l'on garde : au-delà, les plus anciennes
// partent. Un accord est un geste du joueur, et soixante-quatre couvrent une
// longue soirée de chat.
inline constexpr std::size_t kMostAllowedOnce = 64;
inline constexpr int kHttpOk = 200;

class PreviewAllowList {
 public:
  PreviewAllowList() = default;
  explicit PreviewAllowList(std::span<const std::string> user_hosts);
  // Règles d'usine + hôtes du joueur.
  [[nodiscard]] bool AllowsHost(std::string_view host) const;
  // https seulement, sans '@', hôte admis, public et lisible tel qu'il s'écrit.
  [[nodiscard]] bool AllowsUrl(std::string_view openable) const;
  // ⚠ L'ACCORD VAUT POUR L'HÔTE DE CETTE ADRESSE, pas au-delà : ses sauts
  // restent sur cet hôte ou vont vers un hôte de la liste. Il ne lève aucune
  // autre garde — HTTPS, pas de « @ », pas d'hôte local, numérique ou
  // trompeur —, et rien n'en est retenu au-delà de l'adresse.
  void AllowOnce(std::string_view openable);
  [[nodiscard]] bool AllowedOnce(std::string_view openable) const;
  // AllowsUrl || AllowedOnce.
  [[nodiscard]] bool Previewable(std::string_view openable) const;

 private:
  std::vector<std::string> user_hosts_;
  // Les adresses accordées pour cette fois, telles qu'ouvrables. Les plus
  // anciennes partent au-delà de `kMostAllowedOnce`.
  std::vector<std::string> allowed_once_;
};

[[nodiscard]] bool IsMirrorPicture(std::string_view url);

enum class PreviewBody : std::uint8_t { kImage, kHtml, kRefused };
[[nodiscard]] PreviewBody ClassifyContentType(std::string_view content_type);
[[nodiscard]] std::string ResolveLocation(std::string_view base, std::string_view location);
// Cherche la balise og:image dans la tête du HTML. L'adresse rendue est
// absolue ; rien quand la balise est absente.
[[nodiscard]] std::optional<std::string> FindOgImage(std::string_view html,
                                                     std::string_view page_url);

// Ce qu'un saut a rendu.
struct PreviewReply {
  int status = 0;
  std::string content_type;
  std::string location;
  std::string body;
  std::uint64_t declared_length = 0;
  std::string error;
};
using PreviewGet = std::function<PreviewReply(std::string_view url, std::size_t most_bytes)>;
struct PreviewFetched {
  bool ok = false;
  std::string final_url;
  std::string body;
  std::string failure;
};
// La boucle des sauts, pure : HTTPS imposé, hôte revérifié à chaque saut,
// 3 redirections au plus, seul le statut 200 est accepté, og:image suivi une
// seule fois. Aucun réseau ici : le transport arrive par `get`.
[[nodiscard]] PreviewFetched FetchPreviewBytes(std::string_view url, const PreviewAllowList& allow,
                                               const PreviewGet& get);

// Les états et l'ordre LRU, sans aucune texture. Un échec reste en cache.
enum class PreviewState : std::uint8_t { kUnknown, kPending, kReady, kFailed };
class PreviewBook {
 public:
  PreviewBook() = default;
  // Un registre plus court que celui des aperçus : la visionneuse, dont chaque
  // image pèse seize fois plus.
  explicit PreviewBook(std::size_t most_entries) : most_entries_(most_entries) {}

  // Vrai : lancer (≤ 3 en vol, ni prêt ni échoué). Une entrée connue est
  // seulement servie, et rend faux.
  [[nodiscard]] bool Start(const std::string& key);
  void Ready(const std::string& key);
  void Failed(const std::string& key);
  void Touch(const std::string& key);
  // Retire une entrée prête ou échouée, pour qu'un accord neuf la relance. Une
  // entrée en vol reste : son fil la rendra.
  void Forget(std::string_view key);
  [[nodiscard]] PreviewState State(std::string_view key) const;
  [[nodiscard]] int in_flight() const noexcept { return in_flight_; }
  // Au-delà de 24 : les plus anciennes, jamais une en vol.
  [[nodiscard]] std::vector<std::string> Evict();

 private:
  struct Entry {
    std::string key;
    PreviewState state = PreviewState::kUnknown;
  };
  [[nodiscard]] std::vector<Entry>::iterator Find(std::string_view key);
  [[nodiscard]] std::vector<Entry>::const_iterator Find(std::string_view key) const;

  // De la moins récemment servie à la plus récente.
  std::vector<Entry> entries_;
  std::size_t most_entries_ = kPreviewCacheEntries;
  int in_flight_ = 0;
};

}  // namespace ro::core
