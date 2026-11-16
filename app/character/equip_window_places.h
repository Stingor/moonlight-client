#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ro/sim/inventory.h"
#include "ro/ui/character_window.h"
#include "ro/ui/equip_window.h"

#include "character/equip_slot_map.h"

namespace moonclient {

// OÙ LE SERVEUR RANGE CE QUE MONTRE LA FENÊTRE D'ÉQUIPEMENT DU JEU.
//
// ⚠ À PART DE SA COMPOSITION, et pour la raison de `equip_slot_map.h` : ces
// règles ne connaissent que `sim` et `ui`, et l'échange de deux places voisines
// est muet en jeu tant que le personnage n'en porte qu'une.

// LES BITS QU'UNE PLACE MONTRE SUR CET ONGLET : celui de la place sur
// l'onglet Général ; le costume ou l'ombre qui la double sur l'onglet Costume.
// Zéro pour une place qui n'existe pas.
[[nodiscard]] std::uint32_t EquipWindowLocationOf(std::size_t place, ro::ui::EquipWindowTab tab);

// LA PIÈCE PORTÉE À L'UN DE CES BITS, ou nulle. ⚠ UN SEUL SUFFIT, à l'inverse de
// `Inventory::WornAt` : une arme à deux mains se montre à la place de l'arme
// comme à celle du bouclier.
[[nodiscard]] const ro::sim::InventoryItem* EquipWindowPieceOn(const ro::sim::Inventory& bag,
                                                               std::uint32_t location);

// LES PLACES OÙ UNE PIÈCE QUI SE PORTE À CES BITS PEUT ALLER, sur l'un ou
// l'autre onglet : c'est là que la fenêtre l'entoure pendant qu'on la traîne.
[[nodiscard]] std::array<bool, ro::ui::kEquipWindowPlaceCount> EquipWindowPlacesFor(
    std::uint32_t wearable_at);

// CE QUE « TOUT RETIRER » DEMANDE au serveur pour cet onglet : les dix places
// et la munition, ou tout ce que l'onglet Costume montre.
[[nodiscard]] std::uint32_t EquipWindowTakeOffAllMask(ro::ui::EquipWindowTab tab);

// LES COIFFES ET LA CAPE QUE LA SILHOUETTE PORTE SUR CET ONGLET : celles de ses
// places, et seulement elles. ⚠ UNE COIFFE QUI COUVRE PLUSIEURS PLACES NE SE
// DESSINE QU'À LA PLUS HAUTE, comme sur la carte.
[[nodiscard]] WornViews EquipWindowWornViews(const ro::sim::Inventory& bag,
                                             ro::ui::EquipWindowTab tab);

// LES GESTES DE LA FENÊTRE VERSÉS DANS CEUX DE LA FICHE, que la même route
// joue ensuite — retrait, pose, description, lien, réglages, compagnon.
//
// ⚠ CE QUE LA FICHE A DEMANDÉ À CETTE IMAGE GAGNE : une seule souris ne fait
// pas deux gestes à la fois, et un champ déjà posé n'est pas écrasé.
void MergeSheetGestures(ro::ui::CharacterWindowAction& into,
                        const ro::ui::CharacterWindowAction& from);

}  // namespace moonclient
