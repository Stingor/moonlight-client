#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/whisper_blocks.h"

#include "web/user_config_web.h"
#include "web/web_transport.h"

namespace moonclient {

// ── LA LISTE DE BLOCAGE, D'UNE CONNEXION À L'AUTRE ──────────────────────────
//
// Le serveur de zone oublie sa liste de blocage à la déconnexion. Le client
// d'origine en garde donc une à lui, rangée dans les données de compte du
// service web (`web/user_config_web.h`), et la rejoue à la première entrée en
// jeu : un blocage par nom, comme si le joueur retapait `/ex`.
//
// 🔴 CETTE LISTE N'EST PAS LE REFLET DU SERVEUR. Le reflet (`sim::WhisperBlockList`)
// se vide quand un autre serveur de zone prend la main ; celle-ci non. Elle suit
// les CHANGEMENTS que les réponses du serveur ont faits — un blocage réussi
// ajoute, un déblocage retire, « tout autoriser » vide — et se sauvegarde à
// chacun.
//
// ⚠ ELLE SE REJOUE UNE FOIS, après l'avis de chargement de la première carte :
// avant lui, le serveur jette sans réponse tout ce qu'il reçoit, et la demande
// notée attendrait une réponse qui ne viendra pas.
//
// ⚠ SANS SERVICE, LA LISTE NE VIT QUE LA SESSION, comme celle du serveur : un
// avertissement au journal, une seule fois, et le jeu continue. Un changement
// fait pendant la relecture attend qu'elle aboutisse, pour ne pas écraser la
// liste rangée par une liste partielle.
class WhisperBlockMemory {
 public:
  // `transport` nul : l'installation ne déclare pas de service.
  WhisperBlockMemory(web::WebTransport* transport, web::UserConfigIdentity identity);

  // La première carte est chargée, et le serveur l'a appris.
  void NoteMapEntered() noexcept { map_entered_ = true; }

  // L'IMAGE : suit les changements, fait avancer le service, et rend les noms
  // à bloquer de nouveau — une seule fois par session, et au plus
  // `sim::kMostPendingWhisperBlocks`, ce que le reflet sait apparier.
  [[nodiscard]] std::vector<std::string> Advance(
      std::span<const ro::sim::WhisperBlockChange> changes);

  // Les noms rangés, dans l'ordre où ils sont entrés.
  [[nodiscard]] std::span<const std::string> names() const noexcept { return names_; }
  // Vrai quand la liste a été relue : elle survivra à la session.
  [[nodiscard]] bool kept() const noexcept { return stage_ == Stage::kKept; }

 private:
  enum class Stage : std::uint8_t { kLoading, kKept, kSessionOnly };

  // Applique un changement ; vrai s'il a changé la liste.
  bool Follow(const ro::sim::WhisperBlockChange& change);
  void Digest(const web::UserConfigReply& loaded);
  void GiveUp(std::string_view why);

  std::optional<web::UserConfigWeb> web_;
  Stage stage_ = Stage::kSessionOnly;
  std::vector<std::string> names_;
  // Les changements arrivés pendant la relecture, appliqués après elle.
  std::vector<ro::sim::WhisperBlockChange> early_changes_;
  bool map_entered_ = false;
  bool replay_due_ = false;
  bool warned_ = false;
};

}  // namespace moonclient
