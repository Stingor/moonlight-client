#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/formats/image.h"
#include "ro/formats/rsw.h"
#include "ro/render/ground_quad.h"
#include "ro/render/skill_area.h"

namespace ro::render {

// ── GREYWORLD : LE MONDE DÉPOUILLÉ ──────────────────────────────────────────
//
// Le pendant, réglable et réversible, des GRF « greyworld » / « darkside » que
// la communauté RO se passe pour la WoE : on ôte le décor qui cache les
// entités, on rend le sol lisible, et on montre où l'on peut MARCHER. Tout se
// fait sur les fichiers déjà lus, donc sur toutes les cartes, et s'éteint d'un
// clic.
//
// Quatre leviers sous un interrupteur commun :
//
//   1. DÉCORS — les modèles du .rsw ne se peignent plus, ni ses objets d'effet
//      (torches, fumées, lueurs), ni le ciel animé (nuages, brume, étoiles).
//      Les monstres et PNJ en modèle ne sont pas concernés : ce sont des
//      acteurs, pas du décor.
//   2. QUADRILLAGE — une case par cellule de la grille, colorée selon qu'on
//      peut y marcher, que c'est un mur, ou que c'est infranchissable mais
//      tirable, posée sur le relief comme le curseur au sol.
//   3. SOL UNI — le terrain repeint d'une couleur, sans texture ni éclairage.
//   4. APLATIR — le relief ramené à une seule altitude, au chargement de la
//      carte.
//
// ⚠ PAS LE BROUILLARD : il a déjà sa bascule, et une seconde case pour un même
// état n'aurait rien qui les tienne d'accord.

// LES CASES QU'ON PEINT : toutes, ou seulement celles qui bordent un
// changement de terrain — la silhouette des obstacles.
//
// 🔴 PAS DE « UNE CASE SUR DEUX » : un filtre qui saute des cases sans regarder
// leur type peut sauter un MUR, donc mentir sur le terrain. Le contour garde
// toujours les bords : il retire de la matière sans jamais perdre un obstacle.
//
// ⚠ L'ORDRE EST CELUI DU FICHIER DE RÉGLAGES : on ajoute à la fin. Et le type
// est `int` parce que le rang s'y déclare en `int*` — voir `SkillAreaPattern`.
enum class GreyWorldFill : int {
  kAll = 0,
  kOutline,
};

// LES TROIS FAMILLES D'UNE CASE. C'est ce qui décide de la couleur, et c'est
// la frontière que suit le contour : un changement de type à l'intérieur d'une
// même famille ne se voit pas et n'intéresse personne.
enum class CellFamily : std::uint8_t {
  kWalkable,
  kWall,
  // Infranchissable, mais les sorts et les flèches passent : un vide, une
  // dénivellation.
  kSnipeable,
};

// La famille d'un type de cellule tel que le fichier l'écrit. ⚠ LE DRAPEAU
// D'EAU NE COMPTE PAS : il ne change rien à un mur, et une cellule marchable
// sous l'eau reste marchable — voir `formats::kGatWaterFlag`.
[[nodiscard]] CellFamily CellFamilyOf(std::int32_t file_type) noexcept;

// Les bornes des réglages, tenues ici parce que c'est ici qu'elles veulent dire
// quelque chose. Elles bornent le DESSIN et non la saisie : un fichier de
// réglages écrit à la main n'est pas passé par le curseur.
//
// La portée est le seul réglage qui coûte : le nombre de cases croît au CARRÉ
// (24 en vaut 2 401, 60 en vaut 14 641).
inline constexpr std::int32_t kSmallestGreyWorldRadiusCells = 8;
inline constexpr std::int32_t kLargestGreyWorldRadiusCells = 60;
inline constexpr float kSmallestGreyWorldGapPercent = 0.0F;
inline constexpr float kLargestGreyWorldGapPercent = 40.0F;

// LES COULEURS PAR DÉFAUT, empaquetées comme `SkillAreaLook::color` — alpha,
// bleu, vert, rouge du poids fort au faible.
inline constexpr std::uint32_t kDefaultGreyWorldGroundColor = 0xFF2E2A2AU;  // ardoise
inline constexpr std::uint32_t kDefaultGreyWorldWalkColor = 0x29FFFFFFU;    // trait fin
inline constexpr std::uint32_t kDefaultGreyWorldWallColor = 0x8C404FFFU;
inline constexpr std::uint32_t kDefaultGreyWorldSnipeColor = 0x8C40BFFFU;

// Ce que le joueur règle.
struct GreyWorldLook {
  bool enabled = false;
  bool hide_models = true;
  // LA LIGHTMAP AVEC EUX : l'ombre et la lumière cuites dans le sol, dont une
  // bonne part est celle des décors. Sans eux, leurs ombres resteraient au sol,
  // détachées de tout. Ne vaut que si les décors sont masqués.
  bool hide_lightmap = true;
  // Les objets d'effet du .rsw, ni peints ni semés, et le ciel animé de la
  // carte. Les effets portés par un acteur, et la météo de la carte, restent.
  bool hide_map_effects = true;
  bool grid = true;
  bool flat_ground = true;
  // 🔴 IL EMPORTE LE MASQUAGE DES DÉCORS, et ce n'est pas un choix offert au
  // joueur : un décor garde sa hauteur d'origine, et sur un sol aplati il
  // flotte ou s'enfonce. Aplatir en gardant le décor est un état CASSÉ, qu'on
  // ne rend pas atteignable. Voir `GreyWorldHidesModels`.
  bool flatten = false;
  // Demi-côté, en cellules, du carré dessiné autour du joueur.
  std::int32_t radius_cells = 24;
  // Le carreau plein et son joint : le vrai greyworld.
  SkillAreaPattern pattern = SkillAreaPattern::kSolid;
  GreyWorldFill fill = GreyWorldFill::kAll;
  // Le JOINT, en pourcentage du côté de la case : de combien le carreau plein
  // se rétrécit pour laisser voir le sol tout autour. C'est ce vide qui dessine
  // la bordure.
  float gap_percent = 12.0F;
  std::uint32_t ground_color = kDefaultGreyWorldGroundColor;
  std::uint32_t walk_color = kDefaultGreyWorldWalkColor;
  std::uint32_t wall_color = kDefaultGreyWorldWallColor;
  std::uint32_t snipe_color = kDefaultGreyWorldSnipeColor;

  friend bool operator==(const GreyWorldLook&, const GreyWorldLook&) = default;
};

// Ce que chaque levier demande, l'interrupteur commun compris.
[[nodiscard]] bool GreyWorldHidesModels(const GreyWorldLook& look) noexcept;
[[nodiscard]] bool GreyWorldHidesLightmap(const GreyWorldLook& look) noexcept;
[[nodiscard]] bool GreyWorldHidesMapEffects(const GreyWorldLook& look) noexcept;
[[nodiscard]] bool GreyWorldShowsGrid(const GreyWorldLook& look) noexcept;
[[nodiscard]] bool GreyWorldPaintsGround(const GreyWorldLook& look) noexcept;
[[nodiscard]] bool GreyWorldFlattens(const GreyWorldLook& look) noexcept;

// La couleur du sol uni, en trois canaux 0..1 — ce que `TerrainStages` attend.
// ⚠ L'ALPHA NE PEINT PAS : la passe du terrain est opaque.
[[nodiscard]] std::array<float, 3> GreyWorldGroundColor(const GreyWorldLook& look) noexcept;

// La part de la case que le carreau occupe, joint déduit. Un pour les deux
// motifs qui dessinent un contour : les creuser les rapetisserait au lieu de
// les espacer.
[[nodiscard]] float GreyWorldCellFill(const GreyWorldLook& look) noexcept;

// La texture d'un motif. Les équerres et le cadre sont ceux de la zone des
// sorts ; le carreau plein, lui, est un APLAT, un seul texel blanc opaque
// étiré sur la case — la couleur vient de la teinte du quad. Son nom n'est pas
// un chemin d'archive, mais la clé de son téléversement.
[[nodiscard]] std::string_view GreyWorldTexturePath(SkillAreaPattern pattern) noexcept;
[[nodiscard]] bool GreyWorldIsPainted(std::string_view texture_path) noexcept;
[[nodiscard]] formats::Image ComposeGreyWorldTile();

// Y a-t-il du sol sous cette cellule ? Un cube du .gnd couvre deux cellules
// sur deux, et c'est sa face du dessus qui dit si le sol existe.
//
// 🔴 LA GRILLE DÉCRIT TOUT LE RECTANGLE, Y COMPRIS CE QUI N'EST PAS DU SOL. Une
// carte à trous — gonryun est une île dans le ciel — a des cellules au-dessus
// du VIDE : sans ce filtre, le quadrillage s'y poserait, et sur un relief
// aplati il flotterait dans le ciel au-delà du bord de l'île.
[[nodiscard]] bool GroundHasSurface(const formats::GndMap& gnd, std::int32_t cell_x,
                                    std::int32_t cell_y) noexcept;

// LES CASES DU QUADRILLAGE autour de la cellule centrale, une par cellule à
// peindre, déjà posées sur le relief. `quads` est vidé puis rempli : on le
// garde d'une image à l'autre pour ne pas réallouer.
//
// Une famille dont la couleur est TRANSPARENTE ne produit aucune case : mettre
// « marchable » à zéro ne laisse que les obstacles, et c'est la façon la moins
// chère d'afficher le quadrillage.
void ComposeGreyWorldCells(const formats::GndMap& gnd, const formats::GatMap& gat,
                           std::int32_t center_x, std::int32_t center_y, const GreyWorldLook& look,
                           std::vector<GroundQuad>& quads);

// ── APLATIR ─────────────────────────────────────────────────────────────────
//
// Le niveau commun du terrain aplati, dans l'unité et le sens du fichier : la
// MOYENNE DES COINS DES SEULS CUBES QUI ONT DU SOL.
//
// 🔴 PAS LA MOYENNE DE TOUT. Sur une carte à trous, le vide est MAJORITAIRE et
// ses hauteurs plongent très au-dessous du sol : sur gonryun, la moyenne de
// tous les cubes posait le plan à plus de six cellules sous l'île. Sur une
// carte pleine, les deux se confondent.
//
// ⚠ PAS ZÉRO NON PLUS : le niveau d'eau du .rsw n'est pas touché, et un sol à
// zéro s'en décrocherait. Sur une carte qui a de l'eau, le sol aplati peut
// passer dessous.
//
// Rien quand aucun cube n'a de face du dessus : il n'y a pas de sol à niveler.
[[nodiscard]] std::optional<float> FlatReliefLevel(const formats::GndMap& gnd);

// REPOSE LES OBJETS D'EFFET de la carte sur le sol aplati : chacun se déplace
// de l'écart entre le sol d'origine sous lui et `level`, et garde ainsi sa
// hauteur au-dessus du sol. ⚠ `ground` EST LE RELIEF D'ORIGINE : l'appeler
// après avoir aplati ne déplacerait rien.
//
// ⚠ SAUF AU-DESSUS DU VIDE : un cube sans face du dessus garde des hauteurs qui
// plongent loin sous le sol, et l'objet serait relevé de toute leur profondeur.
// Il reste où il est.
void ReseatMapEffects(formats::RswScene& rsw, const formats::GndMap& ground, float level);

}  // namespace ro::render
