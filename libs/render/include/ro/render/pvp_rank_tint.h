#pragma once

#include <array>
#include <cstdint>

namespace ro::render {

// ── L'ANNEAU DES DIX PREMIERS, ET LA COULEUR QUE SON RANG LUI DONNE ─────────
//
// `EF_TOPRANK`, l'effet 159 : l'anneau posé sous quiconque entre dans les dix
// premiers du classement PVP d'une carte.
//
// 🔴 SA COULEUR PORTE LE RANG, et c'est ce qui le rend lisible d'un coup d'œil
// dans une mêlée : le premier est presque blanc, le dixième rouge vif. Sans
// elle, dix joueurs portent le même anneau et il ne dit plus rien.
inline constexpr std::uint32_t kPvpTopRankEffect = 159;

// VRAI QUAND LA VALEUR DE L'EFFET EST LE PARAMÈTRE DE SON INSTANCE.
//
// 🔴 UN SEUL CHAMP POUR DEUX MÉTIERS, et c'est le client qui le veut ainsi : le
// même paramètre d'instance porte le NOMBRE DE COUPS d'un coup multi-coups et le
// RANG d'EF_TOPRANK, chacun posé par une commande différente. C'est pourquoi la
// valeur d'un effet ne se verse PAS dans le paramètre sans regarder lequel :
// une valeur de soin — le paquet 0x0284 en porte de plusieurs centaines —
// tombant dans le paramètre ferait semer des centaines de nœuds aux patrons qui
// le suivent.
//
// ⚠ ELLE REND LA QUESTION ET NON LA VALEUR, parce que le défaut du paramètre
// appartient à `app` : le redire ici en ferait deux, qui divergeraient.
//
// Les sphères élémentaires — feu, vent, eau, terre, de 735 à 738 — y lisent
// leur NOMBRE : un nœud par sphère, cinq au plus, et les halos au-delà.
inline constexpr std::uint32_t kFirstSpiritCharmEffect = 735;
inline constexpr std::uint32_t kLastSpiritCharmEffect = 738;

[[nodiscard]] constexpr bool EffectReadsValueAsParam(std::uint32_t effect_id) noexcept {
  return effect_id == kPvpTopRankEffect ||
         (effect_id >= kFirstSpiritCharmEffect && effect_id <= kLastSpiritCharmEffect);
}

// LE ROUGE, CONSTANT, ET LE PAS DES DEUX AUTRES CANAUX.
//
// `R = 250` tel quel, puis `V = B = (10 − rang) × 25`.
inline constexpr float kPvpTopRankRed = 250.0F;
inline constexpr std::int32_t kPvpTopRankPaleRank = 10;
inline constexpr float kPvpTopRankStep = 25.0F;

// LE RANG ONZE, QUI SORT DE LA RAMPE : un anneau VERT au lieu de la dégradation.
//
// ⚠ IL EST DANS LE CODE, PAS DANS LE JEU. Le récepteur de `ZC_NOTIFY_RANKING`
// ne pose l'effet que si le rang est au plus dix : cette branche ne s'atteint
// par aucun paquet. On la porte parce qu'elle est LUE, et on dit qu'elle dort —
// la taire ferait croire à un oubli le jour où quelqu'un relira le cas.
inline constexpr std::int32_t kPvpTopRankGreenRank = 11;

// ⚠ L'ALPHA NE VIENT PAS DU CAS, et n'a donc rien à faire ici : le constructeur
// du nœud l'écrit à 254, le cas de l'anneau n'y touche jamais, et
// le patron le porte déjà.

// La teinte, canal par canal, dans l'ordre rouge-vert-bleu.
//
// ⚠ RVB ET NON COLORREF, et cela se tranche sur le FORMAT DE SOMMET, pas sur un
// immédiat : la couleur part dans le champ DIFFUSE d'un sommet déclaré
// `XYZRHW|DIFFUSE|SPECULAR|TEX1`, donc un `D3DCOLOR 0xAARRGGBB`. La preuve
// indépendante est une valeur ASYMÉTRIQUE : le cas voisin, `EF_PARTY`, écrit
// (50, 50, 253) — l'anneau de groupe est BLEU, et lu à l'envers il serait
// rouge.
//
// ⚠ UN RANG ABSENT OU CACHÉ DONNE UN ANNEAU PRESQUE BLANC, et non une couleur
// aberrante. Le serveur envoie moins un pour un joueur caché : (10 − (−1)) × 25
// vaut 275, que le client ÉCRÊTE à 255 au premier tic de sa rampe. Un rang zéro
// — celui d'un effet allumé autrement que par le paquet, une commande d'essai
// par exemple — donne 250. Les deux sont indiscernables du rang un, et c'est
// ainsi chez le client : la teinte ne se mesure qu'avec le vrai paquet 0x019a.
[[nodiscard]] std::array<float, 3> PvpTopRankTint(std::int32_t rank) noexcept;

}  // namespace ro::render
