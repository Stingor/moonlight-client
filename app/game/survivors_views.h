#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/item_icons.h"
#include "ro/render/skill_icons.h"
#include "ro/sim/minigame_trophies.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/survivors_rules.h"
#include "ro/ui/survivors_prepare_window.h"
#include "ro/vfs/job_names.h"
#include "ro/vfs/monster_list.h"

#include "game/survivors_book_mode.h"
#include "game/survivors_codex_desk.h"
#include "game/survivors_effect_desk.h"
#include "game/survivors_meta_desk.h"
#include "game/survivors_ranking_desk.h"
#include "game/survivors_ranking_mode.h"
#include "game/trade_items.h"
#include "game/ui_stage_survivors.h"
#include "survivors/arena_card_cues.h"
#include "survivors/arena_measures.h"
#include "survivors/arena_session.h"
#include "survivors/real_world_watch.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"
#include "world/monster_sprite.h"

namespace moonclient {

// LES VUES DU MODE SURVIVORS : ce que les fenêtres de `ui` montrent, composé
// depuis l'étage et la partie, les noms et les icônes résolus ici. `ui` ne
// voit jamais un type de `sim` ni un chemin de fichier.

// LES TABLES DU JEU où se lisent noms et icônes. Toutes facultatives : sans
// elles, un identifiant s'écrit « #id » et la case reste vide ; sans
// garde-robe, la fiche du Codex n'a pas de sprite.
struct SurvivorsTables {
  const ro::sim::SkillTree* skill_tree = nullptr;
  ro::render::SkillIcons* skill_icons = nullptr;
  const TradeItems* items = nullptr;
  ro::render::ItemIcons* item_icons = nullptr;
  const ro::vfs::MonsterList* monsters = nullptr;
  const ro::vfs::JobNames* jobs = nullptr;
  ro::render::ActorWardrobe* wardrobe = nullptr;
};

// CE QUE L'IMAGE AJOUTE À L'ÉTAGE pour ses fenêtres.
struct SurvivorsViewFacts {
  // L'arène, pour placer la jauge du héros et les flèches de bord ; nulle
  // pour un bilan relu sans partie.
  const ArenaSession* arena = nullptr;
  float width = 0.0F;
  float height = 0.0F;
  // La rangée des chiffres de l'image, par sa place : voir
  // `SurvivorsOfferView::pressed_card_key`.
  int pressed_card_key = 0;
  // L'alerte qui a mis en pause, quand c'est le monde réel.
  std::optional<RealWorldAlert> banner;
  std::uint32_t unread_whispers = 0;
  SurvivorsOptions options;
  const ArenaMeasures* measures = nullptr;
  // Les voix du mode occupées, pour les comptes de la pause.
  std::size_t busy_voices = 0;
  // CE QUE LA PARTIE A RAPPORTÉ, pour l'écran de fin : le crédit du carnet
  // (nul avant lui, ou pour un bilan relu d'une autre session), le carnet
  // relu après lui et les succès gagnés (Q13).
  const SurvivorsCredit* credit = nullptr;
  const SurvivorsSave* save = nullptr;
  std::span<const ro::sim::MinigameTrophy> trophies;
  // Les fenêtres de la méta : l'Église s'ouvre par-dessus l'écran de fin.
  SurvivorsMetaWindows* meta = nullptr;
  // LE CLASSEMENT : la marque du HUD, la section de l'écran de fin et le
  // tableau mondial par-dessus lui. Nul, rien du classement ne paraît.
  const SurvivorsRankingMode* ranking = nullptr;
  // L'heure de l'image : le sprite de la fiche du Codex s'anime sur elle.
  std::uint32_t now_ms = 0;
};

// LES ICÔNES ET LES ILLUSTRATIONS, lues dans les tables ; vides sans elles.
[[nodiscard]] ImTextureID SurvivorsSkillIcon(std::uint16_t skill_id, const SurvivorsTables& tables);
[[nodiscard]] ImTextureID SurvivorsItemIcon(std::uint16_t item_id, const SurvivorsTables& tables);
[[nodiscard]] ro::ui::SkinImage SurvivorsCardIllustration(std::uint16_t card_item_id,
                                                          const SurvivorsTables& tables);

// Ce qu'un lien du Codex désigne : un sort, un objet ou une classe RO.
enum class CodexLinkKind : std::uint8_t { kSkill, kItem, kFoe };

class SurvivorsViews {
 public:
  // LA PRÉPARATION, dans la fenêtre de la salle : les bolts de départ de la
  // classe, celui qui est choisi, la raison du refus d'entrée (nulle quand la
  // partie peut commencer) et les lettres du clavier du joueur.
  void ComposePrepare(ro::ui::SurvivorsPrepareView& view, const ro::sim::SurvivorsRules& rules,
                      std::uint16_t chosen_skill_id, const char* refusal,
                      const std::array<std::string, 4>& key_labels, const SurvivorsTables& tables);

  // L'ACCUEIL ET SES FENÊTRES DE MÉTA (Q1), depuis le carnet relu : la méta de
  // la préparation, puis l'Église, la Kafra, le Codex et les records ouverts ;
  // avec le classement (`ranking`, nul sans lui), ce que l'accueil en montre
  // service ouvert et le tableau mondial. Voir `survivors_meta_views.cpp`.
  void ComposeMeta(ro::ui::SurvivorsPrepareView& prepare, SurvivorsMetaWindows& windows,
                   const SurvivorsMetaDesk& desk, const SurvivorsBookMode& book,
                   const ro::sim::SurvivorsRules& rules, const SurvivorsTables& tables,
                   const SurvivorsRankingMode* ranking, std::uint32_t now_ms);

  // L'IMAGE D'UNE PARTIE : le HUD et la fenêtre de l'écran en cours.
  [[nodiscard]] SurvivorsFrameView Compose(const SurvivorsStage& stage,
                                           const ro::sim::SurvivorsRules& rules,
                                           const SurvivorsViewFacts& facts,
                                           const SurvivorsTables& tables);

  // LES CARTES ET LES OBJETS RAMASSÉS des nouvelles que l'image vient de
  // prendre, une fois chacun : la vignette d'une carte paraît au HUD (Q6),
  // l'avis d'un objet à effet au-dessus du héros.
  void NoticeFinds(const ro::sim::SurvivorsNews& news, std::uint64_t presentation_ms) {
    card_vignettes_.Notice(news.cards_found, presentation_ms);
    pickup_notices_.Notice(news.pickups_taken, presentation_ms);
  }

  // Une fenêtre qui s'ouvre repart de son état neuf.
  void ResetOffer() noexcept { offer_state_ = {}; }
  void ResetPause() noexcept { pause_state_ = {}; }

 private:
  [[nodiscard]] const char* SkillName(std::uint16_t skill_id, const SurvivorsTables& tables);
  [[nodiscard]] const char* CardName(std::uint16_t card_item_id, const SurvivorsTables& tables);
  [[nodiscard]] const char* FoeName(std::uint16_t foe_id, const SurvivorsTables& tables);
  [[nodiscard]] const char* JobName(std::uint16_t job_id, const SurvivorsTables& tables);
  void ComposeHud(const ro::sim::SurvivorsField& field, const ro::sim::SurvivorsRules& rules,
                  std::uint64_t clock_ms, const SurvivorsViewFacts& facts,
                  const SurvivorsTables& tables);
  void ComposeOffer(const ro::sim::SurvivorsField& field, const ro::sim::SurvivorsRules& rules,
                    bool accepts_input, int pressed_card_key, const SurvivorsTables& tables);
  void ComposeChest(const SurvivorsStage& stage, const SurvivorsTables& tables);
  void ComposePause(const SurvivorsStage& stage, const ro::sim::SurvivorsField* field,
                    const SurvivorsViewFacts& facts);
  void ComposeSummary(const SurvivorsStage& stage, const ro::sim::SurvivorsRules& rules,
                      const SurvivorsTables& tables);
  // LA MÉTA DANS LA PARTIE (`survivors_meta_views.cpp`) : les charges, les
  // vies et la marque du tableau au HUD ; le crédit à l'écran de fin ; une
  // boutique et ses lignes.
  void ComposeHudMeta(const SurvivorsStage& stage, const ro::sim::SurvivorsField& field,
                      const SurvivorsTables& tables);
  // LES EFFETS (`survivors_effect_views.cpp`) : au HUD, ceux qui sont en
  // cours et les avis des objets ramassés ; dans la pause, les permanents.
  void ComposeHudEffects(const ro::sim::SurvivorsField& field, const ro::sim::SurvivorsRules& rules,
                         std::uint64_t presentation_ms, const SurvivorsTables& tables);
  void ComposePauseEffects(const ro::sim::SurvivorsField& field,
                           const ro::sim::SurvivorsRules& rules, const SurvivorsTables& tables);
  void ComposeCredit(const SurvivorsStage& stage, const ro::sim::SurvivorsRules& rules,
                     const SurvivorsViewFacts& facts, const SurvivorsTables& tables);
  void ComposeShop(SurvivorsMetaShop shop, SurvivorsMetaWindows& windows, const SurvivorsSave& save,
                   const SurvivorsTables& tables);
  void ComposeAlbum(SurvivorsMetaWindows& windows, const SurvivorsSave& save,
                    const ro::sim::SurvivorsRules& rules, const SurvivorsTables& tables);
  // LE CODEX OUVERT (`survivors_codex_views.cpp`), à l'accueil ou sur la
  // pause : les fiches tirées des règles, du carnet et du relevé de la partie
  // en cours (`live`, nul hors partie), puis leurs noms, leurs icônes, le
  // sprite de l'entrée choisie, et l'Album de l'onglet Cartes.
  void ComposeCodex(SurvivorsMetaWindows& windows, const SurvivorsSave& save,
                    const SurvivorsCodexSeen* live, const ro::sim::SurvivorsRules& rules,
                    const SurvivorsTables& tables, std::uint32_t now_ms);
  void NameCodexEntry(ro::ui::SurvivorsCodexEntry& entry, const SurvivorsTables& tables);
  void NameCodexLink(ro::ui::SurvivorsCodexLink& link, CodexLinkKind kind,
                     const SurvivorsTables& tables);
  void ComposeCodexSprite(const ro::ui::SurvivorsCodexState& state, const SurvivorsTables& tables,
                          std::uint32_t now_ms);
  // LE CLASSEMENT DANS LA PARTIE : la marque « Classée » du HUD, la section de
  // l'écran de fin et le tableau mondial ouvert par-dessus lui.
  void ComposeRanking(const SurvivorsStage& stage, const SurvivorsViewFacts& facts,
                      SurvivorsFrameView& view);
  // Le skill que débloque chaque exploit : son icône et son nom.
  void NameFeats(std::span<ro::ui::SurvivorsFeatEntry> feats, const ro::sim::SurvivorsRules& rules,
                 const SurvivorsTables& tables);

  // LES NOMS, résolus une fois par identifiant : les vues en gardent l'adresse.
  std::map<std::uint16_t, std::string> skill_names_;
  std::map<std::uint16_t, std::string> card_names_;
  std::map<std::uint16_t, std::string> foe_names_;
  std::map<std::uint16_t, std::string> job_names_;
  // LES LIGNES DES VUES, réutilisées d'une image à l'autre.
  std::vector<ro::ui::SurvivorsBoltChoice> bolts_;
  std::vector<ro::ui::SurvivorsHudSlot> skill_slots_;
  std::vector<ro::ui::SurvivorsHudSlot> card_slots_;
  std::vector<ro::ui::SurvivorsEdgeTarget> edge_targets_;
  std::vector<ro::ui::SurvivorsOfferCard> offer_cards_;
  std::vector<ro::ui::SurvivorsChestGainRow> chest_gains_;
  std::vector<ImTextureID> roulette_icons_;
  std::vector<ro::ui::SurvivorsMeasureRow> measure_rows_;
  std::vector<ro::ui::SurvivorsCountRow> count_rows_;
  std::vector<ro::ui::SurvivorsFoeRow> foe_rows_;
  std::vector<ro::ui::SurvivorsSkillDamageRow> skill_rows_;
  std::vector<ro::ui::SurvivorsCardRow> card_rows_;
  CardVignettes card_vignettes_;
  // LES EFFETS : en cours et avis au HUD, permanents dans la pause.
  PickupNotices pickup_notices_;
  std::vector<SurvivorsTimedEffect> timed_effects_;
  std::vector<PickupNoticeShown> notices_shown_;
  std::vector<SurvivorsPermanentEffect> permanent_effects_;
  std::vector<ro::ui::SurvivorsHudEffect> hud_effects_;
  std::vector<ro::ui::SurvivorsHudNotice> hud_notices_;
  std::vector<ro::ui::SurvivorsPauseEffectRow> pause_effects_;
  ro::ui::SurvivorsHudView hud_;
  ro::ui::SurvivorsOfferView offer_;
  ro::ui::SurvivorsOfferState offer_state_;
  ro::ui::SurvivorsChestView chest_;
  ro::ui::SurvivorsPauseView pause_;
  ro::ui::SurvivorsPauseState pause_state_;
  ro::ui::SurvivorsSummaryView summary_;
  // LES LIGNES DE LA MÉTA.
  std::vector<ro::ui::SurvivorsHudCharge> hud_charges_;
  std::vector<ro::ui::SurvivorsShopRow> church_rows_;
  std::vector<ro::ui::SurvivorsShopRow> kafra_rows_;
  std::vector<ro::ui::SurvivorsAlbumEntry> album_cards_;
  std::vector<ro::ui::SurvivorsFeatEntry> album_feats_;
  SurvivorsRecordRowsHeld record_rows_;
  SurvivorsCreditRowsHeld credit_rows_;
  std::vector<ro::ui::SurvivorsTrophyRow> trophy_rows_;
  SurvivorsRankingRowsHeld ranking_rows_;
  // LE CODEX : ses lignes, et le sprite de l'entrée choisie, sur la
  // garde-robe qui l'a vu naître.
  SurvivorsCodexRowsHeld codex_rows_;
  std::optional<MonsterSprite> codex_sprite_;
  const ro::render::ActorWardrobe* codex_sprite_wardrobe_ = nullptr;
};

}  // namespace moonclient
