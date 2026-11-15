#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/inventory.h"
#include "ro/sim/item_enchant.h"

namespace ro::net {

// L'ENCHANTEMENT, SUR LE FIL.
//
// L'ouverture de l'interface d'enchantement et la réponse sont à lui ; une
// ouverture d'une autre interface ne l'est pas (`kUnhandled`). Les changements de
// carte, il les lit sans les prendre : ils le ferment en silence.
//
// ⚠ LE SAC CHANGE AVEC UNE RÉUSSITE : la case visée reçoit l'enchantement que la
// réponse nomme, ou une remise à zéro vide les cases d'enchantement. Le serveur ne
// renvoie pas la pièce ; les zenys et les matériaux, eux, arrivent par leurs trames.
[[nodiscard]] FrameOutcome ApplyItemEnchantFrame(const Frame& frame, sim::ItemEnchant& enchant,
                                                 sim::Inventory& bag);

// Envoie la demande. Refuse d'émettre ce que `sim::CheckItemEnchantRequest` rejette :
// le serveur ne répondrait pas, et la fenêtre attendrait pour rien.
[[nodiscard]] core::Result<void> RequestItemEnchant(Connection& link, sim::ItemEnchant& enchant,
                                                    const sim::ItemEnchantGroup& group,
                                                    sim::ItemEnchantRequest request,
                                                    const sim::Inventory& bag,
                                                    std::uint8_t card_slots, std::int64_t zeny);

// Émet la fermeture due (`sim::ItemEnchant::TakeFarewell`).
void SayItemEnchantFarewell(Connection& link, sim::ItemEnchant& enchant);

}  // namespace ro::net
