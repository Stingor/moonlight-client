#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/error.h"

#include "web/web_transport.h"

namespace moonclient::web {

// ── LES DONNÉES DE COMPTE DU SERVICE WEB ────────────────────────────────────
//
// Le service web du serveur de jeu garde, par compte et par monde, un document
// JSON que le client relit et complète :
//
//   relire     /userconfig/load  ←  AID, WorldName
//              →  {"Type":1, "data":{…}}, ou {"Type":1} pour un compte neuf ;
//   compléter  /userconfig/save  ←  AID, AuthToken, WorldName, data
//              où `data` est un document {"data":{…}} que le service FUSIONNE
//              au sien, clé par clé.
//
// 🔴 LA FUSION GARDE CE QU'ON N'ENVOIE PAS. Les raccourcis et les macros
// d'émotion que le client d'origine y range survivent donc à un envoi qui ne
// porte que la liste de blocage. Un tableau, lui, se remplace en entier : la
// liste envoyée est LA liste.
//
// ⚠ LA RELECTURE NE DEMANDE PAS DE JETON, la sauvegarde si : un jeton refusé
// ne se voit qu'à la première sauvegarde.

inline constexpr std::string_view kUserConfigLoadPath = "/userconfig/load";
inline constexpr std::string_view kUserConfigSavePath = "/userconfig/save";

// La clé, sous `data`, de la liste de ceux dont on ne veut plus de
// chuchotement : un tableau de noms en UTF-8, celle du client d'origine.
inline constexpr std::string_view kWhisperBlockListKey = "WhisperBlockList";

// CE QU'IL FAUT POUR PARLER AU SERVICE : le compte, le jeton de la session, et
// le monde sous lequel les données sont rangées.
struct UserConfigIdentity {
  std::uint32_t account_id = 0;
  std::string token;
  std::string world_name;

  [[nodiscard]] bool usable() const noexcept { return account_id != 0 && !token.empty(); }
};

// CE QU'UNE REQUÊTE A RAPPORTÉ : le corps quand le service a répondu 200, le
// motif sinon.
struct UserConfigReply {
  std::string body;
  std::string refusal;

  [[nodiscard]] bool served() const noexcept { return refusal.empty(); }
};

// LA LISTE DE BLOCAGE QU'UNE RELECTURE PORTE, dans l'ordre du document.
//
// ⚠ UN COMPTE NEUF N'EN A PAS : sans `data`, ou sans la clé, la liste est vide.
// Un élément qui n'est pas un nom non vide est écarté, un doublon aussi.
// Refusé : un corps illisible, ou dont `Type` n'est pas celui d'une réponse
// servie.
[[nodiscard]] ro::core::Result<std::vector<std::string>> WhisperBlocksFromUserConfig(
    std::string_view body);

// LE DOCUMENT À ENVOYER pour que le service range cette liste, et rien d'autre.
// ⚠ UNE LISTE VIDE PART QUAND MÊME, en tableau vide : l'omettre laisserait
// l'ancienne en place, puisque le service fusionne.
[[nodiscard]] std::string UserConfigOfWhisperBlocks(std::span<const std::string> names);

class UserConfigWeb {
 public:
  UserConfigWeb(WebTransport& transport, UserConfigIdentity identity)
      : transport_(&transport), identity_(std::move(identity)) {}

  [[nodiscard]] bool ready() const noexcept { return identity_.usable(); }

  // Demande la relecture. Sans effet si elle attend déjà.
  void Load() noexcept { load_wanted_ = true; }
  // Demande la sauvegarde de ce document. ⚠ LE DERNIER GAGNE : un document
  // qui attend encore son tour est remplacé, puisqu'il dit un état dépassé.
  void Save(std::string document) { save_document_ = std::move(document); }
  [[nodiscard]] bool idle() const noexcept {
    return flight_ == Flight::kNothing && !load_wanted_ && !save_document_.has_value();
  }

  // Prend une réponse, lance la requête suivante : la relecture d'abord.
  void Advance();

  // Ce qui est arrivé depuis la dernière fois. ⚠ SE PREND ET S'EFFACE.
  [[nodiscard]] std::optional<UserConfigReply> TakeLoaded();
  [[nodiscard]] std::optional<UserConfigReply> TakeSaved();

 private:
  enum class Flight : std::uint8_t { kNothing, kLoad, kSave };

  WebTransport* transport_;
  UserConfigIdentity identity_;
  bool load_wanted_ = false;
  std::optional<std::string> save_document_;
  Flight flight_ = Flight::kNothing;
  std::optional<UserConfigReply> loaded_;
  std::optional<UserConfigReply> saved_;
};

}  // namespace moonclient::web
