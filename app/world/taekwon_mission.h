#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "ro/sim/strike_log.h"
#include "ro/sim/world.h"

namespace moonclient {

// ── LE COMPTE D'UNE MISSION TAEKWON, TENU PAR LE CLIENT ─────────────────────
//
// Le serveur ne dit l'avancement qu'à la demande — la compétence relancée — et
// quand la centaine est atteinte. Entre les deux, il compte sans rien dire : un
// monstre de l'espèce visée, achevé par le joueur ou par ce qui le sert — pet,
// homoncule, mercenaire, élémentaire, monstre invoqué. Le client refait ce
// compte avec ce qu'il voit : le coup qui achève, puis la mort.
//
// ⚠ C'EST UNE ESTIMATION. Un monstre achevé hors de vue, ou dont le dernier coup
// n'est pas parvenu, échappe au compte ; la prochaine annonce du serveur le
// recale.

// Combien de monstres une mission demande.
inline constexpr std::uint32_t kTaekwonMissionGoal = 100;

class TaekwonMission {
 public:
  // L'annonce du serveur : l'espèce visée, son nom, et le compte où il en est.
  void Start(std::uint32_t monster_id, std::string monster_name, std::uint32_t count);

  // Suit les coups et les morts depuis le dernier appel, et écrit dans la
  // conversation chaque monstre compté.
  void Advance(ro::sim::World& world, std::uint32_t now_ms);

  [[nodiscard]] std::optional<std::uint32_t> count() const noexcept {
    return target_.has_value() ? std::optional(target_->count) : std::nullopt;
  }

 private:
  struct Target {
    std::uint32_t monster_id = 0;
    std::string monster_name;
    std::uint32_t count = 0;
  };

  std::optional<Target> target_;
  ro::sim::StrikeCursor strikes_;
  // Pour chaque monstre frappé : le dernier coup porté venait-il de nous ?
  std::map<std::uint32_t, bool> last_blow_ours_;
  // Les monstres déjà vus morts : une mort ne se compte qu'une fois.
  std::set<std::uint32_t> seen_dead_;
};

}  // namespace moonclient
