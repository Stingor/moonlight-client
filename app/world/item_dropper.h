#pragma once

#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>

#include "ro/sim/actor.h"
#include "ro/sim/world_fwd.h"

// QUI VIENT DE LÂCHER UN OBJET AU SOL.
namespace moonclient {

// À QUELLE DISTANCE LE SERVEUR PEUT POSER UN BUTIN DE SON LÂCHEUR, en cases et
// SUR CHAQUE AXE.
//
// 🔴 DEUX POUR UN OBJET QU'UN JOUEUR JETTE, ET UN SEULEMENT POUR UN BUTIN DE
// CRÉATURE. `pc_dropitem` passe `DIR_MAX` et le drapeau 2, ce qui envoie
// `map_search_freecell_dist` chercher une case libre à DEUX ;
// `mob_process_drop_list` passe le drapeau 4 et se contente d'une. C'est la
// borne la plus large qui décide ici.
inline constexpr float kDropSearchCells = 2.0F;

// Et de combien le SOUS-POSITIONNEMENT écarte en plus, par axe : le serveur tire
// `rnd_value(1, 4) * 3` seizièmes de case, soit 3 à 12 là où le centre est 8.
inline constexpr float kDropSubCellCells = 5.0F / 16.0F;

// Jusqu'où l'on cherche QUI a lâché l'objet, en cases : la DIAGONALE des deux
// bornes ci-dessus, soit un peu plus de trois cases.
//
// 🔴 DEUX CASES ET DEMIE NE SUFFISAIENT PAS, et la trace l'a montré : un objet
// jeté à deux cases EN DIAGONALE est à 2,9 cases, et son lâcheur n'était pas
// trouvé. Le butin partait alors d'à côté de sa propre case — « hors du
// sprite », une fois sur trois et sans qu'on puisse le prévoir.
//
// ⚠ UN SEUIL SE DÉRIVE D'UNE BORNE DU PROTOCOLE, il ne se pose pas au jugé :
// celui d'avant séparait bien au cas ordinaire et se retournait au cas limite.
inline constexpr float kItemArcSourceRangeCells =
    (kDropSearchCells + kDropSubCellCells) * std::numbers::sqrt2_v<float>;

// L'entité la plus proche de la case de l'objet, ou NUL s'il n'y en a aucune à
// portée.
//
// 🔴 LE PAQUET NE NOMME PERSONNE, et aucun octet de réseau n'a été ajouté pour
// le dire : c'est la PROXIMITÉ qui tranche, et elle suffit. Deux faits l'ont
// rendue fiable — le serveur pose le butin sur la case de la créature ou l'une
// des voisines, et il envoie les butins sans délai tout en ne retirant l'unité
// qu'à +250 ms. L'ordre joue en notre faveur ; il aurait pu être l'inverse.
//
// ⚠ ON NE FILTRE PAS SUR L'ESPÈCE. Un joueur qui jette un objet à ses pieds le
// fait jaillir de lui autant qu'une créature qui meurt, et rien dans le paquet
// ne dit laquelle des deux c'est.
//
// ⚠ ON ÉCARTE LES OBJETS AU SOL, ET EUX SEULS : un butin déjà tombé à côté ne
// lâche rien, et le prendre pour source ferait partir chaque pièce d'une pluie
// de la précédente, en file indienne au lieu d'une gerbe.
[[nodiscard]] const ro::sim::Actor* DropperNear(const ro::sim::World& world,
                                                const ro::sim::Actor& item, std::uint32_t now_ms);

// COMBIEN LA PILE EN PORTE, à écrire à la suite du nom, ou VIDE.
//
// 🔴 C'EST UN AJOUT, ET LE CLIENT NE LE FAIT PAS. Il reçoit pourtant le compte
// et le range dans son objet au sol — rien ne l'y relit, pas même son
// nameplate, qui ne porte qu'un rectangle de désignation. Un joueur qui voit une
// pile au sol ne peut donc pas savoir ce qu'elle vaut avant de la ramasser.
//
// ⚠ VIDE POUR UNE PILE D'UN SEUL, et pour tout ce qui n'est pas un objet au
// sol : « Red Potion (1) » n'apprend rien et alourdirait chaque butin d'une
// carte encombrée.
[[nodiscard]] std::string GroundItemAmount(const ro::sim::Actor& actor);

// ── L'OUTIL DE RÉGLAGE ──────────────────────────────────────────────────────

// Combien de faux butins la commande sème par défaut, et l'objet qu'ils
// portent — une potion rouge, dont le sprite est dans toute installation.
inline constexpr std::size_t kFakeLootCount = 8;
inline constexpr std::uint16_t kFakeLootItemId = 501;

// LE MODE « TOURNANT » : chaque butin prend le pilier suivant, si bien qu'une
// seule commande montre les cinq couleurs côte à côte.
inline constexpr std::uint16_t kFakeLootCyclingMode = 0xFFFF;

// D'où partent les identifiants des faux butins.
//
// 🔴 HORS DE TOUTE PLAGE DU SERVEUR, et il le faut : ces objets n'existent que
// chez nous. Un identifiant qui tomberait sur celui d'un vrai butin le
// remplacerait dans le monde, et le ramassage suivant viserait un objet que le
// serveur ne connaît pas.
inline constexpr std::uint32_t kFakeLootFirstId = 0xF0000000;

// FAIT PLEUVOIR DE FAUX BUTINS AUTOUR DU JOUEUR, pour régler le jaillissement
// sans attendre qu'une créature en lâche un. Rend combien ont été posés.
//
// 🔴 CE SONT DES LEURRES, ET LE SERVEUR N'EN SAIT RIEN : cliquer dessus
// demanderait un objet qui n'existe pas, et le serveur se tairait. Ils ne
// servent qu'à REGARDER — la trajectoire, la colonne, le scintillement.
//
// Chaque appel efface ceux du précédent : un compte NUL les retire tous, et
// c'est ainsi qu'on nettoie la carte.
//
// ⚠ ILS TOMBENT EN CERCLE AUTOUR DU JOUEUR, sur les cases voisines — là où le
// serveur pose les siens. Semés sur la même case, ils se superposeraient et le
// jaillissement ne se jugerait pas.
std::size_t SowFakeLoot(ro::sim::World& world, std::size_t count, std::uint16_t item_id,
                        std::uint16_t drop_effect_mode, std::uint32_t now_ms);

}  // namespace moonclient
