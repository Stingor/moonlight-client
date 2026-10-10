#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// L'ATELIER, traduit dans le monde. Voir `ro/sim/craft_session.h` pour ce que le
// serveur tient — la session — et ce que le client en retient.
//
// Sept trames : trois listes de fabrication, leur réponse, la maîtrise
// culinaire de ce serveur, la liste des armes à affiner et sa réponse.
[[nodiscard]] FrameOutcome ApplyCraftFrame(const Frame& frame, sim::World& world);

// CE QUE LE JOUEUR VIENT D'ENVOYER, s'il peut ouvrir une liste : un skill lancé
// sur une cible, un objet utilisé. À brancher sur l'observateur d'émission de la
// liaison — voir `Connection::set_send_observer`.
//
// 🔴 L'OBJET SE RÉSOUT MAINTENANT, par sa place dans le sac : une fois
// l'exemplaire consommé, la place ne désigne plus rien, ou un autre objet.
void ObserveCraftTrigger(std::span<const std::uint8_t> sent, sim::World& world,
                         std::uint32_t now_ms);

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// ⚠ ILS ÉMETTENT, ET RIEN DE PLUS. Chacun répond à la liste que le serveur a
// ouverte, et le serveur l'efface dès qu'il l'a reçu : une seconde réponse à la
// même liste est jetée sans un mot.

// La réponse d'une ANNULATION sur les chemins qui la portent en identifiant.
inline constexpr std::uint32_t kMakeCancelItem = 0xFFFFFFFFU;
// Les trois matériaux optionnels du chemin `kProduce` ; zéro est un emplacement
// vide.
inline constexpr std::size_t kProduceExtraSlots = 3;

// « Fabrique ceci » sur la liste des flèches. `kMakeCancelItem` annule.
[[nodiscard]] core::Result<void> RequestMakeArrow(Connection& link, std::uint32_t item_id);

// « Fabrique ceci » sur une liste qui rappelle son genre. `kMakeCancelItem`
// annule, avec le MÊME genre.
[[nodiscard]] core::Result<void> RequestMakeItem(Connection& link, std::uint16_t make_type,
                                                 std::uint32_t item_id);

// « Fabrique ceci, avec ces matériaux en plus » sur une liste de forge ou de
// pharmacie. ⚠ L'ANNULATION EST ZÉRO ICI, pas moins un : c'est ainsi que le
// serveur l'attend sur ce chemin.
[[nodiscard]] core::Result<void> RequestProduce(
    Connection& link, std::uint32_t item_id,
    const std::array<std::uint32_t, kProduceExtraSlots>& extras);

// La place qui ANNULE un affinage.
inline constexpr std::int32_t kWeaponRefineCancel = -1;

// « Affine l'arme de cette place. » `kWeaponRefineCancel` annule.
[[nodiscard]] core::Result<void> RequestWeaponRefine(Connection& link, std::int32_t index);

}  // namespace ro::net
