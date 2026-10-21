#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <imgui.h>

#include "ro/ui/hud_frame.h"

namespace ro::ui {

// ── LES JAUGES LIBRES ───────────────────────────────────────────────────────
//
// Vie, magie, expérience de base, expérience de métier, poids, bourse et
// incantation : sept jauges que le joueur pose où il veut, comme les cadres du
// HUD de cible et par la même brique.
//
// 🔴 CHACUNE EST INDÉPENDANTE — sa place, sa taille, sa couleur, son
// interrupteur — et le VERROU EST COMMUN. C'est le marché de tout HUD ici :
// verrouillé, il se fige et laisse passer les clics au jeu.
//
// ⚠ ELLES NE REMPLACENT PAS LES FENÊTRES : la fiche de personnage et le sac
// disent les mêmes nombres, en plus complet. Ces jauges existent pour qu'on les
// lise SANS ouvrir quoi que ce soit, du coin de l'œil et pendant un combat.

// Les sept, dans l'ordre où elles se persistent. ⚠ NE PAS INTERCALER : l'ordre
// part dans le fichier de réglages, et une valeur glissée au milieu déplacerait
// les jauges déjà posées.
enum class StatBar : std::uint8_t {
  kHealth = 0,
  kMagic = 1,
  kBaseExp = 2,
  kJobExp = 3,
  kWeight = 4,
  kZeny = 5,
  // MON INCANTATION. ⚠ ELLE NE LIT AUCUNE VALEUR DU PERSONNAGE : l'écoulé et
  // la durée viennent du sort en cours, et son texte arrive tout composé. Hors
  // incantation elle ne se dessine PAS — une jauge vide en permanence prendrait
  // la place sans rien dire.
  kCast = 6,
};
inline constexpr std::size_t kStatBarCount = 7;

// Ce que le texte d'une jauge dit. ⚠ MÊME RÈGLE D'ORDRE.
// 🔴 EN `int`, PARCE QU'IL SE PERSISTE. Ce rang part dans le fichier de
// réglages par un `reinterpret_cast<int*>`, et la lecture écrit QUATRE octets :
// sur un `std::uint8_t`, elle écrasait les trois champs suivants de la
// structure. Rien ne le disait — l'ordre de déclaration réparait la plupart des
// cas en relisant ces champs juste après.
enum class StatBarText : int {
  kNothing = 0,
  kPercent = 1,
  kValues = 2,
  kBoth = 3,
  // Les valeurs, puis le pour cent SOUS elles : « 300 / 1200 », « (25 %) ».
  kBothStacked = 4,
};

// LA FORME D'UNE JAUGE. ⚠ MÊME RÈGLE D'ORDRE que le reste : elle part dans le
// fichier de réglages.
//
// 🔴 ET C'EST UN RÉGLAGE PAR JAUGE, non un style commun. La disposition qu'on
// cherche est celle de Diablo : deux ORBES pour la vie et la magie, encadrant des
// BARRES pour l'expérience. Une forme commune aux six interdirait exactement
// l'écran pour lequel les orbes existent.
// ⚠ EN `int` POUR LA MÊME RAISON QUE `StatBarText` : il se persiste.
enum class StatBarShape : int {
  kBar = 0,
  kOrb = 1,
  kRing = 2,
};

// L'épaisseur d'un anneau, en part de son rayon. Bornée : au-delà de la moitié,
// il n'y a plus de trou et l'anneau n'est plus qu'un camembert.
inline constexpr float kThinnestRing = 0.05F;
inline constexpr float kThickestRing = 0.50F;

// Le pas de facettage d'un orbe et d'un anneau, en segments de tour complet.
// Assez pour qu'un bord ne se voie pas anguleux à cent pixels.
inline constexpr int kRoundSegments = 64;

// 🔴 UN ORBE N'EST PAS LE CADRE QUI LE PORTE. Le cadre reste un RECTANGLE — c'est
// par lui qu'on saisit, qu'on retaille et qu'on s'aimante — mais le disque s'y
// inscrit, centré, sur le plus PETIT des deux côtés. Le prendre sur la largeur
// ferait déborder un orbe d'un cadre plus haut que large, et la moitié du
// remplissage sortirait de ce que le joueur a posé.
struct OrbSurface {
  ImVec2 middle{0.0F, 0.0F};
  float radius = 0.0F;
};

[[nodiscard]] OrbSurface OrbSurfaceOf(ImVec2 top_left, ImVec2 bottom_right);

// ── LE BORD D'UN ORBE, ET POURQUOI IL DEMANDE UN CALCUL ────────────────────
//
// 🔴 UN REMPLISSAGE ET UN TRAIT NE S'ARRÊTENT PAS AU MÊME ENDROIT, À LARGEUR
// ÉGALE. ImGui étale un pixel de fondu, et il le prend des deux côtés d'un trait
// mais seulement au DEHORS d'un remplissage (`imgui_draw.cpp` :
// `half_inner_thickness = (thickness - AA_SIZE) * 0.5f` pour l'un,
// `dm *= AA_SIZE * 0.5f` vers l'extérieur pour l'autre). Un disque rempli au
// rayon R est donc OPAQUE jusqu'à R, tandis qu'un filet de deux pixels posé pour
// border ce même disque cesse de l'être à R−0,5.
//
// ⚠ IL RESTE ALORS UN DEMI-PIXEL DE COULEUR VIVE TOUT AUTOUR, que ni l'ombre de
// profondeur ni le filet ne couvrent — un liseré saturé qui suit l'orbe, visible
// surtout quand il est plein. C'est ce que le joueur a vu, et c'était bien « un
// pixel trop tard ».
inline constexpr float kAntiAliasedEdge = 1.0F;

// LE RAYON JUSQU'OÙ UN TRAIT CIRCULAIRE EST VRAIMENT OPAQUE. Au-delà, il n'y a
// plus que son fondu — qui laisse passer ce qu'il y a dessous.
[[nodiscard]] constexpr float OpaqueOuterRadius(float center_radius, float thickness) {
  return center_radius + (thickness - kAntiAliasedEdge) * 0.5F;
}

// DE COMBIEN LE LIQUIDE RENTRE SOUS LA SILHOUETTE. Un pixel : son bord opaque
// passe alors sous le bord opaque du filet, et son propre fondu s'éteint juste
// là où celui-ci commence.
inline constexpr float kOrbEdgeInset = 1.0F;

// LE DISQUE DU LIQUIDE : celui de l'orbe, rentré d'un pixel.
//
// ⚠ TOUT CE QUI EST VIF S'Y RAPPORTE — le liquide, son niveau, l'ombre de
// profondeur, le reflet. Seuls le FOND et le FILET gardent le disque plein :
// c'est le fond qui donne la silhouette, et le filet qui la borde.
[[nodiscard]] OrbSurface OrbLiquidOf(const OrbSurface& disc);

// UN TRAIT CIRCULAIRE, TEL QU'ON LE POSE : son rayon de centre et son épaisseur.
//
// ⚠ SON BORD OPAQUE N'EST PAS `centre + épaisseur / 2`, et c'est tout le piège —
// voir `OpaqueOuterRadius`. Le dessin pose ces deux traits et un banc les compare
// au bord du liquide : c'est la seule façon de PROUVER qu'aucune couleur vive ne
// dépasse, sans avoir un écran sous les yeux.
struct OrbStroke {
  float center_radius = 0.0F;
  float thickness = 0.0F;

  [[nodiscard]] float opaque_outer() const { return OpaqueOuterRadius(center_radius, thickness); }
};

// LE FILET, sur le disque PLEIN : c'est lui qui borde la silhouette.
[[nodiscard]] OrbStroke OrbRimStroke(const OrbSurface& disc);

// UNE BANDE D'OMBRE, sur le disque du LIQUIDE. La bande zéro est posée pour que
// son bord opaque tombe pile sur le bord du liquide ; les suivantes s'enfoncent
// d'une épaisseur chacune.
[[nodiscard]] OrbStroke OrbShadowStroke(const OrbSurface& liquid, int rank);

// De quoi peindre un orbe : sa part, ses teintes, et ses deux interrupteurs.
struct OrbPaint {
  float fill = 0.0F;
  ImU32 ink = 0;
  ImU32 background = 0;
  ImU32 border_color = 0;
  bool glossy = true;
  bool border = true;
};

// L'ORBE ENTIER, dans l'ordre : le fond, le liquide, la profondeur, le reflet,
// le filet.
//
// 🔴 EXPORTÉ POUR ÊTRE MESURÉ, et c'est ce qui distingue cette preuve d'un
// raisonnement. Comparer des rayons ne dit rien si le dessin passe le mauvais
// disque à l'une de ses couches — c'était précisément le défaut. Un banc peint
// donc un orbe dans une liste de dessin nue et relève ses SOMMETS : aucun de
// ceux qui portent la teinte vive ne doit sortir du disque du liquide.
void PaintStatusOrb(ImDrawList& into, const OrbSurface& disc, const OrbPaint& how);

// L'ORDONNÉE DU NIVEAU DANS UN ORBE, pour une part remplie.
//
// ⚠ ELLE MONTE QUAND LA PART MONTE, et c'est le seul sens qui se lise : un orbe
// se vide par le bas comme un verre. À zéro elle est au bas du disque, à un au
// sommet — jamais l'inverse, et jamais au milieu du cadre.
[[nodiscard]] float OrbLevelY(const OrbSurface& disc, float fill);

// La forme, lue d'un entier qui vient du fichier. ⚠ BORNÉE, comme le mode de
// texte : la source n'est pas sous notre contrôle.
[[nodiscard]] StatBarShape StatBarShapeOf(int stored);

// LE PLAFOND DUR DE LA BOURSE : `MAX_ZENY` vaut `INT_MAX` dans
// `src/common/mmo.hpp`, et le serveur refuse de compiler au-delà
// (`#error MAX_ZENY is too big`).
//
// ⚠ IL NE SERT PAS DE DÉNOMINATEUR À LA JAUGE, et c'est une correction. Une
// bourse remplie contre deux milliards est TOUJOURS vide : un million de zenys y
// vaut un vingtième de pour cent, soit rien de visible. La jauge se remplissait
// donc contre un nombre exact et inutile.
inline constexpr std::int64_t kZenyCeiling = 2147483647;

// 🔴 CE CONTRE QUOI LA BOURSE SE REMPLIT VRAIMENT, et pourquoi c'est un RÉGLAGE.
// Une bourse n'a pas de maximum qui veuille dire quelque chose — ni le plafond
// du serveur, ni rien d'autre. Le seul dénominateur honnête est celui que le
// joueur CHOISIT : « plein » veut dire ce qu'il a décidé que ça vaut.
//
// ⚠ ET LE MONTANT S'ÉCRIT QUAND MÊME EN ENTIER : le repère ne borne que le
// remplissage, jamais le chiffre.
inline constexpr std::int64_t kDefaultZenyReference = 1000000;
inline constexpr std::int64_t kSmallestZenyReference = 1000;

// 🔴 LE PLUS HAUT REPÈRE QU'UN CURSEUR PUISSE PORTER, ET CE N'EST PAS LE PLAFOND
// DU SERVEUR. Un curseur compte en flottants, et `2 147 483 647` n'est pas
// représentable en simple précision : le flottant le plus proche est
// `2 147 483 648`, soit UN DE TROP. Le curseur poussé à fond rendait donc ce
// nombre-là, et `static_cast<int>` d'un flottant hors des bornes d'un entier est
// un comportement indéfini — en pratique, le repère basculait à
// « -2 147 483 648 ».
//
// ⚠ LA RÈGLE, ET ELLE VAUT POUR TOUTE BORNE DE CURSEUR : une borne doit
// SURVIVRE À L'ALLER-RETOUR `int → float → int`. Deux milliards le fait
// (2 000 000 000 = 15 625 000 × 2⁷, et 15 625 000 tient dans les vingt-quatre
// bits de mantisse) ; le `static_assert` juste dessous le vérifie à la
// compilation, parce qu'un essai se contourne et pas lui.
inline constexpr std::int64_t kLargestZenyReference = 2000000000;
static_assert(static_cast<std::int64_t>(static_cast<float>(kLargestZenyReference)) ==
                  kLargestZenyReference,
              "une borne de curseur doit survivre à l'aller-retour en flottant");

// LE REPÈRE, LU D'UN FLOTTANT QUI VIENT D'UN CURSEUR.
//
// ⚠ BORNÉ ICI ET NON AU CURSEUR : la valeur vient aussi du fichier de réglages,
// que le joueur peut écrire à la main, et le cast qui suivrait serait le même
// comportement indéfini.
[[nodiscard]] int ZenyReferenceOf(float asked);

// LE MODE DE TEXTE, LU D'UN ENTIER QUI VIENT DU FICHIER DE RÉGLAGES.
//
// ⚠ BORNÉE PARCE QUE LA SOURCE NE L'EST PAS : le joueur peut écrire n'importe
// quoi dans son fichier, et un `static_cast` sur une énumération hors bornes est
// un comportement indéfini, pas une valeur bizarre.
[[nodiscard]] StatBarText StatBarTextOf(int stored);

// Ce qu'une jauge vaut. ⚠ `known` FAUX N'EST PAS ZÉRO : tant que le serveur n'a
// rien dit, une barre vide annoncerait une bourse vide ou une mort.
struct StatBarValue {
  bool known = false;
  std::int64_t current = 0;
  std::int64_t most = 0;
  // LE TEXTE DÉJÀ COMPOSÉ, qui prend la place de celui du mode — vide pour les
  // jauges qui écrivent un nombre. C'est celui de l'incantation : le sort et le
  // temps restant disent plus qu'un pour cent.
  std::string composed;
};

using StatBarValues = std::array<StatBarValue, kStatBarCount>;

// Une jauge : ce que le joueur en a fait.
// UNE JAUGE : TOUT CE QUE LE JOUEUR EN A FAIT.
//
// 🔴 ET C'EST BIEN « TOUT ». L'apparence était commune aux six — une seule
// forme de texte, un seul arrondi, un seul fond —, ce qui interdisait
// exactement l'écran pour lequel les formes existent : deux ORBES sans texte
// encadrant des BARRES qui écrivent leurs valeurs ne se règlent pas avec les
// mêmes nombres. Un réglage commun n'est pas plus simple, il est plus PAUVRE.
//
// ⚠ NE RESTE COMMUN QUE CE QUI L'EST VRAIMENT : le verrou, l'aimantation, le
// repère de pose. Ceux-là décrivent le GESTE, qui est unique parce qu'il n'y a
// qu'une souris.
struct StatBarLook {
  bool shown = true;
  HudRect rect;
  ImU32 fill = IM_COL32(200, 200, 205, 255);
  StatBarShape shape = StatBarShape::kBar;

  // LE SENS INVERSÉ : une barre se remplit depuis la droite (ou depuis le haut),
  // un anneau tourne dans le sens inverse des aiguilles.
  //
  // ⚠ PAR JAUGE, parce que c'est une affaire de SYMÉTRIE : une disposition en
  // miroir — la vie à gauche, la magie à droite — veut que les deux se
  // remplissent vers le centre, donc en sens contraire l'une de l'autre.
  // ⚠ UN ORBE L'IGNORE : il se remplit par le bas, et la pesanteur n'a qu'un
  // sens.
  bool reversed = false;

  // Le remplissage monte au lieu d'aller vers la droite. ⚠ NE VAUT QUE POUR UNE
  // BARRE : un orbe se remplit toujours par le bas, un anneau toujours depuis
  // midi.
  bool vertical = false;

  // 🔴 LE RELIEF, ET IL VAUT POUR LES TROIS FORMES. Sur un orbe : un liquide
  // plus clair à sa surface que dans sa profondeur, un trait de surface, un bord
  // qui s'assombrit vers le tour et un reflet en haut. Sur une barre : le même
  // dégradé et un fil de lumière sur son arête haute. Sur un anneau : un biseau.
  // Sans lui, un orbe se lit comme une pastille plate — et c'est le relief, pas
  // la rondeur, qui fait reconnaître la jauge de Diablo.
  bool glossy = true;

  // Ce que cette jauge écrit, et à quelle taille.
  StatBarText text = StatBarText::kPercent;
  float font_pixels = 13.0F;
  // LE NOM COURT DEVANT LE TEXTE — « HP 45 % » plutôt que « 45 % ». Allumé
  // par défaut, il s'éteint au réglage. ⚠ LA BOURSE N'EN A PAS BESOIN : son
  // « z » dit déjà ce qu'elle compte.
  bool label = true;

  // ⚠ L'ARRONDI NE VAUT QUE POUR UNE BARRE : un disque n'a pas de coin.
  float rounding = 4.0F;
  // ⚠ ET L'ÉPAISSEUR QUE POUR UN ANNEAU, en part de son rayon.
  float ring_thickness = 0.22F;

  bool border = true;
  ImU32 background = IM_COL32(13, 13, 18, 179);
  // La teinte du texte, et celle du filet. ⚠ LE FILET SE PEINT APRÈS LE
  // REMPLISSAGE, par la jauge et non par son cadre : peint avant, une jauge
  // pleine et arrondie le recouvrait.
  ImU32 text_color = IM_COL32_WHITE;
  ImU32 border_color = IM_COL32(0, 0, 0, 160);

  // LE DÉCALAGE DU TEXTE depuis le centre de la jauge, EN PART DE SA TAILLE :
  // la largeur pour `text_shift_x`, la hauteur pour `text_shift_y`. Zéro le
  // laisse au centre ; un pousse d'une largeur entière vers la droite (ou d'une
  // hauteur vers le bas).
  //
  // 🔴 UNE PART ET NON DES PIXELS : le texte suit la jauge qu'on retaille, et
  // l'échelle du HUD avec elle, sans qu'aucun calcul ne s'en charge à part.
  float text_shift_x = 0.0F;
  float text_shift_y = 0.0F;
};

// Jusqu'où le texte se décale, en tailles de jauge, de part et d'autre : assez
// pour le poser à côté d'une jauge étroite, pas assez pour le perdre à l'écran.
inline constexpr float kLargestTextShiftShare = 2.0F;

// LE DÉCALAGE DU TEXTE EN PIXELS pour une jauge de `size` pixels à l'écran,
// borné à `kLargestTextShiftShare` : le fichier de réglages s'écrit à la main.
[[nodiscard]] ImVec2 StatBarTextShift(const StatBarLook& look, ImVec2 size);

// RECOPIE LE STYLE D'UNE JAUGE SUR LES SIX AUTRES.
//
// 🔴 LE STYLE, ET NON L'IDENTITÉ. Ce qui part : ce qui décrit une MANIÈRE de
// dessiner — le texte, le fond, le filet, le relief, l'arrondi. Ce qui reste à
// chacune : sa place — celle de son texte comprise —, sa visibilité, sa FORME,
// sa COULEUR et son SENS. Le décalage du texte reste parce qu'il se règle
// contre la forme : celui qui sort le texte d'un orbe le jetterait loin d'une
// barre plate. Recopier
// la forme mettrait six orbes d'un clic, ce qui détruit précisément la
// disposition que les formes servent à bâtir ; recopier la couleur obligerait à
// lire un chiffre pour savoir quelle jauge on regarde.
//
// ⚠ SANS CE GESTE, RENDRE SIX JAUGES SEMBLABLES DEMANDE SOIXANTE CLICS — et
// c'est le prix qu'on paie pour avoir rendu l'apparence granulaire. Les barres
// de raccourcis ont exactement le même bouton, pour exactement cette raison.
void CopyStatBarStyle(std::array<StatBarLook, kStatBarCount>& bars, std::size_t from);

// LA DISPOSITION D'ORIGINE DES SEPT.
//
// 🔴 ELLE N'EST PAS UNE COMMODITÉ. Sans elle, toutes naissent au MÊME endroit —
// le rectangle par défaut d'un cadre — et s'empilent à l'angle de l'écran : le
// joueur en voit UNE, la dernière dessinée, et n'a aucune raison de deviner que
// les autres sont dessous. Un réglage qu'on ne peut pas trouver n'existe
// pas.
//
// ⚠ ET CHACUNE NAÎT AVEC SA TEINTE : rouge pour la vie, bleu pour la magie, or
// pour le métier. Six jauges de la même couleur demanderaient de lire
// leur chiffre pour savoir laquelle est laquelle — c'est-à-dire de renoncer au
// coup d'œil qui les justifie.
[[nodiscard]] std::array<StatBarLook, kStatBarCount> DefaultStatBars();

// CE QUI VAUT POUR LES SEPT, ET RIEN D'AUTRE.
//
// 🔴 LE CRITÈRE N'EST PAS « ÇA SE RESSEMBLE », C'EST « ÇA NE PEUT PAS DIFFÉRER ».
// Le verrou, l'aimantation et le repère de pose décrivent le GESTE : il n'y a
// qu'une souris, donc un seul verrou. Les deux derniers ne concernent qu'UNE
// jauge chacun, et leur rangée vit dans l'onglet de celle-là — c'est la place
// qui dit à qui un réglage s'applique.
struct StatBarsLook {
  // ⚠ OPT-IN. Un HUD qui surgit sans qu'on l'ait demandé se prend pour un défaut.
  bool shown = false;
  bool locked = false;
  bool sticky = true;

  // ⚠ LE REPÈRE DE CENTRE EST UN OUTIL DE POSE, ÉTEINT PAR DÉFAUT. Allumé en
  // permanence, il pose une croix claire au milieu de chaque jauge déverrouillée
  // — que l'on prend pour un défaut d'affichage bien avant de comprendre qu'elle
  // sert à viser. Il ne paraît qu'au déverrouillage, et seulement si on le
  // demande.
  bool center_mark = false;

  // Au niveau maximum, les jauges d'expérience restent au lieu de s'effacer.
  bool keep_maxed_exp = false;
  // LA FENÊTRE « BASIC INFO » DU JEU, à côté des jauges et réglée dans leur
  // page. ⚠ ÉTEINTE D'USINE : la disposition d'usine pose déjà les jauges
  // libres, et les deux ensemble montreraient la vie deux fois.
  bool basic_info_window = false;

  // LE MODE RÉGLAGE : des valeurs factices qui ondulent, du vide au plein, pour
  // voir en direct ce qu'un réglage fait à une jauge qui bouge — l'arrondi d'une
  // part presque vide, le texte d'une jauge pleine, la bascule de surcharge.
  // ⚠ PAS DANS LE FICHIER DE RÉGLAGES, et sans effet panneau fermé : une vie
  // factice affichée en combat serait un mensonge dangereux.
  bool tuning = false;

  // Ce que « bourse pleine » veut dire. Voir `kDefaultZenyReference`. ⚠ IL NE
  // CONCERNE QUE LA BOURSE, et sa rangée est dans l'onglet de la bourse.
  int zeny_reference = static_cast<int>(kDefaultZenyReference);

  // 🔴 LA TEINTE DE SURCHARGE, ET C'EST NOTRE AJOUT. Au-delà du seuil que le
  // SERVEUR annonce, la régénération naturelle s'arrête ; plus haut encore, on
  // ne ramasse plus rien. Une jauge de poids qui ne dit pas où bascule le jeu
  // oblige à ouvrir le sac pour le savoir — or c'est justement ce qu'elle existe
  // pour éviter. Le seuil ne se code pas en dur : il vient de
  // `PlayerStatus::overweight_percent`.
  //
  // ⚠ NE CONCERNE QUE LE POIDS, comme le repère ci-dessus ne concerne que la
  // bourse.
  bool warn_overweight = true;

  std::array<StatBarLook, kStatBarCount> bars = DefaultStatBars();
};

// LE NOM COURT D'UNE JAUGE, celui qui précède son texte. Vide pour la bourse :
// son « z » est déjà son nom. ⚠ DÉJÀ TRADUIT.
[[nodiscard]] const char* StatBarShortName(StatBar bar);

// LE SEUIL DE SURCHARGE, EN POUR CENT, tel que le serveur l'annonce. Zéro quand
// il ne l'a pas dit — et la jauge ne colore alors rien plutôt que de deviner.
struct StatBarWarnings {
  std::int64_t overweight_percent = 0;
};

// Le texte d'une jauge, dans le mode demandé.
//
// ⚠ SÉPARÉ DU DESSIN POUR ÊTRE ESSAYÉ. C'est la seule part de ces jauges qu'un
// banc puisse prouver, et c'est celle où chaque unité a sa règle : le poids se
// dit en dixièmes, la bourse par milliers, l'expérience en nombres qui débordent
// de trente-deux bits.
[[nodiscard]] std::string StatBarCaption(StatBar bar, const StatBarValue& value, StatBarText mode,
                                         bool labelled = false);

// La part remplie, entre 0 et 1.
[[nodiscard]] float StatBarFill(const StatBarValue& value);

// Au niveau maximum, le serveur donne pour seuil du niveau suivant ces plafonds
// au lieu d'un vrai seuil : c'est à eux qu'on reconnaît le niveau maximum, quel
// qu'il soit — 999 en base, celui de la classe pour le métier.
inline constexpr std::int64_t kBaseExpCeiling = 99'999'999;
inline constexpr std::int64_t kJobExpCeiling = 999'999'999;

// VRAI SI LA JAUGE PARAÎT, réglage à part : toutes, sauf l'incantation quand on
// n'incante pas, et, sauf `keep_maxed_exp`, l'expérience de base ou de métier
// au niveau maximum.
[[nodiscard]] bool StatBarPresent(StatBar bar, const StatBarValue& value,
                                  bool keep_maxed_exp = false);

// Vrai quand ce poids-là a franchi le seuil que le serveur annonce.
[[nodiscard]] bool StatBarOverweight(const StatBarValue& weight, const StatBarWarnings& warnings);

// Dessine les sept. `look` est modifié quand le joueur déplace une jauge, et
// `*moved` posé dans ce cas : l'appelant ne persiste qu'une fois.
// ⚠ LA GRILLE SE PASSE, ELLE NE SE RANGE PAS. `StatBarsLook` est un réglage
// PERSISTÉ : y loger un pointeur, c'est y loger une durée de vie que le fichier
// ne sait pas écrire, et qu'aucun défaut ne rappelle. Elle traverse donc en
// paramètre, comme tout ce qui ne survit pas à l'image.
void DrawStatBars(const StatBarValues& values, const StatBarWarnings& warnings,
                  const AlignGrid& grid, StatBarsLook& look, HudGrip& grip, bool* moved = nullptr);

}  // namespace ro::ui
