#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ro/net/frame_tally.h"
#include "ro/sim/companion_ai.h"
#include "ro/sim/world_fwd.h"

#include "debug/debug_request.h"

namespace moonclient {

// ── CE QUE LA CONSOLE DE MISE AU POINT SAIT DIRE ────────────────────────────
//
// 🔴 EN LECTURE SEULE : aucune requête n'écrit dans le monde ni n'envoie quoi
// que ce soit au serveur. La console observe le client ; elle ne joue pas.
//
// Chaque réponse est UNE ligne JSON : `{"id":…,"ok":true,"result":…}`, ou
// `{"id":…,"ok":false,"error":"…"}`.

// Un compagnon tel que la console le voit : son nom pour la réponse, son acteur
// (zéro s'il n'est pas là) et son cerveau.
struct DebugCompanion {
  std::string_view name;
  std::uint32_t actor_id = 0;
  const ro::sim::CompanionBrain* brain = nullptr;
};

// Ce que les requêtes lisent. ⚠ TOUT EN LECTURE : des pointeurs vers du const.
struct DebugSources {
  const ro::sim::World* world = nullptr;
  const ro::net::FrameTally* frames = nullptr;
  std::span<const DebugCompanion> companions;
  std::uint32_t now_ms = 0;
};

// La réponse à une requête, sans le saut de ligne final.
[[nodiscard]] std::string AnswerDebugRequest(const DebugRequest& request,
                                             const DebugSources& sources);

// La réponse à une ligne que l'analyse n'a pas comprise.
[[nodiscard]] std::string AnswerUnreadableLine();

}  // namespace moonclient
