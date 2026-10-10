#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA COQUILLE SPHÉRIQUE, sous-type 61.
//
// UN SEUL bâtisseur pour les cinq identifiants, et tout ce qui les distingue
// tient dans un OCTET que leur site lui passe.
//
// ⚠ IL CHARGE DEUX IMAGES. Le champ qui les compte vaut deux : la première
// vient du site — `effect\guardK.tga` —, la seconde est écrite en dur —
// `effect\guardK2.tga`. N'en charger qu'une priverait la coquille extérieure de
// son image.
//
// ⚠ ET IL N'ALLUME QU'UNE PISTE SUR QUATRE. Le bâtisseur couche les trois
// autres une à une, explicitement. En allumer quatre peindrait cent
// quatre-vingts panneaux là où le client en peint quarante-cinq.

// ⚠ IL Y A DEUX BÂTISSEURS, ET ILS DIFFÈRENT PAR DEUX CONSTANTES.
// Le frère du bâtisseur de Kyrie pose la MÊME figure autrement : sa seconde
// image est `a01.bmp` et non `guardK2.tga`, sa clé de montée vaut dix et non
// 7,8, et sa marque est écrite en dur — son site ne lui passe aucun octet. Les
// tenir pour constantes de fichier donnerait au bouclier de Kyrie l'image de
// l'autre.
// Ce qui reste commun est vérifié : même sous-type 61, même échelle de onze,
// même rayon d'un, même piste unique.

// L'image que le bâtisseur de Kyrie écrit en dur, en seconde position.
inline constexpr std::string_view kSphereShellSecondTexture = "data\\texture\\effect\\guardK2.tga";

// CE QUE LE BÂTISSEUR SÈME DANS SA SEULE PISTE VIVE.
//
// ⚠ LE RAYON À L'ANCRE EST UN BROUILLON, PAS UN ÉTAT : la figure l'écrase à
// chaque rangée avant de le relire, si bien que le UN semé ici n'est jamais lu.
// Il figure quand même, parce que le retirer ferait croire un jour que le
// client ne le posait pas — et parce qu'un autre dessinateur du même sous-type
// pourrait le lire.
inline constexpr float kSphereShellLiftKey = 7.8F;
inline constexpr float kSphereShellRadiusAtAnchor = 1.0F;
inline constexpr std::uint8_t kSphereShellNodeScale = 11;

// UNE COQUILLE QU'UN EFFET POSE.
struct EffectSphereShellPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  // L'image que le SITE donne, et qui vient en premier.
  std::string_view texture;
  // LA MARQUE, tirée de l'octet du site.
  //
  // ⚠ LE BÂTISSEUR NE LAISSE PASSER QUE QUATRE VALEURS — deux, trois, quatre et
  // cinq. Tout le reste retombe sur ZÉRO, et c'est ce que fait l'octet du
  // premier identifiant. Recopier l'octet sans ce filtre donnerait des marques
  // que la figure ne sait pas peindre.
  //
  // ⚠ LE FRÈRE, LUI, L'ÉCRIT EN DUR à cinq : son site ne lui passe rien.
  std::uint8_t mark = 0;
  // L'image que CE bâtisseur-là écrit en dur, en seconde position.
  std::string_view second_texture = kSphereShellSecondTexture;
  // La clé de montée que sa piste reçoit — 7,8 chez Kyrie, dix chez le frère.
  float lift_key = kSphereShellLiftKey;
};

[[nodiscard]] std::span<const EffectSphereShellPattern> EffectSphereShellPatternTable();

[[nodiscard]] std::span<const EffectSphereShellPattern> EffectSphereShellPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud : la première vive, les trois autres éteintes.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSphereShellTracks(
    const EffectSphereShellPattern& pattern);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSphereShellNode(const EffectSphereShellPattern& pattern,
                                                        glm::vec3 anchor,
                                                        std::int32_t effect_life_ticks);

}  // namespace ro::render
