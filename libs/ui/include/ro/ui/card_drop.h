#pragma once

#include <cstdint>

#include <imgui.h>

namespace ro::ui {

// SERTIR PAR GLISSER-DÉPOSER : une carte tirée du sac, lâchée sur une pièce.
//
// 🔴 LA FENÊTRE NE JUGE RIEN. Elle rapporte quelle carte survole quelle pièce,
// puis sur laquelle on l'a lâchée ; `app` décide et lui rend, à l'image
// suivante, ce qu'elle doit montrer. Le jugement demande le sac, le négoce en
// cours et la réponse du serveur — trois choses qu'une fenêtre ne possède pas.

// CE QUE LA CIBLE MONTRE PENDANT LE SURVOL.
enum class CardDropLook : std::uint8_t {
  // Pas encore jugé : la réponse du serveur n'est pas arrivée.
  kUnknown,
  kAccepted,
  kRefused,
};

// UNE CARTE AU-DESSUS D'UNE PIÈCE — ou lâchée dessus. Les deux sont des places
// du sac : une pièce portée y a toujours la sienne.
struct CardDropAim {
  std::uint16_t card_slot = 0;
  std::uint16_t equip_slot = 0;

  [[nodiscard]] bool aimed() const noexcept { return card_slot != 0 && equip_slot != 0; }
};

// CE QU'UNE FENÊTRE A VU PENDANT L'IMAGE.
struct CardDropSeen {
  CardDropAim hovering;
  CardDropAim dropped;
};

// LE JUGEMENT QUE `app` REND POUR LE SURVOL DE L'IMAGE PRÉCÉDENTE.
//
// ⚠ IL NE VAUT QUE POUR SA PAIRE : une autre carte, ou une autre pièce, se
// montre comme non jugée.
struct CardDropHint {
  std::uint16_t card_slot = 0;
  std::uint16_t equip_slot = 0;
  CardDropLook look = CardDropLook::kUnknown;
  // Ce que le fantôme du glisser écrit en rouge. `nullptr` sans refus.
  const char* refusal = nullptr;

  [[nodiscard]] bool Describes(const CardDropAim& aim) const noexcept {
    return aim.card_slot == card_slot && aim.equip_slot == equip_slot;
  }
};

// CE QU'UNE CIBLE A PRIS.
struct CardDropTaken {
  // Vrai : la charge est une carte du sac, et elle est à cette cible. Aucune
  // autre cible du même élément ne doit alors la lire — elle rangerait la case
  // ou enfilerait la carte.
  bool taken = false;
  CardDropLook look = CardDropLook::kUnknown;
};

// LA CIBLE DE SERTISSAGE, posée sur le DERNIER élément soumis : la case d'une
// pièce dont `equip_slot` est la place dans le sac. Écrit le survol et le
// lâcher dans `seen`.
//
// ⚠ ELLE NE PREND QU'UNE CARTE SORTIE DU SAC : tout autre objet tiré passe aux
// cibles ordinaires du même élément.
[[nodiscard]] CardDropTaken TakeCardDrop(std::uint16_t equip_slot, const CardDropHint& hint,
                                         CardDropSeen& seen);

// LE CADRE DU JUGEMENT, vert ou rouge, ou pâle tant qu'il manque. À peindre
// APRÈS l'icône de la case, qui le couvrirait.
void PaintCardDropFrame(ImDrawList* into, ImVec2 top_left, ImVec2 bottom_right, CardDropLook look);

}  // namespace ro::ui
