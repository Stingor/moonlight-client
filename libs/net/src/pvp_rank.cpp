// Le classement PVP. Voir l'en-tête pour la raison qui oblige à nommer les
// décalages, et pour ce que « en zone » change à la lecture.

#include "ro/net/pvp_rank.h"

#include "ro/core/byte_reader.h"
#include "ro/core/log.h"
#include "ro/sim/pvp_rank.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// L'opcode, qu'aucune structure engendrée ne porte.
constexpr std::uint16_t kPvpRankOpcode = 0x019a;

}  // namespace

FrameOutcome ApplyPvpRankFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != kPvpRankOpcode) {
    return FrameOutcome::kUnhandled;
  }
  if (frame.bytes.size() < kPvpRankWireSize) {
    return FrameOutcome::kUndecodable;
  }

  core::ByteReader reader(frame.bytes);
  reader.Seek(kPvpRankIdOffset);
  const std::uint32_t id = reader.U32();
  // ⚠ LUS EN SIGNÉ, ET C'EST CE QUI REND SON SENS AU RANG D'UN JOUEUR CACHÉ. Le
  // serveur y met `UINT32_MAX` en annonçant « On client displays as -- » ; le
  // client d'origine, lui, le compare à zéro SIGNÉ, donc moins un ne dessine
  // rien du tout. Les lire en non signé afficherait « 4294967295 ».
  const auto rank = static_cast<std::int32_t>(reader.U32());
  const auto total = static_cast<std::int32_t>(reader.U32());
  if (!reader.ok()) {
    return FrameOutcome::kUndecodable;
  }

  // 🔴 L'ANNEAU D'ABORD, ET POUR TOUT LE MONDE. Le paquet arrive en zone : le
  // rang d'un voisin est ce qui décide de l'anneau qu'il porte, et c'est sa
  // seule source. Le poser sur soi aussi n'est pas un cas à part — le client
  // d'origine marque quiconque entre dans les dix premiers, soi compris.
  const bool marked = world.TellActorPvpRank(id, rank, now_ms);
  // Ce qui décide de l'anneau, et s'il a trouvé son porteur : sans cette ligne,
  // un anneau absent ne dit pas si le rang manquait ou si l'acteur n'était pas là.
  core::LogDebug("classement PVP : {} rang {} sur {}{}", id, rank, total,
                 marked ? "" : " (hors de vue, gardé au carnet)");

  // 🔴 LE COMPTEUR, LUI, N'ÉCOUTE QUE SON PROPRE IDENTIFIANT. Le prendre du
  // dernier paquet reçu le ferait sauter au rang du voisin qui vient de tuer
  // quelqu'un — invisible en solo, où l'on ne reçoit que le sien.
  if (id != world.player_id()) {
    // ⚠ « APPLIQUÉ » DÈS QUE L'ANNEAU A TROUVÉ SON PORTEUR : la trame a changé
    // le monde. Elle n'est IGNORÉE que si l'acteur n'est pas en vue — ce qui
    // arrive, le serveur diffusant à toute la carte quand la zone s'éteint.
    return marked ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
  }
  world.pvp_standing().Note(rank, total, now_ms);
  return FrameOutcome::kApplied;
}

}  // namespace ro::net
