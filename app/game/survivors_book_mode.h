#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <utility>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"

#include "survivors/survivors_book.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// LE CARNET DANS LE MODE : le départ de chaque partie se lit dans le carnet
// relu, et chaque partie le crédite UNE FOIS — à son entrée dans le bilan, ou
// quand elle se fige (liaison perdue, partie qui se ferme), avec sa fin
// interrompue. Rien ne se garde entre deux appels que le crédit de la partie
// en cours, pour l'écran de fin.
//
// ⚠ UN CRÉDIT QUE LE DISQUE REFUSE (fichier tenu par un autre programme) SE
// DIT AU JOURNAL DU CLIENT ET SE PERD : la partie ne se recrédite pas.
class SurvivorsBookMode {
 public:
  // Les secondes UTC du moment ; vide, l'horloge du système.
  using UtcClock = std::function<std::int64_t()>;
  explicit SurvivorsBookMode(UtcClock now_utc = {}, SurvivorsSaveFile file = SurvivorsSaveFile());

  // Le tableau de la prochaine partie, que l'onglet de l'accueil choisit ;
  // « Rejouer » le garde.
  void Choose(SurvivorsBoardKind kind) noexcept { kind_ = kind; }
  [[nodiscard]] SurvivorsBoardKind chosen() const noexcept { return kind_; }
  // L'ARÈNE DE LA PARTIE LIBRE, que l'accueil choisit ; « Rejouer » la garde.
  // Un Id que `rules` ne connaît pas ne se retient pas. 0 : l'arène par défaut
  // (`FreeArenaOf`). Le défi et la partie classée ne la lisent pas.
  void ChooseArena(const ro::sim::SurvivorsRules& rules, std::uint16_t arena_id) noexcept;
  [[nodiscard]] std::uint16_t free_arena_id() const noexcept { return free_arena_id_; }
  // Les secondes UTC du moment, à l'horloge du mode.
  [[nodiscard]] std::int64_t now_utc() const { return now_utc_(); }

  // LE DÉPART DE LA PROCHAINE PARTIE et son tableau : la partie libre part du
  // carnet relu, sur `seed` et `first_skill_id` ; le défi part de la semaine
  // UTC du moment, et les ignore ; la partie classée part neutre, sur `seed`
  // et `first_skill_id`, jusqu'à ce que la graine du serveur la remplace.
  struct Begun {
    ro::sim::SurvivorsStart start;
    SurvivorsRunBoard board;
  };
  [[nodiscard]] Begun StartOf(const ro::sim::SurvivorsRules& rules,
                              const std::filesystem::path& client_root, std::uint64_t seed,
                              std::uint16_t first_skill_id) {
    return StartAs(kind_, rules, client_root, seed, first_skill_id);
  }
  // Le départ d'un tableau donné, quel que soit le choix de l'accueil : une
  // partie classée sans jeton repart libre, sur l'arène `free_arena_id` quand
  // elle est donnée — celle qu'elle demandait —, sinon sur celle de l'accueil.
  [[nodiscard]] Begun StartAs(SurvivorsBoardKind kind, const ro::sim::SurvivorsRules& rules,
                              const std::filesystem::path& client_root, std::uint64_t seed,
                              std::uint16_t first_skill_id,
                              std::optional<std::uint16_t> free_arena_id = std::nullopt);

  // Une partie neuve commence : elle sera créditée à son tour.
  void Restart() noexcept;
  // L'IMAGE QUI VIENT DE S'AVANCER : une partie entrée dans le bilan crédite
  // le carnet.
  void FollowEntered(const SurvivorsStage& stage, const ro::sim::SurvivorsRules& rules,
                     const std::filesystem::path& client_root);
  // CRÉDITE LA PARTIE de l'étage, si elle est finie et pas encore créditée :
  // l'appel qui suit `FreezeAndSave`.
  void Credit(const SurvivorsStage& stage, const ro::sim::SurvivorsRules& rules,
              const std::filesystem::path& client_root);
  // Ce que la partie en cours a ajouté au carnet ; rien avant son crédit.
  [[nodiscard]] const std::optional<SurvivorsCredit>& credit() const noexcept { return credit_; }
  [[nodiscard]] bool credited() const noexcept { return credited_; }
  // LA COPIE D'UN CARNET ABÎMÉ que le départ ou le crédit vient de faire, à
  // dire au joueur une fois ; rien ensuite.
  [[nodiscard]] std::optional<std::filesystem::path> TakeDamagedCopy() noexcept {
    return std::exchange(damaged_copy_, std::nullopt);
  }

 private:
  UtcClock now_utc_;
  SurvivorsSaveFile file_;
  SurvivorsBoardKind kind_ = SurvivorsBoardKind::kFree;
  std::uint16_t free_arena_id_ = 0;
  bool credited_ = false;
  std::optional<SurvivorsCredit> credit_;
  std::optional<std::filesystem::path> damaged_copy_;
};

}  // namespace moonclient
