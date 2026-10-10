#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE SERTISSAGE D'UNE CARTE : la fenêtre qui remplace le « Insert Card » du
// client.
//
// 🔴 ELLE NE DÉCIDE D'AUCUNE COMPATIBILITÉ. La liste vient du serveur, prise
// telle quelle ; la fenêtre la montre, et rend la pièce choisie. Ce qu'elle
// écarte d'elle-même — une pièce dont les cases sont toutes prises — n'est pas
// un filtre : c'est la borne du serveur, qui ne la proposerait plus.
//
// ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : c'est la liste du serveur qui
// l'ouvre. L'état recopie la session du monde, comme la banque.

inline constexpr const char* kCardInsertWindowTitle = core::TrText("Sertir une carte");
inline constexpr const char* kCardInsertWindowId = "card_insert";

// COMBIEN DE SERTISSAGES D'AFFILÉE UN BOUTON PEUT DEMANDER. Au-delà, la pièce
// n'a de toute façon plus de case : le fil n'en porte que quatre.
inline constexpr int kCardInsertMostTimes = 4;

// UNE PIÈCE PROPOSÉE.
struct CardInsertRowView {
  // Sa place dans le sac : c'est elle qui part sur le fil.
  std::uint16_t equip_slot = 0;
  std::uint32_t item_id = 0;
  // Le nom composé — raffinement, cartes et « [N] » compris.
  std::string_view name;
  SkinImage icon;
  // Les cases de carte déjà prises parmi celles que la pièce possède, et
  // combien elle en possède. ⚠ ZÉRO SUR UN OBJET FORGÉ : ses quatre nombres
  // disent qui l'a forgé, pas ce qui y est serti.
  int used_slots = 0;
  int total_slots = 0;
};

// CE QUE LA FENÊTRE SAIT DU MONDE, recopié par l'appelant à chaque image.
struct CardInsertView {
  // La carte est-elle encore dans le sac ? ⚠ ELLE PEUT AVOIR DISPARU pendant
  // que la fenêtre est ouverte — vendue, échangée, ou sertie jusqu'à la
  // dernière.
  bool card_found = false;
  std::string_view card_name;
  SkinImage card_icon;
  // Combien il en reste en tout dans le sac, toutes piles confondues. Lu à
  // chaque image : il décroît à chaque accusé.
  int stock = 0;
  // Les pièces qu'on RETROUVE dans le sac et qui ont encore une case libre.
  std::span<const CardInsertRowView> rows;
  // Combien le serveur en avait listé. ⚠ C'EST CE QUI SÉPARE deux fenêtres
  // vides : une liste qu'on ne retrouve plus, et une liste qui n'en portait pas.
  int listed = 0;
  // Le rang de la liste montrée : une liste nouvelle efface la sélection.
  std::uint64_t offer_seq = 0;
};

enum class CardInsertVerb : std::uint8_t {
  kNothing,
  // Sertir `times` fois la carte sur la pièce choisie.
  kInsert,
  // 🔴 FERMER N'EST PAS UN VERBE DU SERVEUR : il ne tient aucune session de
  // sertissage. La croix et « Fermer » referment chez nous, sans un octet.
  kClose,
};

struct CardInsertWindowAction {
  CardInsertVerb verb = CardInsertVerb::kNothing;
  std::uint16_t equip_slot = 0;
  int times = 1;
  // La pièce survolée, pour l'aperçu de description que l'étage dessine.
  std::uint16_t hovered_slot = 0;
  std::uint32_t hovered_id = 0;
};

struct CardInsertWindowState {
  // ⚠ CE N'EST PAS LE JOUEUR QUI LE POSE : il recopie la session du monde.
  bool open = false;
  // Le front montant : la fenêtre passe au PREMIER PLAN à l'ouverture, et
  // seulement là. Le double-clic vient du sac, qui a le focus — sans ce
  // geste elle naîtrait derrière lui ; le forcer à chaque image la rendrait
  // impossible à passer dessous.
  bool was_open = false;
  // LA PIÈCE CHOISIE, par sa PLACE et non par son rang : elle reste juste si la
  // liste change d'ordre, et tombe si la pièce disparaît. Zéro : aucune.
  std::uint16_t selected = 0;
  std::uint64_t seen_offer_seq = 0;
  // L'aperçu de description remplace-t-il l'infobulle simple ? C'est le
  // réglage du SAC : les deux fenêtres montrent les mêmes objets.
  bool hover_preview = false;
};

[[nodiscard]] CardInsertWindowAction DrawCardInsertWindow(CardInsertWindowState& state,
                                                          const CardInsertView& view,
                                                          const SkinArt& art);

}  // namespace ro::ui
