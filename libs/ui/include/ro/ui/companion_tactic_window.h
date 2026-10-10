#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LES RÉGLAGES AVANCÉS D'UNE TACTIQUE DE COMPAGNON ────────────────────────
//
// La petite fenêtre « Tactique : <monstre> » qu'ouvre « Avancé… » dans le menu
// d'entité. Elle porte les champs d'une ligne de tactique que le sous-menu ne
// montre pas.
//
// 🔴 `DrawTacticAdvancedFields` EST LE CŒUR, ET IL SE RÉEMPLOIE : la section
// « Tactiques » de la fenêtre d'édition de l'IA dessinera les mêmes champs. La
// fenêtre n'en est que l'enveloppe.
//
// Cette couche ne connaît pas l'IA : les champs arrivent en nombres et en rangs
// de liste, dans l'ordre de ses énumérations, et `app` les traduit.

// Les rangs de chaque liste, dans l'ordre où `app` les numérote.
inline constexpr int kTacticAvoidChoices = 3;
inline constexpr int kTacticSkillUseChoices = 4;
inline constexpr int kTacticShareChoices = 3;
inline constexpr int kTacticChaseChoices = 3;
// La poursuite « Toujours », après « Comme le réglage général ».
inline constexpr int kTacticChaseAlways = 1;
// « N fois » et « une fois au niveau L » : les deux emplois qui portent un nombre.
inline constexpr int kTacticSkillUseTimes = 1;
inline constexpr int kTacticSkillUseOnceAtLevel = 2;
// Les bornes de ce que les champs acceptent : celles de l'IA, ce que `app`
// vérifie à la compilation avant de revalider chaque ligne.
inline constexpr int kTacticAvoidMostCells = 20;
inline constexpr int kTacticSkillUseMost = 99;
inline constexpr int kTacticSpReserveMost = 99999;
inline constexpr float kTacticWeightMost = 100.0F;

// LA BORNE D'UN CURSEUR DE DISTANCE : le rayon de vue que le serveur annonce,
// sous le plafond absolu `most_cells`. Au-delà, le compagnon disparaît du
// client et aucune cible n'y est connue. Zéro, la valeur d'une vue non remplie :
// le plafond seul.
[[nodiscard]] constexpr int CompanionAiCellsShown(int view_cells, int most_cells) noexcept {
  return view_cells > 0 ? std::min(view_cells, most_cells) : most_cells;
}

struct TacticAdvancedFields {
  int avoid = 0;
  int avoid_threshold_cells = 1;
  int avoid_step_cells = 1;
  int skill_use = 0;
  int skill_use_count = 0;
  // Zéro : aucun.
  std::uint16_t opening_skill_id = 0;
  bool harass = false;
  // Négatif : celle du réglage général.
  int sp_reserve_points = -1;
  float weight = 1.0F;
  int share = 0;
  int chase = 0;

  [[nodiscard]] friend bool operator==(const TacticAdvancedFields&,
                                       const TacticAdvancedFields&) = default;
};

// UN SKILL QUE L'OUVERTURE PEUT NOMMER, son libellé déjà traduit.
struct TacticSkillChoice {
  std::uint16_t skill_id = 0;
  std::string label;
};

// LES CHAMPS, dessinés en place. Rend vrai quand l'un d'eux a changé.
// `for_player` : ceux d'une conduite envers un joueur, sans l'évitement, le
// harcèlement, le partage ni le poids, qui ne valent que pour un monstre.
// `view_cells` : le rayon de vue du serveur, qui borne les curseurs de distance.
[[nodiscard]] bool DrawTacticAdvancedFields(TacticAdvancedFields& fields,
                                            std::span<const TacticSkillChoice> openings,
                                            const SkinArt& art, int view_cells,
                                            bool for_player = false);

// L'identité de la fenêtre, pour retenir son épingle et la ranger dans la
// pile d'Échap.
inline constexpr const char* kCompanionTacticWindowId = "###companion_tactic";

struct CompanionTacticView {
  // « Tactique : <monstre> », et le compagnon, déjà traduits.
  std::string title;
  std::string companion;
  TacticAdvancedFields fields;
  std::span<const TacticSkillChoice> openings;
  // Le rayon de vue de la session, en cases : celui que le serveur annonce, et
  // le défaut d'un rAthena (quatorze) tant qu'il n'a rien dit. `app` le pose
  // toujours ; zéro, la valeur d'une vue non remplie, laisse le plafond seul.
  int view_cells = 0;
};

struct CompanionTacticWindowState {
  bool open = false;
  // Le front montant : la fenêtre se place sous la souris et passe devant.
  bool was_open = false;
  // L'épingle de sa barre de titre, retenue par `app` d'une session à l'autre.
  bool pinned = false;
  // LE BOUTON « SIGNALER UN BUG » de sa barre de titre, posé par `app` depuis
  // les réglages.
  bool bug_report = false;
};

struct CompanionTacticWindowAction {
  // Les champs, s'ils viennent de changer : ils valent aussitôt pour l'IA.
  std::optional<TacticAdvancedFields> changed;
  bool closed = false;
  // Le bouton « signaler un bug » de la barre de titre a été cliqué.
  bool report_bug = false;
};

[[nodiscard]] CompanionTacticWindowAction DrawCompanionTacticWindow(
    CompanionTacticWindowState& state, const CompanionTacticView& view, const SkinArt& art);

}  // namespace ro::ui
