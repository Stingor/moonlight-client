#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/craft_log_view.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA FABRICATION : la fenêtre des listes du serveur — flèches, convertisseurs
// élémentaires, poisons, cuisine, bombes, pharmacie, runes, forge. Une fenêtre
// pour trois listes : elles ne diffèrent que par la réponse, que la partie
// choisit.
//
// Ce que le client d'origine ne dit pas, et qu'elle montre : le stock, le nombre
// de fabrications possibles, la recette dès la sélection, la chance exacte, les
// matériaux optionnels de la forge avec leur prix en réussite, l'enclume
// retenue, la série et la relance.
//
// 🔴 ELLE NE DÉCIDE DE RIEN, comme sa voisine d'affinage : la partie compose ce
// qu'il faut montrer, déjà traduit et déjà coloré, et la fenêtre rend des verbes.

inline constexpr const char* kMakeItemWindowId = "make_item";

inline constexpr std::size_t kMakeFilterBytes = 64;
inline constexpr std::size_t kForgeSlotCount = 3;
// La série : de un à neuf cent quatre-vingt-dix-neuf tours.
inline constexpr int kLeastBatch = 1;
inline constexpr int kMostBatch = 999;

// UN PRODUIT PROPOSÉ.
struct MakeItemRowView {
  std::uint32_t item_id = 0;
  std::string_view name;
  SkinImage icon;
  int owned = 0;
  // Combien de fois il est fabricable avec le sac. Zéro ou moins : un tiret —
  // mieux vaut se taire que mentir.
  int craftable = -1;
};

// UNE LIGNE DE TEXTE COMPOSÉE PAR LA PARTIE : son texte, le rôle de sa couleur,
// et, s'il y en a une, l'aide au survol d'un « (?) » posé à sa suite.
//
// ⚠ LE TITRE D'AIDE EST DÉJÀ TRADUIT ; LES LIGNES SONT DES CLÉS DE CATALOGUE,
// traduites au survol — voir `SkinHelpMarker`. Une ligne calculée passe telle
// quelle.
struct CraftTextLine {
  std::string_view text;
  SkinTone tone = SkinTone::kText;
  // S'enroule-t-elle ? Jamais après un `SameLine` : elle partirait de la marge.
  bool wrapped = false;
  const char* help_title = nullptr;
  std::span<const char* const> help_lines;
  // Un complément posé à la suite, dans sa propre couleur — la maîtrise
  // culinaire après la chance, les enclumes inutiles après la retenue.
  std::string_view suffix;
  SkinTone suffix_tone = SkinTone::kText;
};

// UN MATÉRIAU DE LA RECETTE, sa ligne déjà composée.
struct MakeMaterialView {
  // Zéro : un nom qu'on n'a pas su résoudre — ni icône, ni lien.
  std::uint32_t item_id = 0;
  SkinImage icon;
  std::string_view label;
  bool enough = false;
};

// UN CHOIX POSSIBLE pour un emplacement de forge.
struct ForgeChoiceView {
  std::uint32_t item_id = 0;
  // « Nom  (n) » — le nom, et ce qui reste à poser.
  std::string_view label;
};

struct ForgeSlotView {
  std::uint32_t chosen = 0;
  std::string_view chosen_label;
  std::span<const ForgeChoiceView> choices;
};

// CE QUI EXPLIQUE UNE LISTE VIDE : la ligne d'un produit de la liste d'avant, et
// ce qui lui manque.
struct MakeStaleLineView {
  std::string_view text;
};

struct MakeItemView {
  std::string_view title;
  // Le serveur n'a proposé aucun produit.
  bool empty_list = false;
  std::string_view cant_make;
  std::span<const MakeStaleLineView> stale_lines;
  int stale_more = 0;
  // La liste ne vaut plus côté serveur : elle se grise, et reste lisible.
  bool stale = false;
  std::span<const MakeItemRowView> rows;
  // Une liste de flèches énumère des MATÉRIAUX : la colonne « Faisable » y
  // répéterait « Possédé ».
  bool arrow_list = false;

  // ── LA RECETTE DU PRODUIT CHOISI ─────────────────────────────────────────
  bool recipe_shown = false;
  bool recipe_unknown = false;
  std::string_view recipe_title;
  std::span<const CraftTextLine> chance;
  std::span<const MakeMaterialView> materials;

  // ── LES MATÉRIAUX OPTIONNELS DE LA FORGE ─────────────────────────────────
  bool forge_shown = false;
  std::array<ForgeSlotView, kForgeSlotCount> forge_slots = {};
  std::span<const CraftTextLine> forge_notes;

  // ── LE PIED ──────────────────────────────────────────────────────────────
  // Tout permet de fabriquer, SAUF peut-être la visibilité du choix, que la
  // fenêtre joint.
  bool can_make = false;
  // La liste est consommée : le bouton principal devient « Relancer ».
  bool spent = false;
  bool can_relaunch = false;
  std::string_view relaunch_tip;
  // La ligne « Quantité » : seulement si la relance qui la fait tourner est
  // cochée — sans elle, aucune liste ne reviendrait après la première.
  bool batch_shown = false;
  // Ce qui dit pourquoi « Fabriquer » est grisé, ou le dernier résultat, puis
  // l'état de la relance. ⚠ « MASQUÉ PAR LE FILTRE » N'Y EST PAS : seul le
  // dessin sait ce que la liste a montré, et la fenêtre l'ajoute elle-même.
  std::span<const CraftTextLine> footer_notes;
  // « série : n/m » pendant qu'elle tourne ; vide sinon.
  std::string_view batch_progress;
  std::span<const CraftLogLineView> history;

  // ── LES RÉGLAGES QUI CHANGENT LE DESSIN ─────────────────────────────────
  bool show_owned = true;
  bool show_filter = true;
  bool show_history = false;
  bool log_time = true;
  bool enter_key = false;
  bool hover_preview = true;
};

enum class MakeItemVerb : std::uint8_t {
  kNothing,
  // Fabriquer le produit choisi — bouton, double-clic ou Entrée.
  kMake,
  kRelaunch,
  // 🔴 FERMER RÉPOND AU SERVEUR, comme pour l'affinage.
  kClose,
  // Ouvrir l'Atlas des recettes sur le produit choisi.
  kOpenAtlas,
};

struct MakeItemWindowAction {
  MakeItemVerb verb = MakeItemVerb::kNothing;
  // Un emplacement de forge a changé : lequel, et pour quoi. Zéro vide.
  int forge_slot = -1;
  std::uint32_t forge_item_id = 0;
  // La description d'un objet, par son identifiant.
  std::uint32_t describe_item_id = 0;
  std::uint32_t hovered_id = 0;
  bool open_settings = false;
  // Le choix est-il une ligne RÉELLEMENT montrée ? Un produit masqué par le
  // filtre ne part pas.
  bool selection_visible = false;
};

struct MakeItemWindowState {
  // ⚠ C'EST LA LISTE DU SERVEUR QUI L'OUVRE, pas le joueur.
  bool open = false;
  bool raise = false;
  // Le produit choisi, par son IDENTIFIANT : la liste se retrie. Zéro : aucun.
  std::uint32_t selected = 0;
  std::array<char, kMakeFilterBytes> filter = {};
  // La série voulue : une intention du moment, jamais retenue d'une partie à
  // l'autre.
  int batch_target = kLeastBatch;
};

[[nodiscard]] MakeItemWindowAction DrawMakeItemWindow(MakeItemWindowState& state,
                                                      const MakeItemView& view, const SkinArt& art);

}  // namespace ro::ui
