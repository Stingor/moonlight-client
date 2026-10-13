#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/equip_attributes.h"
#include "ro/sim/skill_tree.h"
#include "ro/vfs/item_words.h"

#include "character/character_bonus_rows.h"

namespace moonclient {

// LES LIGNES DE LA CASE « ATTRIBUTS D'ÉQUIPEMENT », composées de ce que le
// serveur dit d'une pièce ou d'un ensemble qui entre ou sort de l'équipement
// (`ro::sim::EquipAttributes`).
//
// Comme le client d'origine : une ligne par attribut, verte à la pose, rouge au
// retrait ; celles d'un ensemble préfixées « (C) », celles du familier « (P) »,
// sans espace. Les LIBELLÉS sont ceux de la fiche du personnage : un bonus qui y
// a sa rangée s'écrit avec elle, mot pour mot.

// Ce qui nomme les cibles, les compétences, les statuts et les objets. Chacun
// peut manquer : le nom cède alors la place au numéro.
struct EquipAttributeNames {
  const ro::vfs::ItemWords* words = nullptr;
  const ro::sim::SkillTree* skills = nullptr;
  StatusNamer status_of;
  ItemNamer item_of;
};

// Les deux couleurs du client d'origine, en `0xRRGGBB`.
inline constexpr std::uint32_t kEquipAttributesOnColor = 0x00FFC8;
inline constexpr std::uint32_t kEquipAttributesOffColor = 0xFF0000;
// Le préfixe des lignes d'un ensemble.
inline constexpr std::string_view kEquipComboPrefix = "(C)";
// Le préfixe des lignes du familier.
inline constexpr std::string_view kEquipPetPrefix = "(P)";

// Le préfixe des lignes d'une nature, vide pour une pièce.
[[nodiscard]] constexpr std::string_view EquipAttributesPrefix(
    ro::sim::EquipAttributesKind kind) noexcept {
  switch (kind) {
    case ro::sim::EquipAttributesKind::kCombo:
      return kEquipComboPrefix;
    case ro::sim::EquipAttributesKind::kPet:
      return kEquipPetPrefix;
    case ro::sim::EquipAttributesKind::kPiece:
      break;
  }
  return {};
}

[[nodiscard]] constexpr std::uint32_t EquipAttributesColor(bool equipped) noexcept {
  return equipped ? kEquipAttributesOnColor : kEquipAttributesOffColor;
}

// Une ligne par bonus, préfixée selon la nature. Un bonus qui ne dit rien — une
// valeur nulle — ne donne pas de ligne.
[[nodiscard]] std::vector<std::string> ComposeEquipAttributeLines(
    const ro::sim::EquipAttributes& told, const EquipAttributeNames& names);

// Le texte d'UN bonus, sans préfixe, ou vide s'il ne dit rien.
[[nodiscard]] std::string EquipBonusText(const ro::sim::EquipBonus& bonus,
                                         const EquipAttributeNames& names);

}  // namespace moonclient
