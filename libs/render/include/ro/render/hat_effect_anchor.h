#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/hat_effect_table.h"

namespace ro::render {

// OÙ LE `.str` D'UN CHAPEAU SE PEINT, par rapport aux pieds de son porteur.
//
// 🔴 CE N'EST PAS LE RELÈVEMENT ORDINAIRE D'UN EFFET. Un `.str` ordinaire pose
// l'origine de son fichier à quatre-vingts pixels au-dessus du point joué (voir
// `kEffectAnchorLiftPixels`). Celui d'un chapeau n'en a AUCUN : il déplace le
// point lui-même, dans le PLAN DE LA VUE, d'une hauteur en unités du monde que
// la fiche corrige :
//
//     hauteur = base + hatEffectPos − 7
//     base    = 18 + classe de taille du métier affiché
//               (18 tout court si la fiche ignore la monture ou suit la tête ;
//                23 sur une monture du shop, 18 pour un Doram)
//     droite  = hatEffectPosX (+ 5 ou 8 pour trois fichiers nommés)
//
// les deux multipliées par l'échelle du corps du métier. Une fiche à −11 pose
// donc l'origine de son fichier AUX PIEDS d'un personnage à pied ; une fiche à 0
// la pose onze unités plus haut, à peu près où le relèvement ordinaire l'aurait
// mise.
//
// ⚠ LE CLIENT RENONCE quand la hauteur ET le décalage à droite sont tous deux
// négatifs : aucun déplacement, et c'est le relèvement ordinaire qui revient.
//
// 🔴 UNE FICHE QUI SUIT LA TÊTE la suit pour de bon : à l'ancre s'ajoute le
// déplacement de la tête depuis sa place de repos — assis, en frappant, en
// selle —, fois la même échelle. Le renoncement ne l'efface pas.
//
// Voir `docs/classes-du-gestionnaire-d-effets.md`, § « L'ancre d'un chapeau ».

// La hauteur que le client retranche à la base, pour tout identifiant qui n'a
// pas de correction propre — c'est le cas de tous ceux d'un chapeau.
inline constexpr std::int32_t kHatAnchorHeightCorrection = -7;
// La base : dix-huit unités, plus la classe de taille du métier.
inline constexpr std::int32_t kHatAnchorBaseHeight = 18;
// LA BASE SUR UNE MONTURE DU SHOP, quel que soit le métier : la classe de
// taille ne se lit plus. Un Doram garde la base à pied.
inline constexpr std::int32_t kHatAnchorCashMountBaseHeight = 23;
// Ce qu'une fiche attachée à la tête retranche encore pour un Doram.
inline constexpr std::int32_t kHatAnchorDoramHeadCorrection = -3;

// Les fichiers que le client décale à droite d'office, par la PRÉSENCE de ce
// nom dans leur chemin, dans cet ordre : le premier trouvé l'emporte.
struct HatAnchorNamedShift {
  std::string_view name;
  float right_units = 0.0F;
};
inline constexpr std::array<HatAnchorNamedShift, 3> kHatAnchorNamedShifts = {{
    {"strangelights", 5.0F},
    {"superstar", 5.0F},
    {"ljosalfar", 8.0F},
}};

// LES IDENTIFIANTS CONCRETS QU'UN CHAPEAU FAIT JOUER PAR LE LECTEUR DE `.str`,
// et qui prennent donc cette ancre sans passer par leur rang. Des 101
// identifiants que nomme `hateffectinfo`, c'est le seul : les autres sont des
// classes propres ou des effets du répartiteur, qui se placent eux-mêmes.
inline constexpr std::array<std::uint32_t, 1> kHatIdsReadAsStr = {1012};

// Qui porte le chapeau, tel que l'ancre le lit.
struct HatEffectBearer {
  // Le métier AFFICHÉ.
  std::uint32_t job = 0;
  // La réduction en cours du corps, de un à un demi : voir
  // `BodyShrinkAfterEffects`. Elle ne touche que les fiches qui le demandent.
  float shrink_scale = 1.0F;
  // MONTÉ SUR UNE MONTURE DU SHOP — l'état « toutes montures ». Elle remplace
  // la classe de taille du métier dans la base.
  bool cash_mount = false;
  // DE COMBIEN SA TÊTE S'EST DÉPLACÉE depuis sa place de repos, en pixels de
  // sprite, l'ordonnée vers le BAS : voir `HeadPlace` et
  // `ActorOutfit::HeadRestingPlace`. Une fiche qui suit la tête le suit ; nul
  // pour qui n'a pas de tête.
  glm::vec2 head_shift_pixels{0.0F};
};

// CE QUE L'ANCRE LIT DE SA FICHE ET DE SON FICHIER, figé quand le `.str`
// naît. Le porteur, lui, se relit À CHAQUE IMAGE — voir `HatAnchorFor` :
// monter, descendre ou rapetisser déplace le fichier qu'il porte déjà.
struct HatAnchorRecipe {
  // `hatEffectPos`, lu en entier.
  std::int32_t record_height = 0;
  // `hatEffectPosX`, plus le décalage que le chemin du fichier donne d'office.
  float right_units = 0.0F;
  // La fiche ignore la monture, ou suit la tête : la base reste à dix-huit.
  bool fixed_base = false;
  bool attached_head = false;
  bool adjust_position_when_shrunk = false;
  bool adjust_size_when_shrunk = false;
};

// Ce que le semis d'un chapeau fait de son `.str`.
struct HatEffectAnchor {
  // Le déplacement dans le plan de la vue, en unités du monde : l'abscisse vers
  // la droite de l'écran, l'ordonnée vers le BAS.
  glm::vec2 view_offset{0.0F};
  // Le relèvement en pixels d'effet : zéro quand l'ancre tient.
  float lift_pixels = 0.0F;
  // L'échelle de l'image, réduite avec le corps pour les fiches qui le disent.
  float scale = 1.0F;
};

// LA CLASSE DE TAILLE D'UN MÉTIER : trois pour les corps montés des premières
// classes, cinq pour les montures des classes suivantes, zéro sinon.
[[nodiscard]] std::int32_t HatAnchorJobSizeClass(std::uint32_t job);

// L'ancre d'une fiche jouée sous son RANG — toute fiche qui nomme un fichier.
// `file` est le chemin que le client joue, qui peut être celui d'une autre fiche
// (voir `HatEffectTable::FindByConcreteId`) ; `record` est alors cette autre
// fiche, dont le client lit aussi les décalages.
[[nodiscard]] HatEffectAnchor HatStrAnchor(const HatEffect& record, std::string_view file,
                                           const HatEffectBearer& bearer);

// L'ancre d'un identifiant CONCRET joué par le lecteur de `.str` : ni décalage de
// fiche, ni drapeau — le client ne les lit que sous un rang.
[[nodiscard]] HatEffectAnchor HatConcreteStrAnchor(std::string_view file,
                                                   const HatEffectBearer& bearer);

// La recette d'une fiche jouée sous son RANG, et celle d'un identifiant concret.
[[nodiscard]] HatAnchorRecipe HatStrAnchorRecipe(const HatEffect& record, std::string_view file);
[[nodiscard]] HatAnchorRecipe HatConcreteStrAnchorRecipe(std::string_view file);

// L'ancre qu'une recette donne sur ce porteur, à cet instant.
[[nodiscard]] HatEffectAnchor HatAnchorFor(const HatAnchorRecipe& recipe,
                                           const HatEffectBearer& bearer);

// Vrai quand cet identifiant concret prend l'ancre du chapeau.
[[nodiscard]] bool HatIdIsReadAsStr(std::uint32_t effect_id);

}  // namespace ro::render
