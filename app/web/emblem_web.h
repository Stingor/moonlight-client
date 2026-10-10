#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "web/web_transport.h"

namespace moonclient::web {

// ── LE SERVICE WEB DES EMBLÈMES DE GUILDE ───────────────────────────────────
//
// 🔴 UN EMBLÈME NE VOYAGE PLUS PAR LE FIL DU JEU. Le serveur de carte ne publie
// qu'une VERSION ; l'image se dépose et se lit sur son service web, deux
// chemins écrits en dur sous l'adresse que l'installation déclare. Le client
// d'origine fait exactement cela.
//
//   déposer   /emblem/upload    →  {"Type":1,"version":N}, puis on annonce N
//                                   au serveur de carte, qui la publie ;
//   lire      /emblem/download  →  l'image, BMP ou GIF, telle qu'elle a été
//                                   déposée.
//
// ⚠ CE MODULE NE CONNAÎT NI TEXTURE NI FICHIER : il rend des octets. Ce qui les
// décode, les range sur disque et les montre vit ailleurs.
//
// ⚠ UNE REQUÊTE À LA FOIS, comme le transport : un dépôt passe devant les
// lectures en attente, parce que c'est le joueur qui l'a demandé.

inline constexpr std::string_view kEmblemUploadPath = "/emblem/upload";
inline constexpr std::string_view kEmblemDownloadPath = "/emblem/download";

// LE DÉLAI AVANT DE REDEMANDER un emblème qui n'est pas venu, en millisecondes.
//
// ⚠ C'EST CELUI DU CLIENT D'ORIGINE, qui purge ses demandes au bout d'une
// minute et les relance. Plus court, un service en panne serait martelé.
inline constexpr std::uint32_t kEmblemRetryMs = 60'000;

// CE QU'IL FAUT POUR PARLER AU SERVICE : le compte, le jeton de la session, et
// le monde sous lequel les emblèmes sont rangés.
struct EmblemIdentity {
  std::uint32_t account_id = 0;
  std::string token;
  std::string world_name;

  [[nodiscard]] bool usable() const noexcept { return account_id != 0 && !token.empty(); }
};

struct DownloadedEmblem {
  std::int32_t guild_id = 0;
  std::int32_t version = 0;
  // L'image telle que déposée : un BMP, ou un GIF.
  std::string image;
};

// CE QU'UN DÉPÔT A DONNÉ.
struct EmblemUploadOutcome {
  std::int32_t guild_id = 0;
  // La version que le service vient d'attribuer, ou zéro quand il a refusé.
  std::int32_t version = 0;
  // Le motif, en clair, quand il a refusé ou n'a pas répondu.
  std::string refusal;

  [[nodiscard]] bool accepted() const noexcept { return version > 0; }
};

class EmblemWeb {
 public:
  explicit EmblemWeb(WebTransport& transport) : transport_(&transport) {}

  void SetIdentity(EmblemIdentity identity) { identity_ = std::move(identity); }
  [[nodiscard]] bool ready() const noexcept { return identity_.usable(); }

  // « IL ME FAUT CET EMBLÈME. » Sans effet s'il est déjà en file, en vol, ou
  // s'il a échoué il y a moins de `kEmblemRetryMs`. Une version nulle ne
  // demande rien : la guilde n'a pas d'emblème.
  void Want(std::int32_t guild_id, std::int32_t version, std::uint32_t now_ms);

  // DÉPOSE UNE IMAGE BMP. Rend faux quand un dépôt attend déjà : le client
  // d'origine n'en accepte qu'un à la fois, et le joueur doit le savoir.
  [[nodiscard]] bool Upload(std::int32_t guild_id, std::string bmp);
  [[nodiscard]] bool uploading() const noexcept { return upload_.has_value(); }

  // Fait avancer : prend une réponse, lance la requête suivante.
  void Advance(std::uint32_t now_ms);

  // Ce qui est arrivé depuis la dernière fois. ⚠ SE PREND ET S'EFFACE.
  [[nodiscard]] std::vector<DownloadedEmblem> TakeDownloads();
  [[nodiscard]] std::optional<EmblemUploadOutcome> TakeUploadOutcome();

 private:
  using Key = std::pair<std::int32_t, std::int32_t>;
  enum class Flight : std::uint8_t { kNothing, kDownload, kUpload };

  [[nodiscard]] std::vector<WebPart> IdentityParts(std::int32_t guild_id) const;
  void Digest(const WebReply& reply, std::uint32_t now_ms);

  WebTransport* transport_;
  EmblemIdentity identity_;
  std::deque<Key> queue_;
  // L'instant avant lequel une clé ne se redemande pas.
  std::map<Key, std::uint32_t> resting_until_;
  Flight flight_ = Flight::kNothing;
  Key flying_ = {0, 0};
  struct PendingUpload {
    std::int32_t guild_id = 0;
    std::string bmp;
    bool sent = false;
  };
  std::optional<PendingUpload> upload_;
  std::vector<DownloadedEmblem> downloads_;
  std::optional<EmblemUploadOutcome> outcome_;
};

}  // namespace moonclient::web
