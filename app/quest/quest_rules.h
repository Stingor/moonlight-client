#pragma once

// LES RÈGLES DU JOURNAL DES QUÊTES, sans fenêtre ni réseau : ce qu'une chasse
// écrit, le temps qui reste, ce qu'une recherche trouve, l'ordre des quêtes.
// Tout ce qui se trompe en silence est ici, là où un essai peut le voir.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/sim/quest_log.h"
#include "ro/ui/quest_window.h"

#include "quest/quest_catalog.h"

namespace moonclient {

// Les deux filtres que tout catalogue porte : « tout », et « le reste », où va
// une quête qu'aucune autre catégorie ne réclame.
inline constexpr std::string_view kQuestAllCategory = "all";
inline constexpr std::string_view kQuestOtherCategory = "etc";

// CE QU'UNE CHASSE VISE : le nom du monstre que le serveur donne ; pour une
// chasse générique (sans monstre), ses critères dans la langue courante —
// « Monstres : race Bête, taille Petite, sur prt_fild08 », « Monstres de toute
// sorte » —, ou la description du serveur telle quelle si elle ne se lit pas.
[[nodiscard]] std::string HuntTargetName(const ro::sim::QuestHunt& hunt);

// LES LIGNES QUE LES QUÊTES ÉCRIVENT DANS LA CONVERSATION, sous la case
// « Informations de quête », celles du client d'origine.
//
// Une quête reçue ne se nomme pas : « Vous avez reçu une nouvelle quête. »
[[nodiscard]] std::string QuestReceivedLine();
// « La quête [titre] a été supprimée. », pour un achèvement comme pour un
// abandon. Le titre peut être vide : le client d'origine l'écrit quand même.
[[nodiscard]] std::string QuestDeletedLine(std::string_view title);
// « Quête [titre] : vaincre [cible], progression (4/10). » — une ligne par
// chasse que le serveur redit. VIDE quand la quête n'a pas de titre : le client
// d'origine se tait alors.
[[nodiscard]] std::string QuestHuntLine(std::string_view title, const ro::sim::QuestHunt& hunt);

// « Poring Lv.1~Lv.10 », « Poring Lv.5 », « Poring » : ce que la chasse vise,
// puis la tranche de niveaux que le serveur borne.
[[nodiscard]] std::string HuntLabel(const ro::sim::QuestHunt& hunt);

// LA LIGNE D'UNE CHASSE DANS LA BULLE DE SUIVI, en haut de l'écran :
// « Poring [ 4 / 10 ] », puis « Poring [ Terminé ] » quand le compte est plein.
[[nodiscard]] std::string HuntBalloonLine(const ro::sim::QuestHunt& hunt);

// LE TEMPS QUI RESTE AVANT UNE ÉCHÉANCE.
struct QuestTimeLeft {
  // « 3 j 4 h », « 2 h 13 min », « 12 min », « moins d'une minute »,
  // « expirée » ; vide : sans limite.
  std::string text;
  // Moins d'une heure.
  bool urgent = false;
  bool expired = false;
};
// Moins que cela, une quête presse.
inline constexpr std::int64_t kQuestUrgentSeconds = 3600;
[[nodiscard]] QuestTimeLeft TimeLeftOf(std::uint32_t expires_unix_s, std::int64_t now_unix_s);

// LE TEMPS QUI RESTE À UNE QUÊTE D'ATTENTE : le même compte, mais rien ne
// presse, et l'échéance passée se dit « prête » — `expired` vaut alors « il est
// temps de retourner voir le PNJ ».
[[nodiscard]] QuestTimeLeft WaitLeftOf(std::uint32_t expires_unix_s, std::int64_t now_unix_s);

// « 2026/10/07 18:00 », à l'heure locale ; vide pour zéro.
[[nodiscard]] std::string QuestDateText(std::int64_t unix_s);

// L'AVANCÉE DE 0 À 1 sur l'ensemble des chasses dont le total est connu ;
// négative quand aucune n'en a.
[[nodiscard]] float QuestProgress(const ro::sim::QuestRecord& quest);

// TOUTES LES CHASSES SONT FAITES — et il y en a au moins une : une quête sans
// chasse ne se remplit pas d'elle-même, elle se rend au PNJ.
[[nodiscard]] bool ObjectivesMet(const ro::sim::QuestRecord& quest);

// LE TEXTE LISIBLE d'une ligne balisée : couleurs `^RRGGBB` retirées, liens
// réduits à leur texte. Sert à la recherche, pas à l'affichage.
[[nodiscard]] std::string PlainQuestText(std::string_view marked);

// `needle` se trouve-t-il dans `haystack`, sans tenir compte de la casse
// ASCII ? Un accent reste un accent. Une aiguille vide se trouve partout.
[[nodiscard]] bool QuestTextContains(std::string_view haystack, std::string_view needle);

// LA RECHERCHE : sans tenir compte de la casse ASCII, dans le titre, le résumé,
// la description, les noms des chasses et la carte du PNJ. Une recherche vide
// trouve tout.
[[nodiscard]] bool QuestMatches(const ro::sim::QuestRecord& quest, const QuestInfo* info,
                                std::string_view needle);

// LE FILTRE DE CATÉGORIE : « all » prend tout ; une quête que le catalogue ne
// connaît pas relève de « etc ».
[[nodiscard]] bool QuestInCategory(const QuestInfo* info, const QuestCatalog* catalog,
                                   std::string_view category_key);

// UNE QUÊTE D'ATTENTE (`cooldown` au catalogue) ne fait que courir jusqu'à son
// échéance : inconnue du catalogue, elle ne l'est pas.
[[nodiscard]] bool IsWaitingQuest(const QuestInfo* info);

// L'ONGLET D'UNE QUÊTE DU JOURNAL : « En cours » pour une active, « En attente »
// pour une active d'attente, « Suspendues » pour une suspendue, quelle qu'elle
// soit — le cadenas est un choix du joueur. Ni l'historique ni les
// recommandations ne tiennent de quête du journal.
[[nodiscard]] bool QuestInTab(const ro::sim::QuestRecord& quest, const QuestInfo* info,
                              ro::ui::QuestTab tab);

// La clé de groupe d'une quête : sa catégorie, ou « etc ».
[[nodiscard]] std::string_view QuestGroupKey(const QuestInfo* info, const QuestCatalog* catalog);

// CE QUE LE TRI SAIT D'UNE QUÊTE au-delà du journal.
struct QuestSortFacts {
  // L'instant où le client l'a vue arriver, en secondes Unix ; zéro : jamais
  // noté (elle va alors après les autres en « récentes »).
  std::unordered_map<std::uint32_t, std::int64_t> received_unix_s;
};

// LES QUÊTES DE L'ONGLET, filtrées, cherchées et triées. ⚠ DES POINTEURS dans
// le journal, valables jusqu'à son prochain changement.
struct QuestQuery {
  ro::ui::QuestTab tab = ro::ui::QuestTab::kActive;
  std::string_view category = "all";
  std::string_view search;
  ro::ui::QuestSort sort = ro::ui::QuestSort::kRecent;
};
[[nodiscard]] std::vector<const ro::sim::QuestRecord*> QuestsOfTab(
    std::span<const ro::sim::QuestRecord> quests, const QuestCatalog* catalog,
    const QuestQuery& query, const QuestSortFacts& facts, std::int64_t now_unix_s);

// L'ORDRE DU PANNEAU DE SUIVI : les quêtes suivies et actives — une quête
// d'attente n'y paraît pas, il n'y a rien à y faire —, celles dont le
// PNJ est sur la carte courante d'abord si on le demande, puis l'ordre du
// journal. ⚠ Des pointeurs dans le journal.
[[nodiscard]] std::vector<const ro::sim::QuestRecord*> TrackedQuests(
    std::span<const ro::sim::QuestRecord> quests, std::span<const std::uint32_t> tracked,
    const QuestCatalog* catalog, std::string_view current_map, bool current_map_first);

}  // namespace moonclient
