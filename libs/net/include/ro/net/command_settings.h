#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/command_settings.h"
#include "ro/sim/item_facts.h"

namespace ro::net {

// LES RÉGLAGES DE COMMANDE, SUR LE FIL.
//
//   ZC 0x0F05  les réglages de session — voir `ro/net/player_settings.h`
//   CZ 0x0F04  un réglage que le joueur change, [op][len][id:2][value:4]
//   CZ 0x0F06  un ordre sur les presets, [op][len][cmd:1][slot:1] puis le nom
//   ZC 0x0F07  la liste des presets, [op][len][active:1][count:1] puis count ×
//              [slot:1][autoload:1][namelen:1][nom]
//
// 🔴 LES IDENTIFIANTS SONT UN CONTRAT DE PROTOCOLE, les numéros mêmes que le
// serveur donne à ses réglages : ils ne se renumérotent jamais.
inline constexpr std::uint16_t kSettingShowExp = 0;
inline constexpr std::uint16_t kSettingShowZeny = 1;
inline constexpr std::uint16_t kSettingShowMobInfo = 2;
inline constexpr std::uint16_t kSettingSeparateKills = 3;
inline constexpr std::uint16_t kSettingBlockExp = 4;
inline constexpr std::uint16_t kSettingAutolootRares = 5;
// ⚠ EN POUR-CENT SUR LE FIL, de zéro à cent : le serveur le garde en centièmes.
inline constexpr std::uint16_t kSettingAutolootRate = 6;
// ⚠ DIVISÉ PAR CENT SUR LE FIL, pour tenir dans seize bits côté client.
inline constexpr std::uint16_t kSettingAutolootMinZeny = 7;
inline constexpr std::uint16_t kSettingAutolootTypes = 8;
inline constexpr std::uint16_t kSettingShowSkillDelay = 10;
inline constexpr std::uint16_t kSettingShowSpeed = 11;
inline constexpr std::uint16_t kSettingSellStuff = 12;
inline constexpr std::uint16_t kSettingSellItem = 13;
inline constexpr std::uint16_t kSettingNoAsk = 14;
inline constexpr std::uint16_t kSettingKillStealGuard = 15;
inline constexpr std::uint16_t kSettingWings = 16;
inline constexpr std::uint16_t kSettingAutolootMvp = 17;
inline constexpr std::uint16_t kSettingAutolootMvpRewards = 18;
inline constexpr std::uint16_t kSettingFlyWingMarker = 27;

// Ce que vaut une unité du seuil de zeny, sur le fil.
inline constexpr int kAutolootMinZenyWireUnit = 100;

// L'en-tête de la liste des presets, avant ses entrées : opcode, longueur, preset
// actif, compte.
inline constexpr std::size_t kAutolootPresetListHeader = 6;
// Ce que pèse une entrée avant son nom : emplacement, chargement auto, longueur.
inline constexpr std::size_t kAutolootPresetEntryHeader = 3;
// L'en-tête d'un ordre, avant le nom : opcode, longueur, ordre, emplacement.
inline constexpr std::size_t kAutolootPresetOrderHeader = 6;
// Le nom le plus long que le serveur retient, en octets.
inline constexpr std::size_t kAutolootPresetNameBytes = 50;

// Ce qu'on demande au serveur sur un preset. 🔴 LES VALEURS SONT CELLES DU FIL.
enum class AutolootPresetOrder : std::uint8_t {
  kSave = 2,
  kLoad = 3,
  kDelete = 4,
  // L'emplacement zéro éteint le chargement automatique de tous.
  kAutoload = 5,
  kRename = 6,
};

// Lit les réglages de commande dans une trame 0x0F05, et la liste des presets
// dans une trame 0x0F07.
//
// ⚠ UNE TRAME 0x0F05 NE PORTE PAS FORCÉMENT TOUT : la resynchronisation de la
// liste de ramassage n'en contient que la liste. Seul ce qui est présent change,
// et `told` ne se lève que sur une trame qui porte au moins un réglage.
//
// ⚠ `facts` DONNE LA LISTE DE RAMASSAGE à retenir comme « sauvegardée » quand
// les presets arrivent. Elle doit donc avoir lu la même trame 0x0F05 AVANT : le
// serveur annonce la liste chargée, puis les presets.
//
// ⚠ `kIgnored` pour 0x0F05 : d'autres lecteurs y prennent leur part, et rien de
// ce qui se DESSINE sur la carte n'en dépend.
[[nodiscard]] FrameOutcome ApplyCommandSettingsFrame(const Frame& frame,
                                                     sim::CommandSettings& settings,
                                                     const sim::ItemFacts& facts);

// Ordonne au serveur une opération sur un preset. Le nom n'est lu que pour une
// sauvegarde ou un renommage, et tronqué à ce que le serveur retient.
//
// ⚠ LE SERVEUR RÉPOND PAR LA LISTE DES PRESETS, et c'est elle qui fait foi : rien
// ne se note ici d'avance.
void SendAutolootPresetOrder(Connection& link, AutolootPresetOrder order, std::uint8_t slot,
                             std::string_view name = {});

}  // namespace ro::net
