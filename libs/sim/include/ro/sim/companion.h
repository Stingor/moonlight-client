#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/sim/skill_book.h"

namespace ro::sim {

// LES TROIS COMPAGNONS : le chariot, la monture, le faucon.
enum class CompanionKind : std::uint8_t { kCart, kMount, kFalcon };

inline constexpr std::size_t kCompanionKindCount = 3;

// CE QUE LE PERSONNAGE A DÉJÀ, ET PAR QUELLE COMPÉTENCE IL L'APPELLE.
//
// 🔴 LE SERVEUR LE POUSSE, ET C'EST LE SEUL MOYEN. Le client d'origine devine
// ces trois états depuis son bitmask d'option — et il se trompe : sous
// `NEW_CARTS`, le chariot n'y est PAS. Le serveur envoie donc les états et les
// identifiants des compétences, et aucun identifiant n'est écrit en dur.
//
// 🔴 MAIS CE PAQUET NE PART QU'À LA CONNEXION ET QUAND UN COMPAGNON BASCULE, et
// c'est pourquoi il ne dit pas si la compétence est apprise. Changer de métier,
// apprendre ou oublier une compétence ne le renvoie pas : les niveaux qu'il
// porte sont ceux de son dernier envoi. Le grimoire, lui, est renvoyé à chacun
// de ces moments — c'est lui qu'on lit, voir `CompanionLevelOf`.
//
// ⚠ ET C'EST AUSSI LE SEUL MOYEN DE LES APPELER. Le protocole d'origine n'a
// AUCUN paquet pour monter sur son peco ou sortir son faucon : cela passe par
// un PNJ ou une commande de maître du jeu. Un chevalier ne peut pas invoquer sa
// propre monture depuis son client. `CZ 0x0F15` répare cela, et le serveur
// revalide tout — `pc_setriding` exige `KN_RIDING`, comme toujours.
struct CompanionState {
  // `MC_CHANGECART` : sans elle, pas de décoration à choisir. ⚠ FIGÉ AU DERNIER
  // ENVOI, et le fil ne dit pas son identifiant : rien ne permet de le relire
  // dans le grimoire.
  std::uint8_t change_cart_level = 0;

  // LE TYPE de chariot en cours. Zéro = aucun ; un à N = une décoration.
  //
  // ⚠ CE N'EST PAS UN BOOLÉEN, et c'est ce qui le distingue des deux autres :
  // le chariot a plusieurs apparences, et celle qu'il porte est CET état-là.
  std::uint8_t cart_type = 0;
  bool mounted = false;
  bool falcon_out = false;
  // La décoration la plus haute que le niveau de base autorise. Le serveur la
  // calcule — les paliers sont les siens, et ils diffèrent sous `NEW_CARTS`.
  // ⚠ FIGÉE AU DERNIER ENVOI : un niveau de base gagné ne la renvoie pas.
  std::uint8_t cart_decoration_most = 0;

  // LES IDENTIFIANTS AEGIS DES TROIS COMPÉTENCES, tels que le serveur les
  // nomme.
  //
  // 🔴 ILS VIENNENT DU FIL POUR QU'AUCUN NE SOIT ÉCRIT EN DUR. C'est par eux
  // qu'on retrouve l'ICÔNE dans les tables de l'installation ; les graver ici
  // ferait mentir la case le jour où le serveur renumérote. Zéro tant qu'il
  // n'a rien dit.
  std::uint16_t push_cart_skill = 0;
  std::uint16_t mount_skill = 0;
  std::uint16_t falcon_skill = 0;

  [[nodiscard]] friend bool operator==(const CompanionState&, const CompanionState&) = default;
};

// Ce compagnon est-il là ?
[[nodiscard]] constexpr bool CompanionActive(const CompanionState& told,
                                             CompanionKind kind) noexcept {
  switch (kind) {
    case CompanionKind::kCart:
      return told.cart_type != 0;
    case CompanionKind::kMount:
      return told.mounted;
    case CompanionKind::kFalcon:
      return told.falcon_out;
  }
  return false;
}

// L'identifiant de la compétence qui l'appelle. Zéro tant que le serveur n'a
// rien dit.
[[nodiscard]] constexpr std::uint16_t CompanionSkillOf(const CompanionState& told,
                                                       CompanionKind kind) noexcept {
  switch (kind) {
    case CompanionKind::kCart:
      return told.push_cart_skill;
    case CompanionKind::kMount:
      return told.mount_skill;
    case CompanionKind::kFalcon:
      return told.falcon_skill;
  }
  return 0;
}

// Le niveau de la compétence qui appelle ce compagnon-là. ⚠ ZÉRO VEUT DIRE
// « PAS APPRISE », donc PAS DE CASE : un métier qui n'a pas de chariot ne doit
// pas en voir une grisée, il n'en aura jamais.
//
// 🔴 L'IDENTIFIANT VIENT DU PAQUET DES COMPAGNONS, LE NIVEAU DU GRIMOIRE. Le
// premier ne change pas d'un métier à l'autre ; le second change, et seul le
// grimoire est renvoyé quand il change. Le grimoire porte aussi, au niveau
// zéro, ce que le personnage pourrait apprendre sans l'avoir fait.
[[nodiscard]] inline std::uint16_t CompanionLevelOf(const CompanionState& told,
                                                    const SkillBook& book, CompanionKind kind) {
  const std::uint16_t skill_id = CompanionSkillOf(told, kind);
  if (skill_id == 0) {
    return 0;
  }
  const KnownSkill* known = book.Find(skill_id);
  if (known == nullptr) {
    return 0;
  }
  return known->level;
}

}  // namespace ro::sim
