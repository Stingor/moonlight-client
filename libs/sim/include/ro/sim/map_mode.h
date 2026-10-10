#pragma once

#include <array>
#include <cstdint>

namespace ro::sim {

// ── CE QUE LE SERVEUR DÉCLARE DE LA CARTE OÙ L'ON VIENT D'ENTRER ────────────
//
// Une carte ne se joue pas partout pareil : sur l'une, les autres joueurs ne
// sont que des passants ; sur l'autre, ils se frappent. Le serveur le dit en
// une trame, `ZC_MAPPROPERTY_R2 0x099B`, et rien d'autre ne le dit.
//
// 🔴 C'EST UNE DÉCLARATION D'INTERFACE, PAS LA RÈGLE DU COMBAT. Le serveur ne
// décrit ici que ce que le CLIENT doit montrer — la forme du curseur, le
// compteur, l'emblème. Ce qu'il AUTORISE réellement, c'est `battle_check_target`
// qui en décide, et cette fonction lit des mapflags que la trame NE PORTE PAS.
// Les deux divergent, et c'est mesuré des deux côtés :
//
//   · `pvp_noguild` et `pvp_noparty` n'ont AUCUN bit ici. Sur `pvp_n_3-5`, qui
//     porte `pvp_noguild`, le serveur laisse frapper ses propres compagnons de
//     guilde — le client, lui, n'en saura jamais rien.
//   · `attackable_beyond_party` sort de `mapdata_flag_gvg2`, que l'heure de
//     WoE ne garde pas, alors que le serveur tranche sur `mapdata_flag_gvg`,
//     qui la garde. Hors WoE, la carte de siège annonce GvG et le serveur
//     refuse quand même le coup.
//
// ⚠ ET SIX ÉTATS INDIVIDUELS NE VOYAGENT DANS AUCUN PAQUET : `@killable`,
// `@killer`, `@monsterignore` (PCBLOCK_IMMUNE), `disable_pvp`, l'invincibilité
// d'entrée sur carte — quatre secondes ici — et le groupe de duel d'AUTRUI. Un
// client qui prétendrait dire qui est vraiment frappable mentirait six fois.
//
// D'où la règle tenue ici : **le client désigne, le serveur dispose.** On peint
// l'intention — ce joueur se vise — et l'on ne promet jamais que le coup
// portera. C'est aussi ce que fait le client d'origine.
//
// ⚠ LE SEUL ÉTAT INDIVIDUEL QUE LE PROTOCOLE PORTE EST LE KARMA : le champ
// `isPKModeON` des paquets d'apparition vaut le karma de son porteur. Il vit
// donc sur l'acteur, pas ici.

// CE QU'UNE CARTE EST, du point de vue du combat entre joueurs.
//
// Ce sont les valeurs de `enum map_property` (src/map/clif.hpp du serveur),
// et le champ `type` de la trame les porte telles quelles.
//
// ⚠ TROIS SEULEMENT SONT ÉMISES. `kEventPvpZone`, `kPkServerZone`,
// `kPvpServerZone` et `kDenySkillZone` n'apparaissent dans aucun `.cpp` du
// serveur : les attendre serait attendre ce qui ne vient jamais. On les nomme
// quand même, pour que la valeur reçue ait un nom si elle arrivait un jour.
enum class MapKind : std::uint8_t {
  // Une carte ordinaire. C'est aussi ce que le serveur redit en QUITTANT une
  // zone de combat, et c'est ainsi que le mode s'éteint.
  kNothing = 0,
  // La zone PVP libre — celle du mapflag `pvp`, celle d'un duel, et celle d'un
  // donjon de siège, que le serveur y range faute de mieux (son propre
  // commentaire dit « TODO: Figure out the real packet to send here »).
  kFreePvpZone = 1,
  kEventPvpZone = 2,
  // La zone de siège : `mapdata_flag_gvg`, l'heure de WoE comprise.
  kAgitZone = 3,
  kPkServerZone = 4,
  kPvpServerZone = 5,
  kDenySkillZone = 6,
};

// LES ONZE DRAPEAUX de `ZC_MAPPROPERTY_R2`, un par bit du champ `flags`.
//
// 🔴 DES BOOLÉENS NOMMÉS, ET NON UN MOT DE BITS. Le serveur écrit un `WBUFL`
// brut et sa propre structure nommée — `packet_maptypeproperty2` — n'est employée
// nulle part : les onze sens ne vivent que dans onze commentaires de fin de
// ligne. Les renommer ici est le seul endroit du programme où ils redeviennent
// lisibles, et un décalage d'un bit ne se verrait nulle part ailleurs.
//
// ⚠ LES NOMS SONT CEUX DE L'EFFET, PAS CEUX DU SERVEUR. `PARTY` et `GUILD` ne
// disent rien de ce qu'ils font ; ils veulent dire « le curseur d'attaque
// paraît au-delà du groupe » et « au-delà de la guilde ». Un nom qui ne se
// comprend qu'en relisant le serveur est un nom qui sera mal employé.
struct MapMode {
  MapKind kind = MapKind::kNothing;

  // Le curseur d'attaque paraît sur tout joueur qui n'est pas de MON GROUPE.
  // Levé par le mapflag `pvp`, et par un duel en cours.
  bool attackable_beyond_party = false;
  // … et sur tout joueur qui n'est pas de MA GUILDE. Levé en champ de bataille
  // et en zone GvG.
  bool attackable_beyond_guild = false;
  // L'emblème de guilde paraît sous les noms. Même provenance que le précédent.
  bool guild_emblems_shown = false;
  // Les effets de compétence se jouent dans leur forme simplifiée, comme sous
  // la commande `/mineffect`.
  bool simple_effects_forced = false;
  // ⚠ CE DRAPEAU ÉLARGIT L'ATTAQUE, IL NE LA RESTREINT PAS, et son nom chez le
  // serveur dit l'inverse de ce qu'il fait. Le commentaire de rAthena annonce
  // « only allow attacks on other players with shift key or /ns active » ; le
  // client, lui, le met en DISJONCTION avec la zone PVP et la zone GvG, si bien
  // qu'il rend hostile SANS qu'aucune touche soit tenue. Il remplace MAJ, il ne
  // l'exige pas.
  bool attackable_without_shift = false;
  // Le compteur de rang PVP paraît. ⚠ LEVÉ PAR LE SEUL MAPFLAG `pvp` : ni le
  // duel, ni le GvG, ni le champ de bataille ne le lèvent.
  bool pvp_counter_shown = false;
  // La création et la modification de groupe sont refusées.
  bool party_changes_refused = false;
  // Champ de bataille : l'hostilité ne se décide plus que par le CAMP.
  bool battlefield = false;
  // Les apparences de costume ne se montrent pas.
  bool costumes_hidden = false;
  // ⚠ CES DEUX-LÀ SONT DES PERMISSIONS, et le serveur les écrit à l'ENVERS des
  // autres : il lève le bit quand le mapflag d'interdiction est ABSENT. Le
  // défaut « faux » d'un agrégat neuf voudrait donc dire « chariot interdit »
  // sur une carte dont on n'a rien reçu — ce qui n'arrive pas, le serveur
  // parlant à chaque entrée, mais ce qui se lirait mal.
  bool cart_allowed = false;
  bool sun_moon_star_miracle_allowed = false;
};

// Vrai quand la carte fait des autres joueurs des adversaires, à quelque titre
// que ce soit.
//
// 🔴 C'EST CE QUI OUVRE LE CIBLAGE, et c'est une DISJONCTION parce que le
// client d'origine en fait une : sa cascade de survol essaie le champ de
// bataille, puis le groupe, puis la guilde, et retombe sur cette réunion-là.
// Tester le seul `kFreePvpZone` laisserait le curseur en flèche sur une carte
// de siège.
[[nodiscard]] constexpr bool IsVersusMap(const MapMode& mode) noexcept {
  return mode.attackable_beyond_party || mode.attackable_beyond_guild ||
         mode.attackable_without_shift || mode.battlefield;
}

// LE TYPE DE CARTE QUE DIT `ZC_NOTIFY_MAPPROPERTY2 0x01D6` — l'énumération
// `e_map_type` du serveur, qui n'est pas celle de `MapKind`. Le serveur ne
// l'envoie que sur un champ de bataille.
inline constexpr std::uint16_t kMapTypeBattlefield = 19;

// LES TYPES DE CARTE OÙ UN MEMBRE DU GROUPE VOIT LES EFFETS D'UN AUTRE MÊME
// CACHÉ, dans l'ordre de `e_map_type` : zone PK avec et sans pénalité, guerre
// de guilde d'événement, siège, donjon 2, serveur PK, course turbo, bataille de
// porings, siège v15, champ de bataille, siège de bas niveau, puis 31 et 32 que
// le serveur ne nomme pas. Le client éteint ce droit à chaque carte neuve.
inline constexpr std::array<std::uint16_t, 13> kMapTypesWherePartySeesConcealedEffects = {
    5, 6, 7, 8, 9, 11, 14, 17, 18, kMapTypeBattlefield, 25, 31, 32};

[[nodiscard]] constexpr bool PartySeesConcealedEffects(std::uint16_t map_type) noexcept {
  for (const std::uint16_t type : kMapTypesWherePartySeesConcealedEffects) {
    if (type == map_type) {
      return true;
    }
  }
  return false;
}

}  // namespace ro::sim
