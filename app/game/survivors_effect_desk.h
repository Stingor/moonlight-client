#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/ui/survivors_effects.h"

// LES EFFETS D'UNE PARTIE, lus dans la simulation sans GPU ni table du jeu :
// ce que le HUD montre en cours, l'avis d'un objet ramassé, et ce que la
// pause dit des effets permanents. Les chiffres viennent des règles et du
// départ de la partie ; `survivors_effect_views.cpp` y ajoute les noms et les
// icônes.
namespace moonclient {

// UN EFFET LIMITÉ DANS LE TEMPS, en cours : l'objet qui l'a donné, ce qui en
// reste et sa durée pleine. Le reste suit la simulation : un second ramassage
// le reporte au plus tard des deux termes, soit la durée pleine depuis ce
// ramassage.
struct SurvivorsTimedEffect {
  ro::sim::PickupKind kind = ro::sim::PickupKind::kManual;
  std::uint16_t item_id = 0;
  std::uint32_t remaining_ms = 0;
  std::uint32_t full_ms = 0;
};

// Les effets en cours, dans l'ordre des objets des règles, rangés dans `out`
// (vidé). Un objet à durée sans terme dans le héros n'y paraît pas.
void TimedEffectsOf(const ro::sim::SurvivorsField& field, const ro::sim::SurvivorsRules& rules,
                    std::vector<SurvivorsTimedEffect>& out);

// LE CHIFFRE D'UNE STATISTIQUE, dans la sorte que `ui` sait écrire.
[[nodiscard]] ro::ui::SurvivorsEffectFigure FigureOfStat(ro::sim::SurvivorsStat stat) noexcept;

// L'EFFET D'UN OBJET AU SOL : sa sorte et sa valeur ; rien pour un objet sans
// effet chiffrable (le coffre, la carte).
struct SurvivorsPickupFigure {
  ro::ui::SurvivorsEffectFigure figure = ro::ui::SurvivorsEffectFigure::kHeal;
  std::int32_t value = 0;
};
[[nodiscard]] std::optional<SurvivorsPickupFigure> PickupFigureOf(const ro::sim::PickupRule& rule);

// L'objet ramassé fait-il un avis ? Tous ceux qui ont un effet, sauf le Gold,
// que le compteur des Zeny dit déjà.
[[nodiscard]] bool NoticedPickup(ro::sim::PickupKind kind) noexcept;

// L'AVIS D'UN OBJET RAMASSÉ dure `kPickupNoticeMs` de l'horloge de
// présentation, qui se fige avec la partie. Au plus `kMostPickupNotices` à la
// fois : au-delà, le plus ancien laisse sa place.
inline constexpr std::uint64_t kPickupNoticeMs = 2000;
inline constexpr std::size_t kMostPickupNotices = 4;

struct PickupNoticeShown {
  ro::sim::PickupKind kind = ro::sim::PickupKind::kManual;
  std::uint16_t item_id = 0;
  // De 0 à 1 le long de sa vie.
  float fraction = 0.0F;
};

// LES AVIS EN COURS, chacun sa vie depuis son ramassage. Une horloge qui
// recule est celle d'une partie neuve : ils s'effacent.
class PickupNotices {
 public:
  // Les objets ramassés des nouvelles que l'appelant vient de prendre, une
  // fois chacun.
  void Notice(std::span<const ro::sim::SurvivorsPickupNews> taken, std::uint64_t now_ms);
  // Les avis vivants à `now_ms`, du plus ancien au plus récent, rangés dans
  // `out` (vidé) ; les avis finis s'en vont.
  void ShownAt(std::uint64_t now_ms, std::vector<PickupNoticeShown>& out);
  [[nodiscard]] std::size_t living() const noexcept { return living_.size(); }

 private:
  struct Living {
    ro::sim::PickupKind kind = ro::sim::PickupKind::kManual;
    std::uint16_t item_id = 0;
    std::uint64_t since_ms = 0;
  };
  void Follow(std::uint64_t now_ms) noexcept;

  std::vector<Living> living_;
  std::uint64_t last_now_ms_ = 0;
};

// UN EFFET PERMANENT DE LA PARTIE : ce qui le donne (une carte portée, une
// Bénédiction du départ, une charge Kafra, la vie de Resurrection), son
// niveau (0 : sans niveau) et son effet. Les vies et les charges disent ce
// qui RESTE ; les Bénédictions, ce que le départ de la partie a versé.
enum class SurvivorsPermanentSource : std::uint8_t { kCard, kBlessing, kKafra, kLife };
struct SurvivorsPermanentEffect {
  SurvivorsPermanentSource source = SurvivorsPermanentSource::kCard;
  // L'objet RO d'une carte ou d'un service Kafra ; le rang de la Bénédiction
  // dans le catalogue de la méta ; 0 pour la vie.
  std::uint16_t id = 0;
  int level = 0;
  ro::ui::SurvivorsEffectFigure figure = ro::ui::SurvivorsEffectFigure::kDamagePct;
  std::int32_t value = 0;
};

// Les effets permanents : les cartes portées, puis les Bénédictions dont le
// départ porte un bonus, puis les charges Kafra que le départ a données, puis
// les vies quand le départ en a données. Rangés dans `out` (vidé).
void PermanentEffectsOf(const ro::sim::SurvivorsField& field, const ro::sim::SurvivorsRules& rules,
                        std::vector<SurvivorsPermanentEffect>& out);

}  // namespace moonclient
