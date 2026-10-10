#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_screen_node.h"
#include "ro/render/effect_screen_quad.h"

namespace ro::render {

// ── LE TRAIT FENDU — sous-type 2, ET SON SEMIS ──────────────────────────────
//
// La figure, et le bâtisseur qui l'emploie.
//
// 🔴 UN SEUL IDENTIFIANT S'EN SERT : l'effet 51, l'impact de Frost Diver. La
// figure et son semis vivent donc dans le même fichier — les séparer en deux
// comme l'éclat d'écran donnerait deux noms pour un seul emploi. Le jour où un
// second bâtisseur s'y ajoute, la coupure se fera là où elle se fait ailleurs.
//
// ⚠ ELLE SORT EN PIXELS (ADR 0022), comme les trois autres figures de cette
// voie, et elle partage LE MOUVEMENT DU QUAD : le répartiteur des mouvements
// range les sous-types 0 et 2 dans le même cas. Un trait fendu se déplace donc, tourne
// et s'efface exactement comme un quad d'écran — seul son DESSIN diffère.

// ---------------------------------------------------------------------------
// LA FIGURE
// ---------------------------------------------------------------------------

// ⚠ LES DEUX DEMI-TAILLES SE COUPENT ENCORE EN DEUX. Le dessinateur lit les
// deux demi-tailles et les multiplie par un demi avant tout le reste : ce que le
// bâtisseur écrit « cent » donne un trait de cent pixels de long, pas de deux
// cents. Oublier ce facteur double la figure.
inline constexpr float kSplitBeamHalfShare = 0.5F;

// OÙ LA POINTE D'UNE AILE SE PLACE LE LONG DE L'ARÊTE : à la demi-longueur
// divisée par un et cinquième, soit aux cinq sixièmes. L'aile est donc décalée
// d'un sixième vers le bout opposé, et c'est ce qui rend le trait ASYMÉTRIQUE —
// une pointe de glace, pas un losange.
inline constexpr float kSplitBeamApexDivisor = 1.2F;

// ⚠ UN, ET LE NŒUD N'A PAS SON MOT À DIRE : le dessinateur passe un en dur à la
// primitive, deux fois, sans consulter le mode du nœud. C'est aussi ce que font
// la couronne et l'éclat.
inline constexpr std::int32_t kSplitBeamBlendMode = 1;

// LES TROIS ABSCISSES DE TEXTURE : l'arête porte le MILIEU de l'image, et les
// deux ailes ses deux bords. Les deux triangles montrent donc la même image en
// MIROIR l'un de l'autre, refermée sur l'arête.
inline constexpr float kSplitBeamSpineU = 0.5F;
inline constexpr float kSplitBeamLeftApexU = 0.0F;
inline constexpr float kSplitBeamRightApexU = 1.0F;
// ET LES ORDONNÉES : la pointe d'une aile au milieu, les deux bouts de l'arête
// aux deux extrémités.
inline constexpr float kSplitBeamApexV = 0.5F;
inline constexpr float kSplitBeamSpineHighV = 1.0F;
inline constexpr float kSplitBeamSpineLowV = 0.0F;

// LES DEUX TRIANGLES D'UN TRAIT FENDU, en pixels.
//
// Une ARÊTE commune, de `tourne(0, +h)` à `tourne(0, −h)`, et deux pointes de
// part et d'autre — `tourne(−w, hi − h)` et `tourne(+w, h − hi)`. Chacun sort
// comme un quad dont le quatrième coin REPREND le premier : le second triangle
// du quad est alors d'aire nulle, comme pour l'éclat.
//
// ⚠ LA ROTATION ÉCHANGE LE SINUS ET LE COSINUS, la même convention que
// `ComposeScreenQuad` : un point local (u, v) devient
// `(u·sin − v·cos, u·cos + v·sin)`. Recopier la formule habituelle tournerait la
// figure d'un quart de tour.
//
// ⚠ ET AUCUN DEMI-PIXEL NE SE RETIRE ICI, au contraire du quad : ce dessinateur
// ne fait pas la correction de centre de texel. Voir `kEffectScreenTexelOffset`.
//
// Rend un lot VIDE quand une des deux demi-tailles est nulle : un trait sans
// aire ne se distingue pas d'une absence.
[[nodiscard]] std::vector<EffectScreenQuad> ComposeScreenSplitBeamQuads(
    const EffectScreenNode& node, glm::vec2 owner_screen_position = {0.0F, 0.0F});

// ---------------------------------------------------------------------------
// LE SEMIS
// ---------------------------------------------------------------------------

// LE PAS PAR PALIERS DU LANCER : `(rand() % 45) / 10`, une division ENTIÈRE.
//
// 🔴 CINQ VALEURS, PAS UNE PLAGE CONTINUE. Le client divise un entier par un
// entier : le pas vaut son plancher, ou un de plus, jusqu'à quatre de plus. Le
// porter comme une fourchette rendrait des vitesses que le client ne produit
// jamais — et c'est le genre d'écart qu'on ne retrouve plus une fois écrit.
inline constexpr std::int32_t kSplitBeamStepSpread = 45;
inline constexpr float kSplitBeamStepDivisor = 10.0F;

// LA TAILLE D'UN ÉCLAT ET SON LANCER.
//
// ⚠ LE BÂTISSEUR EN A DEUX JEUX, ET C'EST LE RANG QUI CHOISIT : un éclat sur
// trois est deux fois plus long et deux fois et demie plus large que les autres,
// et part deux fois et demie plus vite. N'en garder qu'un donnerait une étoile
// régulière là où le client en fait une qui alterne.
struct EffectSplitBeamSize {
  // La demi-hauteur, tirée : `rand() % spread`, plus le plancher.
  std::int32_t half_height_spread = 0;
  float half_height_floor = 0.0F;
  // La demi-largeur, posée en clair.
  float half_width = 0.0F;
  // Le plancher du pas, auquel s'ajoutent les paliers ci-dessus.
  float step_floor = 0.0F;
};

// UNE ÉTOILE D'ÉCLATS qu'un bâtisseur pose d'un coup.
struct EffectSplitBeamPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;

  // COMBIEN D'ÉCLATS, et de combien de degrés leurs angles de base s'écartent.
  std::uint16_t count = 0;
  float base_step_degrees = 0.0F;
  // UN ÉCLAT SUR CE COMPTE est du jeu long — le rang zéro en est.
  std::uint16_t long_every = 3;

  std::int32_t life_ticks = 100;
  // MOINS UN veut dire « jamais ».
  std::int32_t fade_out_tick = -1;
  float alpha = 254.0F;

  // LE CAP D'UN ÉCLAT : son angle de base, ce biais, et un tirage sur cette
  // étendue. ⚠ LE MÊME NOMBRE SERT DE CAP ET D'ANGLE DE FIGURE — le bâtisseur
  // l'écrit dans le cap ET dans l'angle —, si bien qu'un éclat file dans la
  // direction où il pointe.
  float heading_bias_degrees = 0.0F;
  std::int32_t heading_spread = 0;

  // LE RAYON DE NAISSANCE, tiré sur cette étendue : `rand() % spread`, en
  // PIXELS. Il se pose sur l'angle de BASE et non sur le cap — ce sont deux
  // angles différents, et les confondre alignerait chaque éclat sur son propre
  // rayon.
  std::int32_t birth_radius_spread = 0;
  // ⚠ NÉGATIF, ET IL ÉLÈVE : l'ordonnée d'écran descend.
  float screen_offset_y = 0.0F;

  // Ce par quoi le freinage divise, après la vie : le client écrit
  // `-(700 / 484) / 1.5`.
  float step_slowdown_divisor = 1.5F;

  EffectSplitBeamSize long_shard;
  EffectSplitBeamSize short_shard;
};

[[nodiscard]] std::span<const EffectSplitBeamPattern> EffectSplitBeamPatternTable();

[[nodiscard]] std::span<const EffectSplitBeamPattern> EffectSplitBeamPatternsFor(
    std::uint32_t effect_id);

// L'ÉTOILE ENTIÈRE, tirages faits.
//
// Le générateur avance d'un éclat à l'autre, et l'ordre des tirages est celui du
// client : le cap, la demi-hauteur, le pas, puis le rayon de naissance.
[[nodiscard]] std::vector<EffectScreenNode> SowEffectSplitBeamNodes(
    const EffectSplitBeamPattern& pattern, std::uint32_t& generator);

}  // namespace ro::render
