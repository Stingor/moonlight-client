#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ro::proto {

// LA MARQUE DE QUÊTE D'UN PNJ (`ZC_QUEST_NOTIFY_EFFECT`), écrite à la main par
// le serveur (clif_quest_show_event) :
//
//     0446 <pnj>.L <x>.W <y>.W <effet>.W <couleur>.W
//
// `effect` est un `e_questinfo_types` du serveur — de 0 à 10, ou 9999 pour
// RETIRER la marque — et `color` un `e_questinfo_markcolor` : 0 aucune, puis
// jaune, vert, violet. La case est celle du PNJ.
struct QuestMarkerNotice {
  static constexpr std::uint16_t kOpcode = 0x0446;
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t npc_id = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint16_t effect = 0;
  std::uint16_t color = 0;
};

// Faux si la trame n'a ni le bon opcode ni la bonne taille.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, QuestMarkerNotice& packet);

// L'ENVERS DU DÉCODEUR, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> Encode(const QuestMarkerNotice& packet);

}  // namespace ro::proto
