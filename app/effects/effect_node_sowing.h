#pragma once

#include <cstdint>

namespace moonclient {

// LE PARAMÈTRE D'UN EFFET QUI N'EN A PAS : UN. C'est ce que le client pose comme
// paramètre d'instance sur son chemin nu — le paquet 0x01F3 de `@effect` —, et
// ce qu'un bâtisseur à `switch` lit alors : une boule pour Soul Strike, là où
// zéro n'en donnerait aucune.
inline constexpr std::int32_t kRawEffectParam = 1;

// L'INSTANCE BORNE LE PARAMÈTRE QU'ELLE REÇOIT À CINQUANTE, et c'est la valeur
// bornée que lisent tous ses bâtisseurs — la teinte d'un flacon comme le
// nombre de semis. Rien ne relève le plancher : un paramètre négatif reste tel.
inline constexpr std::int32_t kInstanceParamCap = 50;
[[nodiscard]] inline std::int32_t InstanceParamOf(std::int32_t param) {
  return param > kInstanceParamCap ? kInstanceParamCap : param;
}

// DE COMBIEN UN NŒUD NAÎT AU-DESSUS DU SOL. 🔴 UNE FIGURE POSÉE À PLAT PARTAGE
// LE PLAN DU TERRAIN, et la profondeur ne peut plus les départager : l'anneau
// se retrouve DANS le sol, qui le mange par bandes selon ses triangles.
// ⚠ CE N'EST PAS UNE MESURE DU CLIENT mais un remède de RENDU : six dixièmes —
// un huitième de cellule — sortent la figure du plan sans qu'on la voie flotter.
inline constexpr float kSownHeight = 0.6F;
// ⚠ ET SEULEMENT POUR UN NŒUD QUE LE CLIENT POSE DANS LE PLAN DU SOL : un
// décalage vertical non nul l'en sort déjà, et la levée s'y ajouterait.
[[nodiscard]] inline float SownLiftFor(float vertical_offset) {
  return vertical_offset == 0.0F ? kSownHeight : 0.0F;
}

}  // namespace moonclient
