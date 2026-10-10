#pragma once

#include <cstdint>

namespace ro::sim {

// OÙ UNE PIÈCE SE PORTE, en drapeaux — `enum equip_pos`, `src/common/mmo.hpp`.
//
// Ce sont les valeurs du SERVEUR, sous ses noms. C'est ce qui permet de
// rapprocher une ligne d'ici d'un script ou d'une base du serveur sans
// traduire, et le rapprochement est la seule vérification possible : aucune de
// ces valeurs ne se déduit, chacune se relève.
//
// ⚠ CE N'EST PAS UN INDEX MAIS UN MASQUE. Une arme à deux mains occupe les deux
// mains, un accessoire peut aller d'un côté ou de l'autre.
inline constexpr std::uint32_t kEquipHeadLow = 0x000001;
inline constexpr std::uint32_t kEquipWeapon = 0x000002;   // EQP_HAND_R
inline constexpr std::uint32_t kEquipGarment = 0x000004;  // la cape
// 🔴 LES DEUX ACCESSOIRES SONT NOMMÉS À L'ENVERS DE CE QUE L'ÉCRAN AFFICHE, et
// ces noms-ci sont ceux du serveur. Le client montre `"Acc. L"` sur ce bit-ci et
// `"Acc. R"` sur l'autre — l'inverse. Les deux sont « justes » dans leur monde,
// ce qui est précisément le piège : renommer l'un pour l'accorder à l'autre
// échangerait deux emplacements sans que rien ne le dise. On garde le nom du
// serveur ici, et l'étiquette de l'écran là où l'on dessine.
inline constexpr std::uint32_t kEquipAccessoryRight = 0x000008;  // EQP_ACC_R
inline constexpr std::uint32_t kEquipArmor = 0x000010;
inline constexpr std::uint32_t kEquipShield = 0x000020;  // EQP_HAND_L
inline constexpr std::uint32_t kEquipShoes = 0x000040;
inline constexpr std::uint32_t kEquipAccessoryLeft = 0x000080;  // EQP_ACC_L
inline constexpr std::uint32_t kEquipHeadTop = 0x000100;
inline constexpr std::uint32_t kEquipHeadMid = 0x000200;

// LES COSTUMES PORTENT LEURS PROPRES BITS. Ils se montrent aux mêmes quatre
// places que les coiffes et la cape, mais chercher un chapeau d'apparat par
// `kEquipHeadTop` ne rend rien : ce sont deux emplacements distincts, et une
// même tête peut porter les deux.
inline constexpr std::uint32_t kEquipCostumeHeadTop = 0x000400;
inline constexpr std::uint32_t kEquipCostumeHeadMid = 0x000800;
inline constexpr std::uint32_t kEquipCostumeHeadLow = 0x001000;
inline constexpr std::uint32_t kEquipCostumeGarment = 0x002000;
inline constexpr std::uint32_t kEquipCostumeMask = 0x003C00;

inline constexpr std::uint32_t kEquipAmmo = 0x008000;

// L'ÉQUIPEMENT D'OMBRE, et il ne se montre nulle part chez nous.
//
// 🔴 IL DOIT SE RECONNAÎTRE POUR ÊTRE ÉCARTÉ. Le client d'origine range ces
// pièces sur les MÊMES places que les costumes, où elles en couvrent un sans
// rien annoncer. Les ignorer par distraction donnerait le même écran ; les
// ignorer nommément est ce qui empêche qu'on les y remette un jour.
//
// ⚠ `0x004000` EST COMPTÉ ICI et le serveur le tient pour un emplacement de
// costume au sol, en commentaire, sans l'employer. Aucune pièce n'en porte ; il
// tombe du côté des ombres, c'est-à-dire du côté où on ne le montre pas.
inline constexpr std::uint32_t kEquipShadowMask = 0x3F4000;

// LES DEUX MAINS ENSEMBLE — `EQP_ARMS`, `src/map/pc.hpp`.
//
// 🔴 UNE ARME À DEUX MAINS PORTE CE MASQUE ENTIER, donc elle répond aussi à la
// case du bouclier. Sans ce nom, le mannequin dessinerait la même arme deux
// fois, et un vrai port à deux armes serait indiscernable d'une claymore.
inline constexpr std::uint32_t kEquipBothHands = kEquipWeapon | kEquipShield;

[[nodiscard]] constexpr bool IsShadowLocation(std::uint32_t location) noexcept {
  return (location & kEquipShadowMask) != 0;
}

[[nodiscard]] constexpr bool IsCostumeLocation(std::uint32_t location) noexcept {
  return (location & kEquipCostumeMask) != 0;
}

// La pièce occupe-t-elle les deux mains à elle seule ?
[[nodiscard]] constexpr bool IsTwoHanded(std::uint32_t worn_at) noexcept {
  return (worn_at & kEquipBothHands) == kEquipBothHands;
}

}  // namespace ro::sim
