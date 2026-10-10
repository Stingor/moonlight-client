#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

namespace ro::vfs {

// LE NOM AFFICHÉ D'UN OBJET SE COMPOSE — il ne se lit nulle part.
//
// « +7 [A] Double Lucky Sword [2] » n'existe dans aucun fichier : le client le
// fabrique à chaque affichage à partir de ce que le fil donne et de trois tables
// de l'installation. La règle est relevée dans l'ADR 0035, fonction par
// fonction.
//
// ⚠ IL VIT DANS `vfs` ET NON DANS `sim`. Il lui faut la table des objets et
// celle des mots, qui sont des fichiers ; `sim` ne connaît pas le `vfs`, et
// c'est bien ainsi — la simulation n'a que faire de l'orthographe d'un nom.
// C'est `app` qui bâtit les faits ci-dessous depuis un objet du sac.

// Ce que le FIL dit d'un objet, et rien de plus.
struct ItemNameFacts {
  std::uint32_t item_id = 0;

  // Le type du serveur : c'est lui qui décide si l'objet se décore de mots de
  // cartes ou se contente de son nom.
  std::int32_t type = 0;

  std::int32_t refine_level = 0;
  std::int32_t grade = 0;

  std::array<std::uint32_t, kItemNameCardSlots> cards{};

  // Le nom de qui a forgé l'objet. ⚠ LE FIL NE LE PORTE PAS : il ne donne qu'un
  // identifiant de personnage, et il faut une demande au serveur pour le
  // traduire. Vide tant que la réponse n'est pas là — et le client d'origine
  // fait exactement pareil pendant sa première seconde.
  std::string creator_name;

  bool identified = true;
};

// Le rôle d'un morceau de nom. Il décide de sa COULEUR, jamais de son texte.
enum class ItemNameRole {
  kOrdinary,
  kCreator,         // le nom est connu
  kUnknownCreator,  // le serveur ne l'a pas encore dit
};

struct ItemNameSegment {
  std::string text;
  ItemNameRole role = ItemNameRole::kOrdinary;
};

// Les types d'objets qui se décorent : armes, protections, accessoires. Le
// serveur emploie les mêmes nombres (`itemdb_isequip2`).
[[nodiscard]] bool ItemTypeIsDecorated(std::int32_t type);

// Les deux plages d'identifiants que le client traite à part : œufs de
// familiers et leurs accessoires. Seule la première carte y compte.
[[nodiscard]] bool ItemIdIsPetRange(std::uint32_t item_id);

// Le suffixe d'emplacements se compose, ou non.
//
// ⚠ LE BANDEAU D'OBTENTION LE TAIT, et c'est le client qui le tait : son
// name-builder ne compose « [N] » que si son dernier argument vaut 1, et le
// chemin du bandeau passe 0.
enum class ItemNameSlots { kShown, kHidden };

// Compose le nom, en MORCEAUX.
//
// 🔴 PAS EN UNE CHAÎNE, et c'est le fond du sujet : le client dessine chaque
// morceau avec sa propre couleur. Les aplatir ici obligerait à retrouver les
// bornes plus tard, c'est-à-dire à recomposer.
//
// Le suffixe « [N] » vient de la table des objets, pas du fil, et s'ajoute dès
// qu'elle déclare des emplacements — à moins que l'appelant ne le taise.
[[nodiscard]] std::vector<ItemNameSegment> ComposeItemName(
    const ItemNameFacts& facts, const ItemTable& items, const ItemWords& words,
    ItemNameSlots slots = ItemNameSlots::kShown);

// Le même nom, aplati — pour une infobulle, un journal, un relevé.
[[nodiscard]] std::string ItemNameText(std::span<const ItemNameSegment> segments);

}  // namespace ro::vfs
