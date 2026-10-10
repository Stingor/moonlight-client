#pragma once

// LES RÈGLES DE LA FENÊTRE DES SUCCÈS, à part de son étage : quels succès un
// onglet montre et dans quel ordre, ce qu'il compte, comment une date, un
// objectif et une ligne du panneau de suivi s'écrivent. Ce sont elles qu'un
// banc vérifie, sans fenêtre ni serveur.

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/achievement_book.h"
#include "ro/ui/achievement_window.h"
#include "ro/vfs/achievement_catalog.h"

namespace moonclient {

// Le succès appartient-il à cet onglet ? Le résumé et « Tous » les prennent
// tous ; l'onglet d'une catégorie, tous ses sous-onglets ; un sous-onglet, les
// siens.
[[nodiscard]] bool InAchievementTab(const ro::vfs::AchievementInfo& info, int tab, int subtab);

// « Faits / tous » d'un onglet.
[[nodiscard]] ro::ui::AchievementTally TallyOfTab(const ro::vfs::AchievementCatalog& catalog,
                                                  const ro::sim::AchievementBook& book, int tab,
                                                  int subtab);

// CE QUE LA LISTE DOIT MONTRER : l'onglet, le filtre, l'ordre, et la recherche.
struct AchievementQuery {
  int tab = ro::ui::kAchievementSummaryTab;
  int subtab = ro::ui::kAchievementWholeTab;
  ro::ui::AchievementFilter filter = ro::ui::AchievementFilter::kAll;
  ro::ui::AchievementSort sort = ro::ui::AchievementSort::kGame;
  // Non vide, elle remplace l'onglet : on cherche dans tout le catalogue.
  std::string_view search;
};

// Le succès répond-il à la recherche ? Le nom, le résumé ou un objectif la
// contient, sans égard à la casse. Une recherche vide répond à tout.
[[nodiscard]] bool MatchesAchievementSearch(const ro::vfs::AchievementInfo& info,
                                            std::string_view search);

// LA PART FAITE D'UN SUCCÈS, de 0 à 1 : un achevé vaut 1 ; sinon la moyenne de
// ses objectifs — un objectif compté vaut sa part du but, une puce vaut 1 dès
// qu'elle a bougé.
[[nodiscard]] double AchievementProgressShare(const ro::vfs::AchievementInfo& info,
                                              const ro::sim::AchievementProgress* progress);

// LES SUCCÈS QUE LA LISTE MONTRE. L'ordre du client d'abord : les achevés, du
// plus ANCIENNEMENT obtenu au plus récent, puis les autres par numéro ; un tri
// à nous le reprend ensuite, sans casser les égalités.
//
// Les filtres : « à réclamer » ne garde des achevés que ceux dont la récompense
// attend, et laisse tous les inachevés ; « en cours » ne garde que les succès
// commencés et pas achevés ; « suivis », que les succès épinglés.
[[nodiscard]] std::vector<const ro::vfs::AchievementInfo*> RowsOfTab(
    const ro::vfs::AchievementCatalog& catalog, const ro::sim::AchievementBook& book,
    const AchievementQuery& query);
[[nodiscard]] std::vector<const ro::vfs::AchievementInfo*> RowsOfTab(
    const ro::vfs::AchievementCatalog& catalog, const ro::sim::AchievementBook& book, int tab,
    int subtab, ro::ui::AchievementFilter filter);

// Les succès dont la récompense attend qu'on la réclame, par numéro croissant.
[[nodiscard]] std::vector<std::uint32_t> ClaimableAchievements(
    const ro::vfs::AchievementCatalog& catalog, const ro::sim::AchievementBook& book);

// Les deux derniers succès obtenus, le plus récent d'abord. Moins de deux s'il
// n'y en a pas assez ; le catalogue doit les connaître.
[[nodiscard]] std::vector<const ro::vfs::AchievementInfo*> MostRecentlyCompleted(
    const ro::vfs::AchievementCatalog& catalog, const ro::sim::AchievementBook& book);

// « 2020. 3.24 » : l'année, puis le mois et le jour sur deux colonnes complétées
// d'ESPACES, à l'heure locale. Vide pour un instant nul.
[[nodiscard]] std::string AchievementDateText(std::uint32_t unix_s);

// L'objectif atteint ? Compté : le compteur a touché le but. À puces : il a bougé.
[[nodiscard]] bool ObjectiveReached(const ro::vfs::AchievementInfo& info, std::size_t index,
                                    const ro::sim::AchievementProgress* progress);

[[nodiscard]] ro::ui::AchievementObjectiveState ObjectiveStateOf(
    const ro::vfs::AchievementInfo& info, std::size_t index,
    const ro::sim::AchievementProgress* progress);

// « libellé ( fait / but ) » pour un succès qui se compte ; le libellé seul sinon.
[[nodiscard]] std::string ObjectiveText(const ro::vfs::AchievementInfo& info, std::size_t index,
                                        const ro::sim::AchievementProgress* progress);

// LA LIGNE D'UN OBJECTIF DANS LE PANNEAU DE SUIVI : « - libellé », ou
// « - libellé (3/10) » pour un succès qui se compte, le libellé coupé de « ... »
// au-delà de quarante octets en tout.
//
// ⚠ LA COUPE TOMBE SUR UN CARACTÈRE ENTIER : un octet de tête seul ferait un
// texte invalide.
[[nodiscard]] std::string TrackerLineText(const ro::vfs::AchievementInfo& info, std::size_t index,
                                          const ro::sim::AchievementProgress* progress);

// Le compteur d'un objectif, zéro pour un succès pas commencé.
[[nodiscard]] std::uint32_t ObjectiveCount(const ro::sim::AchievementProgress* progress,
                                           std::size_t index);

}  // namespace moonclient
