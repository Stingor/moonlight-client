#pragma once

#include <array>
#include <cstdint>

#include "ro/sim/inventory.h"
#include "ro/ui/character_window.h"

namespace moonclient {

// OÙ LE SERVEUR RANGE CE QU'UNE CASE DU MANNEQUIN MONTRE.
//
// ⚠ C'EST LE SEUL ENDROIT QUI CONNAÎT LES DEUX CÔTÉS. `ui` nomme des PLACES —
// elle ne sait rien des drapeaux du serveur —, `sim` nomme des DRAPEAUX — elle
// ne sait rien du dessin. La correspondance est un fait à part entière, et c'est
// pour cela qu'elle vit dans son propre fichier plutôt qu'au milieu de la vue :
// une règle qu'on ne peut pas essayer seule finit par se vérifier à l'écran.
//
// 🔴 LES COSTUMES PORTENT LEURS PROPRES BITS et se montrent aux mêmes places.
// C'est `costume` qui tranche, pas la case : chercher un chapeau d'apparat par
// le bit du chapeau ordinaire ne rend jamais rien, et la case reste vide sans
// que rien ne l'explique.
//
// Rend ZÉRO pour une case qui n'existe pas dans le mode demandé — les mains,
// l'armure, les chaussures et les accessoires n'ont pas d'apparat.
[[nodiscard]] std::uint32_t EquipLocationOf(ro::ui::EquipSlot slot, bool costume);

// LE MANNEQUIN MONTRE-T-IL CE QUI EST PORTÉ LÀ ?
//
// 🔴 C'EST LA GARDE DE « LE PORTÉ QUITTE LE SAC ». Une pièce que le mannequin
// montre n'a plus à encombrer la grille ; une pièce qu'il ignore doit y RESTER,
// sinon elle n'est plus nulle part — et ce qu'on ne voit plus, on ne peut plus
// l'ôter. L'équipement d'ombre est dans ce cas : il se porte à des places
// qu'aucune case de la fiche ne lit.
//
// ⚠ IL SE DÉDUIT DES CASES, il ne se liste pas. Une case ajoutée au mannequin
// entre ici sans qu'on y pense ; une liste écrite à la main aurait vieilli en
// silence, et son défaut se serait lu « cet objet a disparu de mon sac ».
//
// Le masque peut porter plusieurs bits — une arme à deux mains en porte deux —
// et un seul montré suffit : la pièce est alors visible.
[[nodiscard]] bool ShownOnDoll(std::uint32_t worn_at);

// Si une pièce de ce masque se montre dans la grille du sac : tout ce que le
// mannequin ne montre pas, et les munitions même portées. Elles se DÉPENSENT :
// c'est dans le sac, avec leur nombre, qu'on les surveille et qu'on en change —
// comme dans le client.
[[nodiscard]] bool ShownInBag(std::uint32_t worn_at);

// OÙ LA PIÈCE DOIT ALLER, selon que le joueur a tenu Ctrl ou non.
//
// 🔴 ON DEMANDE LA MAIN GAUCHE SANS VÉRIFIER QU'ELLE EST POSSIBLE, et c'est
// voulu : savoir si un personnage peut manier deux armes demande son métier, ses
// compétences et la nature de l'arme — trois choses que le SERVEUR sait et que
// nous devrions recopier pour les tenir à jour. Il refuse tout seul, et son refus
// est déjà traité comme tel.
//
// ⚠ UNE ARME ORDINAIRE NE DÉCLARE QUE LA DROITE : la gauche se DEMANDE, elle ne
// se lit pas sur l'objet.
//
// 🔴 CHEZ UN AMBIDEXTRE, LA MÊME DAGUE DÉCLARE LES DEUX MAINS : le serveur
// annonce alors `kEquipBothHands`, exactement comme pour une arme à deux mains,
// et choisit la main lui-même si on lui renvoie ce masque. Ctrl le resserre sur
// la gauche. Une arme à deux mains, qui porte le même masque, ne s'en trouve
// pas changée : le serveur l'accepte par la gauche et la pose sur les deux.
[[nodiscard]] std::uint32_t WearAt(std::uint32_t wearable_at, bool alternate);

// CE QU'UNE CASE DE BARRE FAIT DE L'OBJET QU'ELLE DÉSIGNE.
enum class ShortcutItemAct : std::uint8_t {
  kUse,
  kWear,
  kNothing,
};

struct ShortcutItemGesture {
  ShortcutItemAct act = ShortcutItemAct::kNothing;
  // L'emplacement demandé au serveur, pour `kWear` seulement.
  std::uint32_t wear_at = 0;
};

// 🔴 UNE PIÈCE QUI SE PORTE S'ÉQUIPE, ELLE NE S'UTILISE PAS : le serveur refuse
// en silence la demande d'usage d'un équipement ou d'une munition.
//
// ⚠ UNE PIÈCE DÉJÀ PORTÉE NE FAIT RIEN — la touche n'ôte pas —, une pièce non
// identifiée non plus. Les munitions échappent aux deux : la demande repart même
// portées, et le serveur les pose toujours à leur place.
[[nodiscard]] ShortcutItemGesture ShortcutGestureOf(const ro::sim::InventoryItem& item);

// L'EXEMPLAIRE DU SAC QU'UNE CASE DE BARRE VISE : le premier qui n'est pas porté,
// à défaut le premier tout court. Deux exemplaires d'une même pièce, l'un porté,
// l'autre pas : la touche doit équiper le second, pas buter sur le premier.
[[nodiscard]] const ro::sim::InventoryItem* ShortcutTargetIn(const ro::sim::Inventory& bag,
                                                             std::uint32_t item_id);

// CE QUE LE PERSONNAGE PORTE À LA TÊTE ET AU DOS, tel que le catalogue le
// dessine : les numéros d'apparence que le serveur donne à chaque pièce.
struct WornViews {
  // Dans l'ordre de l'apparence : la coiffe basse, celle du milieu, la haute.
  std::array<std::uint16_t, 3> headgears{};
  std::uint16_t robe = 0;
};

// Les coiffes et la cape PORTÉES — sans les costumes, et sans ce qu'un script
// impose à l'apparence d'une coiffe.
//
// 🔴 UNE COIFFE QUI COUVRE PLUSIEURS PLACES NE SE DESSINE QU'À LA PLUS HAUTE, et
// les autres restent vides : la dessiner à chacune la superposerait à
// elle-même. ⚠ ON COMPARE LES PIÈCES, JAMAIS LEURS APPARENCES : deux coiffes
// distinctes peuvent en partager une, et chacune reste alors à sa place.
[[nodiscard]] WornViews WornPieceViews(const ro::sim::Inventory& bag);

}  // namespace moonclient
