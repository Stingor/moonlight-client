#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA LISTE DES SORTS — « Spell List » —, la fenêtre où Autospell fait choisir
// le sort qu'il armera.
//
// Celle du client : deux cents pixels de côté, une ligne de trente-deux pixels
// par sort — son icône, puis son nom —, quatre lignes visibles, « OK » et
// « Annuler » en bas à droite. La ligne choisie se surligne.
//
// ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : c'est la liste du serveur qui
// l'ouvre. « OK » envoie le sort choisi ; « Annuler » et la croix envoient zéro,
// le refus, sans quoi le serveur attendrait.

inline constexpr const char* kSpellListWindowId = "spell_list";

// ── LA GÉOMÉTRIE DU CLIENT, en pixels d'art, depuis le coin de la fenêtre ───
inline constexpr float kSpellListWindowSide = 200.0F;
inline constexpr float kSpellListFirstRowTop = 34.0F;
inline constexpr float kSpellListRowStep = 32.0F;
inline constexpr int kSpellListVisibleRows = 4;
inline constexpr float kSpellListIconLeft = 8.0F;
inline constexpr float kSpellListNameLeft = 40.0F;
inline constexpr float kSpellListNameDrop = 4.0F;
// Le surlignage : de x = 5, trente pixels de haut, trois au-dessus de la ligne,
// et vingt-quatre de moins que la fenêtre en largeur.
inline constexpr float kSpellListHighlightLeft = 5.0F;
inline constexpr float kSpellListHighlightRise = 3.0F;
inline constexpr float kSpellListHighlightHeight = 30.0F;
inline constexpr float kSpellListHighlightRightMargin = 24.0F;
// Les deux boutons, depuis le coin bas-droit : « OK » à 92 pixels du bord,
// « Annuler » à 46, tous deux à 24 du bas.
inline constexpr float kSpellListOkFromRight = 92.0F;
inline constexpr float kSpellListCancelFromRight = 46.0F;
inline constexpr float kSpellListButtonsFromBottom = 24.0F;

// UN SORT DE LA LISTE.
struct SpellListRow {
  std::uint16_t skill_id = 0;
  std::string_view name;
  SkinImage icon;
  // Le niveau que le serveur armera, que l'infobulle de la ligne annonce. Zéro :
  // inconnu, et l'infobulle se tait.
  std::uint16_t armed_level = 0;
};

struct SpellListView {
  std::span<const SpellListRow> rows;
};

struct SpellListWindowState {
  // ⚠ CE N'EST PAS LE JOUEUR QUI LE POSE : il recopie la liste du serveur.
  bool open = false;
  bool was_open = false;
  // La ligne choisie. Aucune : « OK » ne fait rien.
  std::optional<std::size_t> selected;
};

// La réponse qui refuse la liste : le serveur n'arme alors aucun sort.
inline constexpr std::uint16_t kSpellListRefusal = 0;

struct SpellListWindowAction {
  // Le sort choisi, ou zéro pour le refus. Vide tant que rien n'est décidé.
  std::optional<std::uint16_t> answer;
};

[[nodiscard]] SpellListWindowAction DrawSpellListWindow(SpellListWindowState& state,
                                                        const SpellListView& view,
                                                        const SkinArt& art);

}  // namespace ro::ui
