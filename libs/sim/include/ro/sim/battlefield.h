#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace ro::sim {

// ── LE CHAMP DE BATAILLE DE LA CARTE ────────────────────────────────────────
//
// Les camps des acteurs, le score et les positions des coéquipiers, tels que
// le serveur les dit sur une carte de champ de bataille. DANS LE MONDE, et
// vidé à l'entrée sur une AUTRE carte : tout ce qu'il porte appartient à la
// carte quittée. Un retour sur la même carte le garde.
//
// 🔴 UN CAMP EST UN ENTIER QUELCONQUE, l'identifiant de l'équipe côté serveur,
// et non 1 ou 2. L'équipe lion est créée avant l'équipe aigle et reçoit donc le
// plus petit des deux : c'est la seule règle qui dise de quel côté on est, et
// elle ne répond qu'une fois un camp adverse vu.

// Aucun camp connu.
inline constexpr std::uint16_t kNoBattleCamp = 0;
// Un score que le serveur n'a pas encore dit sur cette carte.
inline constexpr std::int32_t kBattleScoreUnknown = -1;
// LA COULEUR DE LA PAROLE DU CHAMP, en 0xRRGGBB.
inline constexpr std::uint32_t kBattlefieldChatRgb = 0xFF009B;

enum class BattleSide : std::uint8_t { kUnknown, kLion, kEagle };

struct BattleMateCell {
  std::int16_t x = 0;
  std::int16_t y = 0;
};

// UN COÉQUIPIER SUR LA MÊME CARTE, nous exceptés. Le nom est en UTF-8.
struct BattleMate {
  std::uint32_t account_id = 0;
  std::string name;
  std::uint16_t job = 0;
  BattleMateCell cell;
};

struct BattleScore {
  std::int32_t lion = kBattleScoreUnknown;
  std::int32_t eagle = kBattleScoreUnknown;

  // Rien ne se montre tant que l'un des deux est inconnu.
  [[nodiscard]] bool shown() const noexcept { return lion >= 0 && eagle >= 0; }
};

class Battlefield {
 public:
  // Le camp d'un acteur. `own_id` est notre personnage : son camp devient le
  // nôtre. ⚠ QUAND NOTRE CAMP PASSE À UN AUTRE CAMP NON NUL, les camps des
  // autres s'oublient : ils datent de la manche précédente. Le score et les
  // coéquipiers restent.
  void TellCamp(std::uint32_t actor_id, std::uint16_t camp, std::uint32_t own_id);
  // `kNoBattleCamp` si le serveur ne l'a pas dit.
  [[nodiscard]] std::uint16_t CampOf(std::uint32_t actor_id) const;
  [[nodiscard]] std::uint16_t own_camp() const noexcept { return own_camp_; }
  // Les deux camps connus, et différents. Un camp inconnu n'est ni ennemi ni
  // allié.
  [[nodiscard]] bool IsEnemy(std::uint32_t actor_id) const;
  [[nodiscard]] bool IsAlly(std::uint32_t actor_id) const;
  // Lion si notre camp est plus petit que tout autre camp vu, aigle s'il est
  // plus grand ; inconnu sans notre camp ou sans camp adverse vu.
  [[nodiscard]] BattleSide own_side() const;

  void TellScore(std::uint16_t lion, std::uint16_t eagle);
  [[nodiscard]] const BattleScore& score() const noexcept { return score_; }

  // Pose ou déplace un coéquipier ; x = y = −1 le retire.
  void TellMate(std::uint32_t account_id, std::string name, std::uint16_t job, std::int16_t x,
                std::int16_t y);
  [[nodiscard]] const std::map<std::uint32_t, BattleMate>& mates() const noexcept { return mates_; }

  // Tout s'oublie : camps, score et coéquipiers.
  void Clear();

 private:
  std::map<std::uint32_t, std::uint16_t> camps_;
  std::uint16_t own_camp_ = kNoBattleCamp;
  // Le dernier camp non nul qu'on nous a donné : un camp nul entre deux
  // manches ne fait pas oublier de quelle manche viennent les autres.
  std::uint16_t last_own_camp_ = kNoBattleCamp;
  BattleScore score_;
  std::map<std::uint32_t, BattleMate> mates_;
};

}  // namespace ro::sim
