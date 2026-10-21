#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA FENÊTRE « BASIC INFO » DU JEU : pseudo, classe, niveaux, vie, magie,
// expérience, zeny et poids, en deux tailles — une barre de deux lignes, ou le
// panneau qui détaille tout.
//
// 🔴 C'EST LA COPIE DE CELLE DU JEU, AU PIXEL : sa taille, ses fonds, ses
// jauges, ses textes et leurs formats. Les jauges libres et le portrait
// existent à côté pour qui veut un autre écran ; celle-ci est pour qui veut
// retrouver le sien.
//
// ⚠ LA BARRE DE TITRE EST LA SEULE PRISE : on déplace la fenêtre par elle, et un
// double-clic dessus la replie ou la déplie, comme son bouton.
//
// ⚠ ELLE NE POSSÈDE AUCUN ÉTAT DE JEU : elle reçoit des nombres et des noms, et
// ne rend que le coin sous lequel la grille d'icônes de menu s'accroche.

// L'IDENTITÉ DE LA FENÊTRE, qui porte sa place. ⚠ ELLE NE SE TRADUIT PAS.
inline constexpr const char* kBasicInfoWindowId = "basic_info";
// LE RÉGLAGE DE SA TAILLE, retenu avec la disposition du personnage.
inline constexpr const char* kBasicInfoExpandedKey = "basic_info.expanded";

// ── LA GÉOMÉTRIE, EN PIXELS D'ART ───────────────────────────────────────────
//
// L'origine est le coin haut-gauche de la fenêtre, barre de titre comprise :
// le jeu dessine sa barre dans la fenêtre, et tout ce qui suit s'y mesure.
inline constexpr float kBasicInfoWidthPixels = 220.0F;
inline constexpr float kBasicInfoCollapsedPixels = 55.0F;
// 🔴 DÉPLIÉE, UNE LIGNE DE PLUS QUE LE JEU : le zeny passe sous le poids, au lieu
// de partager sa ligne. Le jeu les met côte à côte, et la ligne déborde à gauche
// dès que les deux nombres sont longs — un zeny au plafond n'en laisse que
// « s : ». Écart choisi par le joueur.
inline constexpr float kBasicInfoGameExpandedPixels = 134.0F;
inline constexpr float kBasicInfoPurseLinePixels = 12.0F;
inline constexpr float kBasicInfoExpandedPixels =
    kBasicInfoGameExpandedPixels + kBasicInfoPurseLinePixels;
inline constexpr float kBasicInfoTitleBarPixels = 17.0F;

// LE SEUIL DE LA COULEUR D'ALERTE de la vie et de la magie : sous un quart, la
// jauge et le texte passent au rouge.
inline constexpr double kBasicInfoLowShare = 0.25;

// CE QUE LA FENÊTRE SAIT DU MONDE, recopié par l'appelant à chaque image.
//
// ⚠ UNE VALEUR INCONNUE VAUT ZÉRO, et c'est le maximum nul qui dit « rien à
// montrer » : une jauge à zéro sur zéro ne se remplit ni n'écrit, comme dans le
// jeu avant le premier paquet.
struct BasicInfoView {
  std::string name;
  // La classe exercée, nommée comme la fiche la nomme.
  std::string job;
  std::int64_t base_level = 0;
  std::int64_t job_level = 0;

  std::int64_t hp = 0;
  std::int64_t max_hp = 0;
  std::int64_t sp = 0;
  std::int64_t max_sp = 0;

  // ⚠ LE PALIER SUIVANT VAUT UNE SENTINELLE AU NIVEAU MAXIMUM — voir
  // `kBaseExpCeiling` et `kJobExpCeiling` : c'est elle qui efface la jauge.
  std::int64_t base_exp = 0;
  std::int64_t next_base_exp = 0;
  std::int64_t job_exp = 0;
  std::int64_t next_job_exp = 0;

  std::int64_t zeny = 0;
  // ⚠ EN DIXIÈMES, comme le serveur l'envoie : la fenêtre écrit des unités.
  std::int64_t weight_tenths = 0;
  std::int64_t max_weight_tenths = 0;
  // Le pour cent de charge où le poids passe au rouge ; zéro tant que le
  // serveur ne l'a pas dit.
  std::int64_t overweight_percent = 0;

  // LA TOUCHE QUI REPLIE ET DÉPLIE, dite par la bulle du bouton. Vide sans
  // touche.
  std::string toggle_keys;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct BasicInfoWindowState {
  // Le réglage l'allume ET un personnage est en jeu. Faux, rien ne se dessine
  // et la réponse ne porte aucun coin.
  bool shown = false;
  // ⚠ DÉPLIÉE D'ABORD : c'est sous cette forme qu'un joueur la reconnaît.
  bool expanded = true;
  // LE COIN HAUT-GAUCHE À GARDER à l'image qui suit un repli : la fenêtre
  // grandit et rétrécit par le bas, où qu'elle soit posée.
  std::optional<ImVec2> keep_top_left;
};

struct BasicInfoAnswer {
  // LE COIN BAS-GAUCHE DE LA FENÊTRE, à l'écran, quand elle est montrée : la
  // grille d'icônes de menu s'y accroche.
  std::optional<ImVec2> dock;
};

// ── LES RÈGLES, séparées du dessin pour s'éprouver sans écran ──────────────

// LE POURCENTAGE D'EXPÉRIENCE DE LA BARRE REPLIÉE, tronqué au dixième.
//
// ⚠ AU NIVEAU MAXIMUM IL VAUT 100 : la sentinelle du palier suivant n'est pas un
// palier, et diviser par elle écrirait un pour cent qui ne monte plus.
[[nodiscard]] double BasicInfoExpPercent(std::int64_t exp, std::int64_t next);

// LA JAUGE D'EXPÉRIENCE SE MONTRE-T-ELLE ? Pas au niveau maximum, ni quand le
// palier est sous l'acquis.
[[nodiscard]] bool BasicInfoExpBarShown(std::int64_t exp, std::int64_t next);

// LA LARGEUR DU REMPLISSAGE d'une jauge, en pixels d'art, tronquée.
[[nodiscard]] float BasicInfoFill(std::int64_t current, std::int64_t most, float span);

// LE ROUGE D'ALERTE de la vie ou de la magie : sous le quart, maximum connu.
[[nodiscard]] bool BasicInfoLow(std::int64_t current, std::int64_t most);

// LE POIDS AU ROUGE : au seuil du serveur ou au-delà, une fois ce seuil dit.
[[nodiscard]] bool BasicInfoOverweight(std::int64_t weight, std::int64_t max_weight,
                                       std::int64_t overweight_percent);

// UN NOMBRE GROUPÉ PAR MILLIERS, à la virgule, comme le jeu écrit les zeny.
[[nodiscard]] std::string BasicInfoGrouped(std::int64_t value);

[[nodiscard]] BasicInfoAnswer DrawBasicInfoWindow(BasicInfoWindowState& state,
                                                  const BasicInfoView& view, const SkinArt& art);

}  // namespace ro::ui
