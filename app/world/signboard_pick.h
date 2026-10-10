#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <glm/glm.hpp>

#include "ro/render/actor_pick.h"
#include "ro/sim/actor.h"

namespace moonclient {

// L'ENSEIGNE D'UNE ÉCHOPPE, TELLE QU'ELLE A ÉTÉ PEINTE : à qui elle est, de
// quelle sorte, et où elle tient à l'écran.
//
// ⚠ RELEVÉE PAR LA PEINTURE, lue par le clic : c'est l'image PRÉCÉDENTE, comme
// pour les acteurs. Recalculer le cadre au clic en referait la mesure à un
// second endroit, et les deux finiraient par ne plus désigner le même panneau.
struct SignboardHit {
  std::uint32_t owner_id = 0;
  ro::sim::SignboardKind kind = ro::sim::SignboardKind::kVendingShop;
  ro::render::ScreenRect rect;
  // La zone où le titre s'écrit, et le titre ENTIER quand il a fallu le couper
  // — vide sinon. La souris arrêtée sur la zone le montre en bulle.
  ro::render::ScreenRect title_rect;
  std::string cut_title;
};

// Une enseigne ouvre-t-elle une échoppe ? Celles des deux échoppes ; celle d'un
// salon a son propre chemin, le double-clic qui y entre.
[[nodiscard]] bool SignboardOpensStore(ro::sim::SignboardKind kind) noexcept;

// CE QUI DÉCIDE SI UNE DEMANDE D'ÉCHOPPE PART : la liste d'un vendeur, ou ce
// qu'un acheteur veut.
struct StoreVisitGate {
  std::uint32_t owner_id = 0;
  std::uint32_t player_id = 0;
  bool player_dead = false;
  // Une visite est déjà ouverte — chez un vendeur ou chez un acheteur.
  bool visiting = false;
  // L'instant de la demande précédente, s'il y en a eu une.
  std::optional<std::uint32_t> last_sent_ms;
  std::uint32_t now_ms = 0;
};

// L'écart à DÉPASSER entre deux demandes, en millisecondes : une demande à
// exactement cet écart de la précédente ne part pas.
inline constexpr std::uint32_t kStoreVisitSpacingMs = 1000;

// La demande part-elle ? Pas pour sa propre échoppe, pas mort, pas quand une
// visite est déjà ouverte, et pas avant que l'écart soit dépassé. C'est ce qui
// empêche un double-clic, sous `/shopping`, d'en envoyer deux.
[[nodiscard]] bool StoreVisitMayLeave(const StoreVisitGate& gate) noexcept;

// L'enseigne sous ce point, de quelque sorte qu'elle soit, ou rien. ⚠ LA
// DERNIÈRE PEINTE GAGNE.
[[nodiscard]] std::optional<SignboardHit> TopSignboardAt(std::span<const SignboardHit> boards,
                                                         glm::vec2 point_px);

// L'enseigne d'échoppe sous ce point, ou rien. ⚠ LA DERNIÈRE PEINTE GAGNE :
// c'est elle qui recouvre les autres à l'écran. Un salon peint par-dessus une
// échoppe la cache donc au clic comme à l'œil.
[[nodiscard]] std::optional<SignboardHit> StoreSignboardAt(std::span<const SignboardHit> boards,
                                                           glm::vec2 point_px);

}  // namespace moonclient
