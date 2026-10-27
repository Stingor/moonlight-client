#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/sim/minigame_trophies.h"
#include "ro/sim/survivors_meta.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"
#include "ro/ui/survivors_album_window.h"
#include "ro/ui/survivors_prepare_window.h"
#include "ro/ui/survivors_records_window.h"
#include "ro/ui/survivors_shop.h"
#include "ro/ui/survivors_summary_window.h"

#include "game/survivors_book_mode.h"
#include "game/ui_stage_survivors.h"
#include "survivors/survivors_book.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// ── LA MÉTA À L'ACCUEIL : ce que le carnet dit, et ce que les fenêtres y font ──
//
// Les lignes des fenêtres de la méta se lisent ici dans le carnet, en nombres
// et en clés : `app/game/survivors_meta_views.cpp` y ajoute ensuite les noms,
// les icônes et les illustrations des tables du jeu. ⚠ LES DEUX SUIVENT LE
// MÊME ORDRE : le catalogue de la méta pour les boutiques, la réserve de
// cartes de la classe pour l'Album, `ShippedSurvivorsFeats` pour les
// exploits, les cartes ramassées de la fin pour l'écran de fin.
//
// 🔴 UNE INTENTION SE JUGE SUR LE CARNET RELU (`SurvivorsSaveFile::Mutate`),
// jamais sur ce que la fenêtre montrait : un achat qu'un autre client a rendu
// impossible entre-temps se refuse sans rien écrire.

// Une ligne au joueur, dans la conversation.
using SurvivorsSay = std::function<void(std::string)>;

// LE CARNET ABÎMÉ COPIÉ DE CÔTÉ se dit au joueur, une fois, par une ligne de
// la conversation qui nomme la copie. Rien sans copie ou sans `say`.
void TellDamagedCopy(const std::optional<std::filesystem::path>& copy, const SurvivorsSay& say);

enum class SurvivorsMetaShop : std::uint8_t { kChurch, kKafra };

// LES INTENTIONS, appliquées à un carnet : vrai quand il a changé. Une ligne
// d'achat est un rang du catalogue de la boutique ; « Tout rembourser » rend
// ce que le lieu a reçu. Un achat que le carnet refuse pose sa raison dans
// `refused`, quand il est donné ; une ligne hors du catalogue n'en a pas.
[[nodiscard]] bool ApplyShopAsk(SurvivorsSave& save, SurvivorsMetaShop shop,
                                const ro::ui::SurvivorsShopAsk& ask,
                                std::optional<ro::sim::MetaRefusal>* refused = nullptr);
// La Cursed Water choisie, ramenée aux rangs que les règles connaissent.
[[nodiscard]] bool ChooseCursedWater(SurvivorsSave& save, const ro::sim::SurvivorsRules& rules,
                                     int rank);

// L'issue d'une partie, telle que les fenêtres la montrent.
[[nodiscard]] ro::ui::SurvivorsEndOutcome EndOutcomeOf(ro::sim::SurvivorsOutcome outcome) noexcept;

// LES LIGNES D'UNE BOUTIQUE, dans l'ordre de son catalogue, sans icône ni nom.
void ShopRowsOf(const SurvivorsSave& save, SurvivorsMetaShop shop,
                std::vector<ro::ui::SurvivorsShopRow>& rows);
[[nodiscard]] ro::ui::SurvivorsShopView ShopViewOf(const SurvivorsSave& save,
                                                   SurvivorsMetaShop shop,
                                                   std::span<const ro::ui::SurvivorsShopRow> rows);

// L'ACCUEIL : le solde, la Cursed Water et son bonus, le résumé du départ
// acheté, et la semaine du défi au moment `now_utc_s`. Le bolt imposé n'y a
// que son Id.
[[nodiscard]] ro::ui::SurvivorsPrepareMeta PrepareMetaOf(const ro::sim::SurvivorsRules& rules,
                                                         const SurvivorsSave& save,
                                                         SurvivorsBoardKind board,
                                                         std::int64_t now_utc_s);

// L'ALBUM : une entrée par carte de la réserve de la classe, un exploit par
// ligne de `ShippedSurvivorsFeats`.
void AlbumCardsOf(const ro::sim::SurvivorsRules& rules, const SurvivorsSave& save,
                  std::vector<ro::ui::SurvivorsAlbumEntry>& cards);
void FeatEntriesOf(const SurvivorsSave& save, std::vector<ro::ui::SurvivorsFeatEntry>& feats);

// LES RECORDS : les parties libres, la semaine en cours, puis la meilleure de
// chaque semaine passée, la plus récente d'abord, et les parties classées
// (Q8), que la vue ne montre que si `ranked_shown` est posé par l'appelant.
// `latest_utc_s` désigne la dernière partie créditée ; ses lignes se
// surlignent. Les lignes suivent l'ordre du tableau : celles d'anciennes
// règles, marquées, sous celles des règles en vigueur, dans leur ordre de
// score, et leur numéro continue celui des lignes qui les précèdent.
struct SurvivorsRecordRowsHeld {
  std::vector<ro::ui::SurvivorsRecordRow> free_runs;
  std::vector<ro::ui::SurvivorsRecordRow> week_runs;
  std::vector<ro::ui::SurvivorsRecordRow> past_weeks;
  std::vector<ro::ui::SurvivorsRecordRow> ranked_runs;
};
[[nodiscard]] ro::ui::SurvivorsRecordsView RecordsViewOf(const ro::sim::SurvivorsRules& rules,
                                                         const SurvivorsSave& save,
                                                         std::int64_t now_utc_s,
                                                         std::optional<std::int64_t> latest_utc_s,
                                                         SurvivorsRecordRowsHeld& held);

// L'ÉCRAN DE FIN, depuis le crédit : Zeny versés et solde du carnet relu
// `save` (un achat fait à l'Église depuis l'écran de fin s'y voit), tableau,
// record, une ligne par carte ramassée (`first_time` pour une entrée neuve
// dans l'Album), et les exploits du carnet, `fresh` pour ceux que la partie a
// atteints.
struct SurvivorsCreditRowsHeld {
  std::vector<ro::ui::SurvivorsDroppedCardRow> dropped;
  std::vector<ro::ui::SurvivorsFeatEntry> feats;
};
[[nodiscard]] ro::ui::SurvivorsCreditRows CreditRowsOf(const SurvivorsCredit& credit,
                                                       const SurvivorsEnding& ending,
                                                       const SurvivorsSave& save,
                                                       SurvivorsCreditRowsHeld& held);
// LES SUCCÈS GAGNÉS (Q13), leur nom seul : l'écran de fin les montre même
// quand le carnet n'a pas pu être crédité.
void TrophyRowsOf(std::span<const ro::sim::MinigameTrophy> trophies,
                  std::vector<ro::ui::SurvivorsTrophyRow>& rows);

// LE CARNET TEL QUE L'ACCUEIL LE MONTRE, et les intentions de ses fenêtres.
// Le carnet se relit à l'ouverture de l'accueil et après chaque crédit ;
// chaque intention le relit encore, l'écrit, et garde ce qu'elle a écrit.
class SurvivorsMetaDesk {
 public:
  explicit SurvivorsMetaDesk(SurvivorsSaveFile file = SurvivorsSaveFile());

  // Relit le carnet. Un fichier qui ne s'ouvre pas garde le carnet montré.
  void Reload(const std::filesystem::path& client_root, const SurvivorsSay& say);
  [[nodiscard]] const SurvivorsSave& save() const noexcept { return save_; }

  // L'ACCUEIL A DEMANDÉ : le tableau, la Cursed Water, un lieu à ouvrir.
  void FollowPrepare(const ro::ui::SurvivorsPrepareAsk& asked, SurvivorsMetaWindows& windows,
                     SurvivorsBookMode& book, const ro::sim::SurvivorsRules& rules,
                     const std::filesystem::path& client_root, const SurvivorsSay& say);
  // LES BOUTIQUES ONT DEMANDÉ : leurs réponses, vidées.
  void FollowShops(SurvivorsMetaWindows& windows, const std::filesystem::path& client_root,
                   const SurvivorsSay& say);
  // L'ÉCRAN DE FIN A DEMANDÉ : « Église » l'ouvre par-dessus lui, et l'Église
  // qui y est ouverte achète comme en ville.
  void FollowSummary(const SurvivorsAsk& asked, SurvivorsMetaWindows& windows,
                     const std::filesystem::path& client_root, const SurvivorsSay& say);

 private:
  void Mutate(const std::filesystem::path& client_root, const SurvivorsSaveDelta& delta,
              const SurvivorsSay& say);
  void FollowShop(SurvivorsMetaShop shop, const ro::ui::SurvivorsShopAsk& asked,
                  const std::filesystem::path& client_root, const SurvivorsSay& say);

  SurvivorsSaveFile file_;
  SurvivorsSave save_;
};

}  // namespace moonclient
