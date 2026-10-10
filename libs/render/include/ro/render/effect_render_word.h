#pragma once

#include <cstdint>
#include <optional>

namespace ro::render {

// LE MOT DE RENDU D'UN NŒUD : ce que le nœud passe TEL QUEL à la file de rendu,
// avec chaque triangle de quad comme avec chaque sprite. La file en fait un
// SEAU, et vide chaque seau sous ses propres états.
//
// Trois bits décident de ce qui se voit :
//   · 0x4, le mélange PROPRE à la primitive : ces seaux-là lisent les facteurs
//     de chaque primitive — le seizième paramètre du traceur, la règle du
//     sprite (additif pour le seul mot cinq). Sans lui, le seau
//     IMPOSE les siens et le choix de la primitive ne compte pas ;
//   · 0x2, l'ADDITION imposée : ces seaux se vident sous `SRCBLEND = ONE` et
//     `DESTBLEND = ONE` ;
//   · 0x200, PAR-DESSUS TOUT : ces seaux se vident les derniers, sans épreuve
//     de profondeur — ni le décor ni un acteur ne
//     les cachent.
// Sans 0x4 ni 0x2, le seau impose le mélange ordinaire, source par son alpha.
//
// ⚠ UN NŒUD NEUF PORTE UN. Un nœud dont le bâtisseur ne touche pas le champ se
// mélange donc à l'ordinaire et passe l'épreuve de profondeur, quel que soit le
// mélange que sa figure demande.
inline constexpr std::uint32_t kEffectRenderWordOfConstructor = 1;
inline constexpr std::uint32_t kEffectRenderWordAdditive = 0x2;
inline constexpr std::uint32_t kEffectRenderWordOwnBlend = 0x4;
inline constexpr std::uint32_t kEffectRenderWordOverEverything = 0x200;
// Le seul mot pour lequel un SPRITE s'ajoute au lieu de se mélanger : sa
// destination vaut `4 · (mot ≠ 5) + 2`.
inline constexpr std::uint32_t kEffectRenderWordOfAdditiveSprite = 5;
// LE MOT QUE LE DESSINATEUR DES ACTEURS REMET POUR UN CALQUE À L'ALPHA PLEIN —
// translucide et trié tôt : mélange ordinaire, épreuve de profondeur, sous les
// `.str`. C'est celui d'un projectile d'acteur, la flèche d'un arc. Il tombe à
// un quand l'alpha du calque n'est pas plein, et à cinq quand l'acteur s'ajoute.
inline constexpr std::uint32_t kEffectRenderWordOfActorSprite = 0x101;

// LES BITS QUI RANGENT LE SEAU SANS DÉCIDER DU MÉLANGE, et que seul le
// brouillard lit ici — voir `EffectRenderWordIsFogged` :
//   · 0x1, TRANSLUCIDE : sans lui, la primitive rejoint les listes opaques du
//     sol et du décor ;
//   · 0x8, TÔT : avec 0x4, un seau vidé juste après le sol, brouillard coupé ;
//     avec 0x200, un seau vidé sans épreuve de profondeur, SOUS le brouillard ;
//   · 0x10, DÉCOUPÉ : un seau trié, vidé sous un test d'alpha ;
//   · 0x100, TRIÉ TÔT : un seau vidé avant la coupure du brouillard, addition
//     comprise.
inline constexpr std::uint32_t kEffectRenderWordTranslucent = 0x1;
inline constexpr std::uint32_t kEffectRenderWordEarly = 0x8;
inline constexpr std::uint32_t kEffectRenderWordCutout = 0x10;
inline constexpr std::uint32_t kEffectRenderWordEarlySorted = 0x100;

// Les facteurs de Direct3D qu'un seau impose.
inline constexpr std::int32_t kEffectRenderBlendOne = 2;
inline constexpr std::int32_t kEffectRenderBlendSourceAlpha = 5;
inline constexpr std::int32_t kEffectRenderBlendInverseSourceAlpha = 6;

struct EffectRenderBlend {
  std::int32_t source = kEffectRenderBlendSourceAlpha;
  std::int32_t destination = kEffectRenderBlendInverseSourceAlpha;
};

// CE QUE LE SEAU FAIT DE SES PRIMITIVES.
struct EffectRenderBucket {
  // Faux pour les seaux vidés sans épreuve de profondeur, après tous les autres.
  bool depth_tested = true;
  // Le mélange que le seau impose, ou rien quand il garde celui de chaque
  // primitive.
  std::optional<EffectRenderBlend> imposed_blend;
};

[[nodiscard]] constexpr EffectRenderBucket EffectRenderBucketOf(std::uint32_t render_word) {
  EffectRenderBucket seau;
  seau.depth_tested = (render_word & kEffectRenderWordOverEverything) == 0;
  if ((render_word & kEffectRenderWordOwnBlend) != 0) {
    return seau;
  }
  seau.imposed_blend =
      (render_word & kEffectRenderWordAdditive) != 0
          ? EffectRenderBlend{kEffectRenderBlendOne, kEffectRenderBlendOne}
          : EffectRenderBlend{kEffectRenderBlendSourceAlpha, kEffectRenderBlendInverseSourceAlpha};
  return seau;
}

// Vrai quand un SPRITE de ce mot se peint en addition. Le mélange propre d'un
// sprite est additif pour le seul mot cinq ; un seau qui impose l'addition
// l'emporte.
[[nodiscard]] constexpr bool EffectSpriteIsAdditive(std::uint32_t render_word) {
  const EffectRenderBucket seau = EffectRenderBucketOf(render_word);
  if (!seau.imposed_blend.has_value()) {
    return render_word == kEffectRenderWordOfAdditiveSprite;
  }
  return seau.imposed_blend->destination == kEffectRenderBlendOne;
}

// Le mélange d'un quad du monde : le sien, ou celui que son seau impose.
[[nodiscard]] constexpr EffectRenderBlend EffectWorldQuadBlend(
    std::int32_t quad_source, std::int32_t quad_destination,
    std::optional<std::uint32_t> render_word) {
  if (render_word.has_value()) {
    if (const EffectRenderBucket seau = EffectRenderBucketOf(*render_word);
        seau.imposed_blend.has_value()) {
      return *seau.imposed_blend;
    }
  }
  return EffectRenderBlend{quad_source, quad_destination};
}

// Vrai quand le lot passe l'épreuve de profondeur. Un lot sans mot la passe.
[[nodiscard]] constexpr bool EffectLotIsDepthTested(std::optional<std::uint32_t> render_word) {
  return !render_word.has_value() || EffectRenderBucketOf(*render_word).depth_tested;
}

// LE SEAU SE VIDE-T-IL SOUS LE BROUILLARD ?
//
// La file de rendu coupe le brouillard avant ses derniers seaux, et le coupe
// aussi le temps d'un seau à part. Lui échappent : tout mélange PROPRE (0x4),
// l'ADDITION sauf quand 0x100 la range parmi les premiers seaux triés, et
// PAR-DESSUS TOUT sauf quand 0x8 l'y garde. Le reste s'embrume — et tout ce
// qui n'est pas translucide, avec le sol et le décor.
[[nodiscard]] constexpr bool EffectRenderWordIsFogged(std::uint32_t render_word) {
  if ((render_word & kEffectRenderWordTranslucent) == 0) {
    return true;
  }
  if ((render_word & kEffectRenderWordOwnBlend) != 0) {
    return false;
  }
  if ((render_word & kEffectRenderWordAdditive) != 0) {
    return (render_word & kEffectRenderWordEarlySorted) != 0;
  }
  if ((render_word & (kEffectRenderWordCutout | kEffectRenderWordEarlySorted)) != 0) {
    return true;
  }
  if ((render_word & kEffectRenderWordOverEverything) != 0) {
    return (render_word & kEffectRenderWordEarly) != 0;
  }
  return true;
}

// Vrai quand un lot s'embrume. UN LOT SANS MOT NE S'EMBRUME PAS : il garde le
// mélange de chaque primitive, ce que fait le seau à mélange propre.
[[nodiscard]] constexpr bool EffectLotIsFogged(std::optional<std::uint32_t> render_word) {
  return render_word.has_value() && EffectRenderWordIsFogged(*render_word);
}

// LE SEAU DES FICHIERS `.str` : leur image porte le mot cinq, translucide au
// mélange propre, et la file la range dans un seau qui se vide TARD, dans
// l'ordre d'arrivée — après tous les seaux triés, addition comprise, et
// seulement avant ceux qui passent par-dessus tout.
//
// Vrai quand un seau de ce mot se vide AVANT celui des fichiers, donc SOUS leur
// image. C'est ce qui met l'impact d'un `.str` par-dessus la flèche de nœuds qui
// l'a apporté : Fire Bolt pose son éclat de feu sur son dard et son anneau, qui
// portent le mot du constructeur.
//
// Le rangement suit la file, bit par bit et dans son ordre de lecture :
//   · sans 0x1, la primitive rejoint les listes OPAQUES, vidées les premières ;
//   · 0x4 : avec 0x8, le seau tôt, vidé juste après l'eau ; avec 0x200, celui
//     qui passe par-dessus tout, APRÈS ; sinon le seau des fichiers lui-même ;
//   · 0x2 : l'addition, triée avant les fichiers, sauf avec 0x200 et sans 0x100 ;
//   · 0x10 et 0x100 : deux seaux triés, avant ;
//   · 0x200 : sans épreuve de profondeur, APRÈS, sauf avec 0x8 qui le vide sous
//     le brouillard, avant ;
//   · le reste, les deux seaux triés des corps et des nœuds ordinaires, avant.
[[nodiscard]] constexpr bool EffectRenderWordPaintsBeforeStr(std::uint32_t render_word) {
  if ((render_word & kEffectRenderWordTranslucent) == 0) {
    return true;
  }
  if ((render_word & kEffectRenderWordOwnBlend) != 0) {
    return (render_word & kEffectRenderWordEarly) != 0;
  }
  if ((render_word & kEffectRenderWordAdditive) != 0) {
    return (render_word & kEffectRenderWordEarlySorted) != 0 ||
           (render_word & kEffectRenderWordOverEverything) == 0;
  }
  if ((render_word & (kEffectRenderWordCutout | kEffectRenderWordEarlySorted)) != 0) {
    return true;
  }
  if ((render_word & kEffectRenderWordOverEverything) != 0) {
    return (render_word & kEffectRenderWordEarly) != 0;
  }
  return true;
}

// Vrai quand un seau de ce mot se vide AVANT LES CORPS. Ils sont deux, que la
// file vide juste après l'eau, dans cet ordre :
//   · le seau tôt, translucide au mélange propre (1|4|8) : celui du mot treize,
//     la file du sol — une ombre posée sous un personnage y passe ;
//   · le seau sans épreuve de profondeur gardé sous le brouillard (1|0x200|8,
//     sans 2, 4, 0x10 ni 0x100) : celui du mot 521, le quadrillage de Digital
//     Space, que le personnage recouvre.
// Le corps peint ensuite recouvre l'un et l'autre.
[[nodiscard]] constexpr bool EffectRenderWordPaintsBeforeActors(std::uint32_t render_word) {
  if ((render_word & kEffectRenderWordTranslucent) == 0) {
    return false;
  }
  if ((render_word & kEffectRenderWordOwnBlend) != 0) {
    return (render_word & kEffectRenderWordEarly) != 0;
  }
  if ((render_word &
       (kEffectRenderWordAdditive | kEffectRenderWordCutout | kEffectRenderWordEarlySorted)) != 0) {
    return false;
  }
  return (render_word & kEffectRenderWordOverEverything) != 0 &&
         (render_word & kEffectRenderWordEarly) != 0;
}

// LE RANG DU SEAU DANS L'ORDRE OÙ LA FILE LES VIDE : un seau de rang plus
// petit se peint entièrement avant un seau de rang plus grand, quelle que soit
// la profondeur de leurs primitives. Dans un même seau trié, c'est la
// profondeur de CHAQUE primitive qui ordonne, quel que soit l'effet qui l'a
// posée : les pointes de deux effets voisins s'entremêlent.
//
// Un rang inférieur à celui des fichiers est exactement
// `EffectRenderWordPaintsBeforeStr`.
enum class EffectBucketRank : std::uint8_t {
  // Sans 0x1 : les listes opaques.
  kOpaque,
  // 1|4|8 : le seau tôt.
  kEarlyOwnBlend,
  // 1|0x200|8 : sans épreuve, sous le brouillard.
  kEarlyOverEverything,
  // 1|0x100 : trié.
  kEarlySorted,
  // 1|2|0x100 : l'addition triée tôt.
  kEarlySortedAdditive,
  // Le mot du constructeur et le reste : le seau trié des nœuds ordinaires.
  kSorted,
  // Avec 0x20 : son voisin trié, vidé juste après.
  kSortedNeighbour,
  // 1|0x10 : le découpé.
  kCutout,
  // 1|2 : l'addition.
  kAdditive,
  // 1|4 : les fichiers `.str`, dans l'ordre d'arrivée.
  kStr,
  // Sans épreuve de profondeur, les derniers : 1|2|0x200, 1|0x200, 1|4|0x200.
  kAdditiveOverEverything,
  kOverEverything,
  kOwnBlendOverEverything,
};

inline constexpr std::uint32_t kEffectRenderWordSortedNeighbour = 0x20;

[[nodiscard]] constexpr EffectBucketRank EffectRenderWordBucketRank(std::uint32_t render_word) {
  if ((render_word & kEffectRenderWordTranslucent) == 0) {
    return EffectBucketRank::kOpaque;
  }
  const bool early = (render_word & kEffectRenderWordEarly) != 0;
  const bool over_everything = (render_word & kEffectRenderWordOverEverything) != 0;
  const bool early_sorted = (render_word & kEffectRenderWordEarlySorted) != 0;
  if ((render_word & kEffectRenderWordOwnBlend) != 0) {
    if (early) {
      return EffectBucketRank::kEarlyOwnBlend;
    }
    return over_everything ? EffectBucketRank::kOwnBlendOverEverything : EffectBucketRank::kStr;
  }
  if ((render_word & kEffectRenderWordAdditive) != 0) {
    if (early_sorted) {
      return EffectBucketRank::kEarlySortedAdditive;
    }
    return over_everything ? EffectBucketRank::kAdditiveOverEverything
                           : EffectBucketRank::kAdditive;
  }
  if ((render_word & kEffectRenderWordCutout) != 0) {
    return EffectBucketRank::kCutout;
  }
  if (early_sorted) {
    return EffectBucketRank::kEarlySorted;
  }
  if (over_everything) {
    return early ? EffectBucketRank::kEarlyOverEverything : EffectBucketRank::kOverEverything;
  }
  return (render_word & kEffectRenderWordSortedNeighbour) != 0 ? EffectBucketRank::kSortedNeighbour
                                                               : EffectBucketRank::kSorted;
}

// Le rang d'un lot. UN LOT SANS MOT garde le mélange de chaque primitive, comme
// le seau des fichiers : il en prend le rang.
[[nodiscard]] constexpr EffectBucketRank EffectLotBucketRank(
    std::optional<std::uint32_t> render_word) {
  return render_word.has_value() ? EffectRenderWordBucketRank(*render_word)
                                 : EffectBucketRank::kStr;
}

// Vrai quand un lot se peint sous les fichiers. ⚠ UN LOT SANS MOT GARDE SA
// PLACE, après eux : sa famille ne porte pas encore le mot du client, et rien ne
// dit dans quel seau il tomberait.
[[nodiscard]] constexpr bool EffectLotPaintsBeforeStr(std::optional<std::uint32_t> render_word) {
  return render_word.has_value() && EffectRenderWordPaintsBeforeStr(*render_word);
}

}  // namespace ro::render
