#pragma once

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── QUICK CAST : LANCER EN UNE ACTION ───────────────────────────────────────
//
// Les cinq réglages de la section « Quick cast », tous éteints par défaut.
// Ils se montrent à DEUX endroits, le panneau d'options et l'établi du staff,
// et c'est le DROIT qui change les bornes et les textes, pas la fenêtre qui
// affiche.
struct QuickCastLook {
  // Un sort de zone armé au clavier part sur la case sous la souris.
  bool ground = false;
  // Un sort ciblé armé au clavier part sur l'entité sous la souris.
  bool target = false;
  // La période de relance tant que la touche est tenue, en millisecondes.
  int repeat_ms = 200;
  // Une case d'objet se réutilise tant que sa touche est tenue.
  bool item = false;
  // La période de réutilisation d'un objet, en millisecondes. ⚠ SOUS LE
  // PLANCHER DU DROIT, elle ne vaut rien : voir `QuickCastItemPeriodMs`.
  int item_repeat_ms = 50;

  // Engendrée : un champ ajouté entre de lui-même dans le relevé.
  friend bool operator==(const QuickCastLook&, const QuickCastLook&) = default;
};

inline constexpr int kQuickCastRepeatLowestMs = 50;
inline constexpr int kQuickCastRepeatStaffLowestMs = 10;
inline constexpr int kQuickCastRepeatHighestMs = 1000;
inline constexpr int kQuickCastItemHighestMs = 1000;

// 🔴 LE PLANCHER DE LA CADENCE D'OBJET, ET C'EST LA SEULE CHOSE QUI SÉPARE LE
// STAFF D'UN JOUEUR. Le serveur ramène l'intervalle entre deux objets à vingt
// millisecondes à partir du niveau de groupe quarante, au lieu des trois cent
// vingt-cinq habituels : le staff peut donc descendre jusque-là. Pour tout le
// monde, en dessous de cent, le serveur répond « veuillez patienter » et rien ne
// va plus vite.
inline constexpr int kQuickCastItemFloorStaffMs = 20;
inline constexpr int kQuickCastItemFloorPlayerMs = 100;

[[nodiscard]] constexpr int QuickCastItemFloorMs(bool staff) noexcept {
  return staff ? kQuickCastItemFloorStaffMs : kQuickCastItemFloorPlayerMs;
}

// LA PÉRIODE EFFECTIVE d'un objet. ⚠ ÉTAGE DE DROIT, PAS D'INTERFACE : le
// plancher s'applique ici, sur ce qui part, pour qu'un fichier de réglages
// édité à la main ne le contourne pas.
[[nodiscard]] constexpr int QuickCastItemPeriodMs(int wanted_ms, bool staff) noexcept {
  const int floor_ms = QuickCastItemFloorMs(staff);
  return wanted_ms > floor_ms ? wanted_ms : floor_ms;
}

// Les libellés et les aides, partagés par les deux endroits qui les montrent.
// ⚠ MARQUÉS, PAS TRADUITS : une constante ne peut pas appeler `Tr`. C'est
// `QuickCastTextsFor` qui les traduit, à la lecture.
inline constexpr const char* kQuickCastTitle = core::TrText("Quick cast");
inline constexpr const char* kQuickCastGroundLabel =
    core::TrText("Sort de zone : cast direct sous la souris");
inline constexpr const char* kQuickCastGroundHelp = core::TrText(
    "Un skill de ZONE (Storm Gust, Meteor Storm…) part immédiatement sur la cellule sous le "
    "curseur : "
    "plus besoin du clic de confirmation.\n\nSi aucune cellule valide n'est visée (ciel, "
    "interface), le mode ciblage classique reste armé. L'animation, la barre de cast et l'envoi "
    "restent ceux du jeu : le serveur reste maître du lancement.");
inline constexpr const char* kQuickCastTargetLabel =
    core::TrText("Sort ciblé : cast direct sur la cible survolée");
inline constexpr const char* kQuickCastTargetHelp = core::TrText(
    "Un skill CIBLÉ part immédiatement si une cible compatible est sous le curseur : monstre ou "
    "joueur pour un sort offensif ; joueur, monstre ou soi-même pour un soutien.\n\nSans cible "
    "compatible sous le curseur, le mode ciblage classique reste armé — rien n'est perdu. C'est "
    "le SERVEUR qui juge si un joueur peut être attaqué (PVP/GVG).");
inline constexpr const char* kQuickCastRepeatLabel = core::TrText("Cadence (ms)");
inline constexpr const char* kQuickCastRepeatHelp = core::TrText(
    "Période de répétition quand tu MAINTIENS la touche : le skill s'enchaîne à ce rythme, en "
    "suivant le curseur.\n\nLe jeu, lui, ignore l'auto-répétition du clavier — un appui maintenu "
    "ne lance qu'une fois. Cette répétition est donc ajoutée par le client. Valeur haute = un "
    "lancement par pression.\n\nLe COOLDOWN réel du skill est de toute façon respecté : tant "
    "qu'il n'est pas prêt, rien n'est envoyé. Ce réglage ne sert qu'aux skills SANS cooldown, "
    "dont le rythme est fixé par le délai d'après-incantation — que le serveur ne communique pas "
    "au client.");
inline constexpr const char* kQuickCastItemLabel =
    core::TrText("Item : répéter tant que la touche est maintenue");
inline constexpr const char* kQuickCastItemHelp = core::TrText(
    "Une case de la barre d'action qui porte un ITEM (Old Blue Box, Dead Branch, potions…) "
    "s'utilise en boucle tant que tu gardes sa touche enfoncée, au lieu d'une fois par "
    "pression.\n\nÇa s'arrête tout seul quand tu relâches, quand la case change, ou quand il "
    "n'en reste plus dans l'Inventory. Cliquer la case, en revanche, ne répète rien : une seule "
    "utilisation, comme avant.\n\nAttention : chaque répétition CONSOMME un item — dont le "
    "dernier.");
inline constexpr const char* kQuickCastItemRepeatLabel = core::TrText("Cadence item (ms)");
inline constexpr const char* kQuickCastItemRepeatStaffHelp = core::TrText(
    "Période de répétition des items. Elle est SÉPARÉE de celle des skills parce que ce n'est "
    "pas le même frein : ici c'est le serveur qui fixe le minimum entre deux items.\n\nPour le "
    "staff, ce minimum est de 20 ms — le serveur descend l'intervalle à cette valeur au-dessus "
    "du niveau de groupe 40, au lieu des 325 ms habituels. Le curseur va donc jusque-là.\n\nEn "
    "dessous, rien ne va plus vite : le serveur refuse et répond « veuillez patienter » dans le "
    "chat. En pratique, la vraie limite est la fréquence d'images du client.");
inline constexpr const char* kQuickCastItemRepeatPlayerHelp = core::TrText(
    "Période de répétition des items, séparée de celle des skills parce que le frein n'est pas "
    "le même : c'est le SERVEUR qui fixe l'intervalle minimal entre deux items — 325 ms en temps "
    "normal, davantage en PvP sur les soins.\n\nRégler plus vite que lui ne consomme rien de "
    "plus : il refuse et répond « veuillez patienter » dans le chat. Ce curseur ne descend pas "
    "sous 100 ms.");

// Les textes, TRADUITS dans la langue courante. L'aide de la cadence d'item
// dépend du droit : c'est lui qui fixe la borne basse qu'elle explique.
struct QuickCastTexts {
  const char* title = "";
  const char* ground_label = "";
  const char* ground_help = "";
  const char* target_label = "";
  const char* target_help = "";
  const char* repeat_label = "";
  const char* repeat_help = "";
  const char* item_label = "";
  const char* item_help = "";
  const char* item_repeat_label = "";
  const char* item_repeat_help = "";
};
[[nodiscard]] QuickCastTexts QuickCastTextsFor(bool staff);

// La section de l'établi du staff. Rend vrai quand un réglage a bougé.
//
// `staff` est le droit du staff, qui abaisse le plancher de la cadence d'item.
[[nodiscard]] bool DrawQuickCastSettings(QuickCastLook& look, const SkinArt& art, bool staff);

}  // namespace ro::ui
