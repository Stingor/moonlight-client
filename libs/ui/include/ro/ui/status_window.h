#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/character_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/stat_preset_window.h"

namespace ro::ui {

// LA PETITE FENÊTRE DE STATUT : les six caractéristiques, leurs flèches de
// montée, et les dérivées du combat, sur le fond peint du jeu.
//
// ⚠ ELLE VIT À CÔTÉ DE LA FICHE, elle ne la remplace pas. La fiche dit tout,
// et longuement ; celle-ci tient en 302 pixels, se laisse ouvrir en jouant,
// et ne montre que ce qu'on regarde en montant un personnage.
//
// 🔴 LES LIBELLÉS SONT PEINTS DANS LE FOND, et le code n'écrit que les
// nombres, aux pixels où l'art leur laisse la place. Sans le fond — une
// installation qui ne le porte pas —, la fenêtre écrit elle-même les
// libellés aux mêmes places, et reste utilisable.
//
// ⚠ ELLE NE POSSÈDE AUCUN ÉTAT DE JEU : elle reçoit des nombres déjà dérivés,
// et rend la caractéristique que le joueur veut monter. C'est `app` qui parle
// au serveur, par la même route que le « + » de la fiche.

// LE TITRE ET L'IDENTITÉ. ⚠ L'IDENTITÉ NE SE TRADUIT PAS : c'est elle qui porte
// la place que le joueur a donnée à la fenêtre.
inline constexpr const char* kStatusWindowTitle = core::TrText("Statut");
inline constexpr const char* kStatusWindowId = "status";

// ── LA GÉOMÉTRIE, EN PIXELS D'ART ───────────────────────────────────────────
//
// L'origine est le coin haut-gauche du FOND, sous la barre de titre. Tout se
// multiplie par l'échelle de l'interface.

// La largeur du fond, et celle de la fenêtre quand il manque.
inline constexpr float kStatusArtWidthPixels = 302.0F;
// La hauteur du fond, et celle du corps quand il manque. ⚠ LA BARRE DE TITRE
// S'Y AJOUTE À SA HAUTEUR RÉELLE, que l'art et la police décident.
inline constexpr float kStatusBodyPixels = 115.0F;

// Les six caractéristiques, puis la rangée des points et de la guilde.
inline constexpr std::size_t kStatusAttributeCount = 6;
inline constexpr std::size_t kStatusRowCount = kStatusAttributeCount + 1;

// LE CENTRE DE CHAQUE RANGÉE, là où l'art peint son libellé. Les deux
// colonnes partagent les mêmes.
inline constexpr std::array<float, kStatusRowCount> kStatusRowCenterPixels = {
    12.0F, 28.0F, 44.0F, 60.0F, 76.0F, 92.0F, 106.0F};

// LA FLÈCHE DE MONTÉE : son coin haut-gauche, sa taille, et ce qui la sépare
// du centre de sa rangée.
inline constexpr float kStatusArrowLeftPixels = 88.0F;
inline constexpr float kStatusArrowPixels = 11.0F;
inline constexpr float kStatusArrowAboveCenterPixels = 5.0F;

// CE QUE LA FENÊTRE SAIT DU MONDE, recopié par l'appelant à chaque image.
//
// ⚠ LES DEUX BORNES DE LA MATK SONT NOMMÉES, et dans l'ordre qu'on lit : le
// serveur envoie cette paire-là à l'envers des autres, et c'est `app` qui la
// remet dans le bon sens. Voir `CharacterStatsView`.
struct StatusWindowView {
  // Force, agilité, constitution, intelligence, dextérité, chance.
  std::array<CharacterAttributeView, kStatusAttributeCount> attributes = {};
  std::int64_t status_point = 0;

  std::int64_t atk1 = 0;
  std::int64_t atk2 = 0;
  std::int64_t matk_min = 0;
  std::int64_t matk_max = 0;
  std::int64_t def1 = 0;
  std::int64_t def2 = 0;
  std::int64_t mdef1 = 0;
  std::int64_t mdef2 = 0;
  std::int64_t hit = 0;
  std::int64_t flee = 0;
  std::int64_t perfect_dodge = 0;
  std::int64_t critical = 0;
  std::int64_t aspd = 0;

  // Le nom de la guilde, VIDE sans guilde. ⚠ IL VIT AUSSI LONGTEMPS QUE LA
  // VUE : c'est l'appelant qui le garde.
  std::string_view guild_name;

  // La fenêtre des préréglages qu'ouvre le bouton de la barre de titre.
  StatPresetWindowView presets;
};

// Ce que la fenêtre garde d'une image à l'autre : son interrupteur, que la
// croix baisse elle-même, et celui des préréglages.
struct StatusWindowState {
  bool open = false;
  StatPresetWindowState presets;
};

enum class StatusVerb : std::uint8_t {
  kNothing,
  // Monter la caractéristique de rang `attribute` de `amount` crans.
  kRaiseAttribute,
};

struct StatusWindowAction {
  StatusVerb verb = StatusVerb::kNothing;
  // De zéro à cinq, dans l'ordre de `StatusWindowView::attributes`.
  std::uint8_t attribute = 0;
  int amount = 0;
  // Ce que la fenêtre des préréglages a demandé.
  StatPresetAction presets;
};

// LA FLÈCHE SE MONTRE-T-ELLE ? Quand le cran suivant a un coût, et que les
// points le paient.
//
// ⚠ UN COÛT NUL LA CACHE : c'est ainsi que le serveur dit « au plafond ». Et
// elle DISPARAÎT au lieu de griser, comme dans le jeu : une flèche éteinte
// promettrait un geste que rien ne rallumera sur une caractéristique au bout.
[[nodiscard]] bool StatusArrowShown(const CharacterAttributeView& attribute,
                                    std::int64_t status_point);

// Le haut de la flèche de la rangée `rank`, en pixels d'art depuis le fond.
[[nodiscard]] float StatusArrowTopPixels(std::size_t rank);

[[nodiscard]] StatusWindowAction DrawStatusWindow(StatusWindowState& state,
                                                  const StatusWindowView& view, const SkinArt& art);

}  // namespace ro::ui
