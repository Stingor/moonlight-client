// Le clic sur un nom. Voir l'en-tête pour la règle entière.

#include "world/name_click.h"

#include <algorithm>
#include <array>

namespace moonclient {
namespace {

// Les compétences qui se lancent sur un joueur nommé sans MAJ.
constexpr auto kWithoutShift = std::to_array<std::uint16_t>({
    4,    28,   29,   34,   35,   53,   54,   66,   67,   68,   71,   72,   73,   108,  138,  139,
    231,  234,  235,  236,  237,  255,  256,  262,  280,  281,  282,  283,  361,  445,  446,  447,
    448,  449,  450,  451,  452,  453,  454,  455,  456,  457,  458,  460,  461,  462,  463,  464,
    465,  479,  494,  564,  572,  573,  574,  575,  577,  593,  621,  622,  626,  636,  637,  641,
    642,  643,  645,  646,  688,  689,  690,  691,  694,  729,  1015, 2001, 2012, 2026, 2041, 2042,
    2043, 2050, 2051, 2052, 2053, 2275, 2287, 2322, 2341, 2345, 2347, 2348, 2383, 2421, 2425, 2451,
    2480, 2482, 2515, 2537, 2596, 2597, 2598, 2599, 2608, 2612, 3025, 3026, 3027, 3031, 3032, 3033,
    3034, 5035, 5038, 5039, 5040, 5041, 5064, 5268, 5269, 5271, 5272, 5275, 5280, 5281, 5282, 5298,
    5299, 5339, 5366, 5368, 8021, 8226, 8227, 8228, 8229, 8230, 8231, 8238, 8239, 8240,
});
// Et celles que le jeu tient hors de cette table, chacune dans sa propre classe
// d'action : les talismans de protection, du guerrier, du magicien et des cinq
// éléments, l'âme du ciel et de la terre, la bénédiction des créatures
// mystiques.
constexpr auto kWithoutShiftActions =
    std::to_array<std::uint16_t>({5418, 5419, 5420, 5423, 5432, 5448});

// Celles qui s'y lancent sous MAJ ou `/noshift`.
constexpr auto kUnderShift = std::to_array<std::uint16_t>({
    28,   29,   30,   34,   35,   53,   54,   66,   67,   68,   71,   72,   73,   77,   78,
    138,  139,  231,  234,  235,  236,  237,  255,  280,  281,  282,  283,  286,  289,  374,
    402,  463,  464,  465,  466,  467,  468,  469,  470,  471,  472,  479,  1015, 2001, 2020,
    2026, 2027, 2050, 2051, 2052, 2053, 2341, 2344, 2345, 2346, 2347, 2348, 2451, 2515, 8221,
});

constexpr std::uint16_t kMapTypePenaltyFreePk = 5;
constexpr std::uint16_t kMapTypeFreePk = 6;
constexpr std::uint16_t kMapTypeBattlefield = 19;

template <std::size_t kCount>
[[nodiscard]] bool Holds(const std::array<std::uint16_t, kCount>& list, std::uint16_t skill_id) {
  return std::find(list.begin(), list.end(), skill_id) != list.end();
}

}  // namespace

bool SkillTargetsPlayerWithoutShift(std::uint16_t skill_id) {
  return Holds(kWithoutShift, skill_id) || Holds(kWithoutShiftActions, skill_id);
}

bool SkillTargetsPlayerUnderShift(std::uint16_t skill_id) { return Holds(kUnderShift, skill_id); }

bool NameClickPkZone(std::uint16_t map_type) {
  return map_type == kMapTypePenaltyFreePk || map_type == kMapTypeFreePk ||
         map_type == kMapTypeBattlefield;
}

bool AimedSkillReachesPlayer(const AimedPlayerScene& scene) {
  if (scene.alt_held) {
    return false;
  }
  // MAJ INVERSE LA PREMIÈRE TABLE : un soutien part sans elle.
  const bool without_shift = SkillTargetsPlayerWithoutShift(scene.skill_id);
  const bool base = without_shift ? !scene.shift_held : scene.shift_held;
  if (scene.self) {
    return scene.support && (base || scene.open_field || scene.no_shift);
  }
  if (scene.battlefield) {
    return scene.other_team || base;
  }
  bool reaches = base;
  if (scene.beyond_party) {
    reaches = reaches || !scene.online_party_member;
  } else if (scene.beyond_guild) {
    reaches = reaches || !scene.same_guild;
  }
  if (scene.siege_map) {
    if (!scene.same_guild && !scene.allied_guild) {
      return true;
    }
    return without_shift ||
           ((scene.no_shift || scene.shift_held) && SkillTargetsPlayerUnderShift(scene.skill_id));
  }
  if (scene.open_field) {
    reaches = reaches || !scene.online_party_member;
  }
  return reaches;
}

bool AimedSkillReachesMonster(std::uint16_t skill_id, bool shift_held, bool no_shift, bool alt_held,
                              bool own_guild_spared) {
  if (alt_held || own_guild_spared) {
    return false;
  }
  return no_shift || (SkillTargetsPlayerWithoutShift(skill_id) ? shift_held : !shift_held);
}

NameClickOrder NameClickOrderOf(const NameClickScene& scene) {
  if (scene.dead) {
    return NameClickOrder::kNothing;
  }
  const bool skill_on_someone =
      scene.aim == NamedAim::kOffensive || scene.aim == NamedAim::kSupport;
  if (scene.self && scene.aim != NamedAim::kSupport) {
    return NameClickOrder::kNothing;
  }
  bool acts = scene.shift_held;
  if (skill_on_someone) {
    acts = acts || SkillTargetsPlayerWithoutShift(scene.skill_id) ||
           ((scene.no_shift || scene.shift_held) && SkillTargetsPlayerUnderShift(scene.skill_id));
    // LA ZONE DE COMBAT LIBRE PASSE AVANT LE SIÈGE : sur une carte qui porte les
    // deux, seul le groupe protège.
    if (scene.pk_zone) {
      acts = acts || !scene.online_party_member;
    } else if (scene.siege_map) {
      acts = acts || (scene.seen && !scene.same_guild && !scene.allied_guild);
    }
  }
  if (!acts || scene.second_of_double || scene.alt_held) {
    return NameClickOrder::kNothing;
  }
  switch (scene.aim) {
    case NamedAim::kNothing:
      return NameClickOrder::kAttack;
    case NamedAim::kOffensive:
    case NamedAim::kSupport:
      return NameClickOrder::kSkill;
    case NamedAim::kElsewhere:
      return NameClickOrder::kEngageOnly;
  }
  return NameClickOrder::kNothing;
}

}  // namespace moonclient
