#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/formats/palette.h"
#include "ro/formats/spr.h"

namespace ro::render {

// LES RAMPES D'UNE PALETTE DE CORPS, et la recoloration par décalage HSV.
//
// Une palette RO n'est pas 256 couleurs indépendantes : c'est une suite de
// DÉGRADÉS, un par pièce du costume — la tunique, la cape, les bottes. Teindre
// une pièce, c'est faire pivoter la teinte de SA rampe ; le contraste et les
// reflets, eux, restent ceux du dessin.
//
// 🔴 CE QUI VOYAGE NE PORTE AUCUNE COULEUR. Le fil ne transmet que des
// DÉCALAGES par rampe (`CZ_MOONLIGHT_STYLE 0x0F26`, cinq octets par rampe), et
// chaque client recalcule la palette depuis le `.spr` qu'il possède déjà. Deux
// clients doivent donc trouver EXACTEMENT les mêmes rampes sur les mêmes
// octets, sinon deux joueurs se voient dans des couleurs différentes sans que
// rien ne le signale.
//
// 🔴 D'OÙ LES RÈGLES D'ÉCRITURE DE CE MODULE, qui ne sont pas des goûts :
//
//   * aucun flottant dans une clé de TRI — le classement décide quelles pièces
//     reçoivent un curseur, et deux implémentations départageraient les ex
//     æquo autrement ;
//   * un tri STABLE, pour la même raison ;
//   * l'arrondi est `int(x + 0,5)` et jamais celui de la bibliothèque : le
//     `round()` de Python est BANCAIRE (`round(0,5) == 0`), et la référence
//     s'aligne sur celui-ci.
//
// LA RÉFÉRENCE EST `palette_ramps.py`, validée sur les corps du client. Toute
// modification se fait DES DEUX CÔTÉS, et `scripts/xcheck_palette_ramps.py`
// le prouve corps par corps — 528 sur cette installation, entrées et sorties
// identiques. ⚠ CE CONTRÔLE NE PEUT PAS DEVENIR UN ESSAI DU DÉPÔT : il lui
// faut de vrais sprites, et aucun asset du jeu n'est versionné. Les RÈGLES,
// elles, s'éprouvent sur des palettes synthétiques dans
// `tests/render/palette_ramps_test.cpp`.

// ── LES SEUILS, ET C'EST EUX QUI DOIVENT ÊTRE IDENTIQUES ────────────────────

// 🔴 UN SEUL INDEX SUFFIT. Mesuré sur les 421 corps : beaucoup de palettes sont
// ÉPARSES et peignent des aplats d'une ou deux couleurs, que le seuil de trois
// écartait alors qu'ils couvrent de vraies pièces. Seuil 3 → 85,7 % des pixels
// réglables, seuil 1 → 91,0 %. Le vrai filtre anti-bruit est le nombre de
// PIXELS, pas la longueur.
inline constexpr int kRampMinLength = 1;
// Écart de teinte toléré à l'intérieur d'une rampe, en degrés.
inline constexpr double kRampHueTolerance = 40.0;
// Remontée de luminosité tolérée avant de couper : un dégradé RO descend.
inline constexpr double kRampValueRebound = 0.08;
// En dessous, la teinte ne veut plus rien dire — c'est un gris.
inline constexpr double kRampNeutralSaturation = 0.12;
// Une rampe qui couvre moins que cela ne vaut pas un curseur.
inline constexpr int kRampMinPixels = 200;
// Le coude de la courbe de rendement, mesuré sur les 421 corps : 4 → 71 % des
// pixels couverts, 6 → 81 %, 8 → 86 %, 10 → 88 %, 12 → 89 %. Au-delà on paie
// des curseurs pour des broutilles.
// 🔴 CETTE VALEUR DIMENSIONNE LA RECETTE RÉSEAU : la changer casse le protocole.
inline constexpr std::size_t kMaxRamps = 8;
// Somme rouge + vert + bleu en dessous de laquelle une entrée compte comme
// NOIRE. Bas exprès — 24 sur 765 —, soit ce qui est indistinguable du noir.
inline constexpr int kPaletteBlackSum = 24;

// ── CE QU'ON DÉTECTE ────────────────────────────────────────────────────────

// Une plage contiguë d'index formant un dégradé.
struct PaletteRamp {
  std::uint8_t start = 0;
  std::uint8_t length = 0;
  // La surface occupée, toutes images du sprite confondues.
  int pixels = 0;
  // La teinte dominante en degrés, ou rien quand la rampe est grise.
  std::optional<int> hue;
  // L'index qui REPRÉSENTE la rampe : sa pastille dans l'éditeur.
  //
  // 🔴 LE MILIEU DES INDEX PEINTS ET NON NOIRS, et non le milieu tout court.
  // Le milieu tout court tombe sur du noir pur pour 84 pièces sur 3 302 : la
  // pastille s'afficherait noire alors que la pièce monte jusqu'au blanc, et le
  // joueur verrait un carré qui refuse de changer.
  //
  // ⚠ PUREMENT LOCAL : il ne voyage pas, et rien de ce qui voyage n'en dépend.
  // Il échappe donc à la règle des deux côtés.
  std::uint8_t ref = 0;
};

// ── CE QU'ON APPLIQUE ───────────────────────────────────────────────────────

// Le réglage d'UNE rampe, dans l'un de deux modes.
//
// RELATIF (le défaut) : la teinte PIVOTE, saturation et luminosité
// MULTIPLIENT. Le dégradé garde intégralement sa structure — ombres, reflets,
// écarts de saturation. 🔴 Sa limite : la saturation étant un facteur, une
// rampe terne le reste. Sur une pièce beige, +100 % ne fait que doubler une
// saturation déjà faible.
//
// ABSOLU : teinte et saturation sont IMPOSÉES à toute la rampe. C'est ce que
// fait une palette de vêtement officielle — une teinte unie que seule la
// lumière du sprite module —, et toute pièce peut alors atteindre n'importe
// quelle couleur.
//
// ⚠ DANS LES DEUX MODES LA LUMINOSITÉ RESTE RELATIVE : c'est elle qui porte le
// modelé, et l'imposer aplatirait le volume.
struct RampAdjust {
  // Relatif : -359..359. Absolu : 0..359.
  std::int16_t hue = 0;
  // Relatif : -100..100. Absolu : 0..100.
  std::int8_t saturation = 0;
  // TOUJOURS relatif : -100..100.
  std::int8_t value = 0;
  bool absolute = false;

  [[nodiscard]] bool neutral() const noexcept {
    return !absolute && hue == 0 && saturation == 0 && value == 0;
  }
};

// La recette d'un corps : un réglage par rampe détectée.
//
// 🔴 ELLE NE VAUT QUE POUR LE SPRITE DONT ELLE EST TIRÉE. Les index n'ont
// aucune signification commune d'un corps à l'autre — mesuré : un Dragon
// Knight emploie 7-73, un Creator 112-223. Le corps est donc à retenir avec.
struct PaletteRecipe {
  std::array<RampAdjust, kMaxRamps> ramps{};

  // ── LES TROIS NUMÉROS QUI ACCOMPAGNENT LES RÉGLAGES ───────────────────────
  //
  // Moins un vaut « rien d'imposé » : le personnage garde ce qu'il porte.
  //
  // 🔴 ILS FONT PARTIE DE LA RECETTE, ET LA TEINTE DE BASE PLUS QUE TOUT. Elle
  // désigne le fichier de palette dont la BASE est tirée, donc le DÉCOUPAGE
  // des rampes : le rang trois ne désigne pas la même pièce du costume d'une
  // teinte à l'autre. Un joueur qui la choisirait sans qu'elle voyage serait
  // vu par les autres dans de tout autres couleurs.
  //
  // ⚠ `ApplyRecipe` NE LES LIT PAS, et ce n'est pas un oubli : ils agissent
  // AVANT lui. Ils choisissent un FICHIER et un SPRITE — voir
  // `ActorLook::body_palette`, `hair_palette` et `hair` —, là où le reste de
  // ce module calcule des couleurs. Ce qu'on donne à peindre à `ApplyRecipe`
  // est justement la base qu'ils ont choisie.
  std::int16_t body_palette = -1;
  std::int16_t hair_palette = -1;
  std::int16_t hair_style = -1;
};

// ── LA CHAÎNE ───────────────────────────────────────────────────────────────

// Combien de pixels emploient chaque index, toutes images indexées confondues.
//
// ⚠ L'INDEX ZÉRO EST LE TRANSPARENT : il est remis à zéro, il ne se peint
// jamais et ne doit pas peser dans la surface d'une rampe.
[[nodiscard]] std::array<int, formats::kPaletteEntryCount> CountIndexUsage(
    const formats::SprSprite& sprite);

// LA BASE : la palette du serveur, trous rapatriés du sprite.
//
// Aucune des deux ne suffit seule. Celle du serveur porte la couleur de
// vêtement que tout le monde voit déjà, mais elle ne définit qu'une PARTIE des
// index — mesuré sur `body_56`, 126 de ses 255 entrées sont noires, et les
// corps de quatrième classe emploient massivement celles-là, d'où des
// silhouettes noires. Celle du sprite est complète mais ignore la teinte.
//
// 🔴 LE CRITÈRE DE « TROU » N'EST PAS « L'INDEX EST NOIR ». Un noir peut être
// voulu, et c'est même le cas courant — les sprites RO ont des contours noirs.
// C'est la PLAGE qui décide : un dégradé qui finit dans le noir est une ombre
// légitime, une plage noire d'un bout à l'autre n'est que du remplissage.
// Mesuré sur les 421 corps : 20,04 % des pixels tombent sur un index noir sans
// fusion, 2,24 % avec le critère naïf des quatre octets nuls, 1,26 % avec
// celui-ci — sans jamais toucher un contour.
//
// Sans palette de serveur, celle du sprite EST le résultat.
[[nodiscard]] formats::Palette MergeServerPalette(const formats::Palette& sprite,
                                                  const formats::Palette* server);

// Découpe la palette en dégradés contigus et cohérents, les plus visibles
// d'abord, et rend au plus `kMaxRamps`.
//
// 🔴 « VISIBLE » N'EST PAS « GRAND ». Trier au seul nombre de pixels perdait
// les pièces petites mais FRANCHES — une rune rouge sur une armure, un œil qui
// brille — contre de grands aplats ternes : 265 des 421 corps gardaient au
// moins une couleur saturée hors des huit retenues, presque toujours au rang
// neuf. Monter le plafond n'y répondait pas (12 → encore 194 corps) ; c'était
// le CRITÈRE qui était faux.
//
// ⚠ LA DÉTECTION SE FAIT SUR LA BASE FUSIONNÉE, jamais sur le sprite nu : ce
// sont les couleurs que le joueur va régler qu'il faut découper.
[[nodiscard]] std::vector<PaletteRamp> DetectRamps(const formats::Palette& palette,
                                                   std::span<const int> usage);

// ── CE QUE L'ÉDITEUR MANIPULE ───────────────────────────────────────────────

// LA PIÈCE À LAQUELLE UN INDEX APPARTIENT, par son rang ; rien s'il n'est
// d'aucune.
//
// 🔴 C'EST LA PIPETTE, et elle ne compare AUCUNE couleur. Deux pièces peuvent
// porter la même teinte après réglage, et une comparaison de couleurs les
// confondrait ; l'index, lui, est exact par construction.
//
// ⚠ « D'AUCUNE » EST UNE VRAIE RÉPONSE : au-delà des huit plus grands
// dégradés, le reste du corps n'a pas de commande — c'est exactement la part
// que « Réglable : N% » annonce.
[[nodiscard]] std::optional<std::size_t> RampAt(std::span<const PaletteRamp> ramps,
                                                std::uint8_t index);

// LA COULEUR D'UNE RAMPE : celle de son index représentatif, dans la palette
// donnée. C'est la pastille que le joueur voit, et celle qu'il clique.
//
// ⚠ ELLE SE PREND SUR LA PALETTE PEINTE pour s'afficher, et sur la BASE pour
// calculer un réglage : ce que la fenêtre montre doit être ce que le
// personnage porte, mais un réglage qui partirait du résultat courant
// s'additionnerait à chaque clic, et la couleur dériverait.
[[nodiscard]] std::array<std::uint8_t, 3> RampColor(const formats::Palette& palette,
                                                    const PaletteRamp& ramp);

// LA CLARTÉ MAXIMALE QU'UNE PIÈCE PEUT ATTEINDRE, de 0 à 255.
//
// 🔴 UNE PIÈCE PEINTE SOMBRE LE RESTE, et il faut pouvoir le dire : désigner
// un rouge vif sur une zone d'ombre rend un rouge sombre, et sans explication
// la commande a l'air cassée. C'est pourtant le sprite qui parle —
// l'entrejambe, le dessous des bras, la nuque sont peints sombres par
// l'artiste, et les réglages gardent ce modelé au lieu de l'aplatir.
//
// ⚠ LE DOUBLE DE LA CLARTÉ DE L'INDEX REPRÉSENTATIF, plafonné : c'est
// exactement ce que `+100` de luminosité rend, et les deux bornes doivent
// bouger ensemble.
[[nodiscard]] int RampValueCeiling(const formats::Palette& palette, const PaletteRamp& ramp);

// LE RÉGLAGE QUI AMÈNE UNE RAMPE À LA COULEUR DEMANDÉE.
//
// 🔴 IL EST TOUJOURS ABSOLU. En relatif, la couleur demandée serait souvent
// inatteignable : la saturation étant un facteur, une pièce terne ou grise ne
// peut pas devenir vive, et le joueur qui désigne un rouge franc obtiendrait
// un rose délavé.
//
// ⚠ LA LUMINOSITÉ, ELLE, RESTE RELATIVE : c'est le rapport entre la clarté
// demandée et celle d'origine, pour que le dégradé garde son modelé. C'est
// aussi pourquoi la clarté plafonne — voir `RampValueCeiling`.
//
// ⚠ `palette` DOIT ÊTRE LA BASE, jamais la palette déjà peinte.
[[nodiscard]] RampAdjust AdjustToReach(const formats::Palette& palette, const PaletteRamp& ramp,
                                       std::array<std::uint8_t, 3> target);

// Applique une recette sur une base. Les rampes sont celles que `DetectRamps` a
// rendues, dans son ordre : c'est le RANG qui voyage.
//
// ⚠ L'OCTET D'ALPHA N'EST PAS TOUCHÉ : celui du fichier ne veut rien dire, et
// en inventer un serait une décision que personne n'a prise.
[[nodiscard]] formats::Palette ApplyRecipe(const formats::Palette& base,
                                           std::span<const PaletteRamp> ramps,
                                           const PaletteRecipe& recipe);

}  // namespace ro::render
