#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/render/sprite_catalog.h"

namespace ro::render {
namespace {

// Les métiers Doram, dont le client décale la position des objets par le Lua.
constexpr std::array<std::uint32_t, 7> kDoramJobs = {4217, 4218, 4219, 4220, 4221, 4308, 4315};

// LES ESPÈCES QUI TIRENT UN PROJECTILE, et le sprite de chacune.
//
// ⚠ L'ESPÈCE ET SON SPRITE SE LISENT À DEUX ENDROITS : une espèce tire par
// l'aiguillage des effets d'espèce du client, et s'habille par
// la table de la classe où il la mène. Une espèce que la table habille sans
// que l'aiguillage la fasse tirer n'est pas ici — et une espèce que
// l'aiguillage envoie AILLEURS non plus : 3791 frappe (voir `kCreatureStrikes`),
// et le client ne lui fait tirer aucune flèche.
//
// Le dossier est celui des monstres, « 몬스터 ».
//
// ET CHAQUE ESPÈCE A SA CLASSE DE PROJECTILE : la flèche, pour celles que le
// client habille par la table de la flèche ; la munition en sprite pour celles
// qu'il envoie à l'effet 2404. Voir `ProjectileImageRule`.
struct CreatureArrow {
  std::uint32_t job;
  std::string_view sprite_base;
  ProjectileImageRule rule = ProjectileImageRule::kArrow;
};
constexpr ProjectileImageRule kSpriteBullet = ProjectileImageRule::kSpriteBullet;
constexpr std::array<CreatureArrow, 88> kCreatureArrows = {{
    // ── La flèche ordinaire, habillée par sa propre table ─────────────────
    // Les archers à flèche ordinaire : Archer Squelette et ses pareils.
    {1016, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1189, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1253, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1258, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1276, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1282, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1285, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1410, "data\\sprite\\몬스터\\PEACH_TREE_BULLET"},
    {1412, "data\\sprite\\몬스터\\GINSENG_BULLET"},
    {1420, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1453, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1455, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1473, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {1495, "data\\sprite\\몬스터\\STONE_SHOOTER_BULLET"},
    {1498, "data\\sprite\\몬스터\\WOOTAN_SHOOTER_BULLET"},
    {1500, "data\\sprite\\몬스터\\PARASITE_BULLET"},
    {1531, "data\\sprite\\몬스터\\GINSENG_BULLET"},
    {1550, "data\\sprite\\몬스터\\PEACH_TREE_BULLET"},
    {1555, "data\\sprite\\몬스터\\PARASITE_BULLET"},
    {1664, "data\\sprite\\몬스터\\CANON_BULLET"},
    {1665, "data\\sprite\\몬스터\\CANON_BULLET_1"},
    {1666, "data\\sprite\\몬스터\\CANON_BULLET_2"},
    {1667, "data\\sprite\\몬스터\\CANON_BULLET_3"},
    {1688, "data\\sprite\\몬스터\\LADY_TANEE_BULLET"},
    {1865, "data\\sprite\\몬스터\\RAGGED_ZOMBIE_BULLET"},
    {2021, "data\\sprite\\몬스터\\LADY_TANEE_BULLET"},
    {2364, "data\\sprite\\몬스터\\PETAL_BULLET"},
    {2475, "data\\sprite\\몬스터\\MG_CORRUPTION_ROOT_BULLET"},
    {3882, "data\\sprite\\몬스터\\e_nyailo_ammo"},
    {6017, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {6018, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {6019, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {6020, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {6021, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {6022, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {6023, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {6024, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {6025, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {6026, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {20274, "data\\sprite\\몬스터\\STONE_SHOOTER_BULLET"},
    {20275, "data\\sprite\\몬스터\\WOOTAN_SHOOTER_BULLET"},
    {20279, "data\\sprite\\몬스터\\WOOTAN_SHOOTER_BULLET"},
    {20368, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {20575, "data\\sprite\\몬스터\\mg_corruption_root_bullet"},
    {20578, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {20585, "data\\sprite\\몬스터\\skel_archer_arrow"},
    {20851, "data\\sprite\\몬스터\\SUMMON_HELLTREE_FRUIT"},
    {21520, "data\\sprite\\몬스터\\EP19_LIMACINA_BULLET"},
    {21589, "data\\sprite\\몬스터\\EP19_LIMACINA_BULLET"},
    {21970, "data\\sprite\\몬스터\\phylla_bullet"},
    {21985, "data\\sprite\\몬스터\\canon_bullet_3"},
    {22001, "data\\sprite\\몬스터\\canon_bullet_3"},
    // ── La munition en sprite, l'effet 2404, habillée par espèce ──────────
    //
    // Sa classe fait défiler ses images à chaque mise à jour, en boucle, et
    // tourne son quad d'un angle réel — et beaucoup de ces munitions en ont
    // plusieurs : quatre pour le boulet de canon, huit pour celle de Rigel,
    // onze pour la racine corrompue. Voir `ProjectileImageRule::kSpriteBullet`.
    {20936, "data\\sprite\\몬스터\\canon_bullet", kSpriteBullet},
    {21052, "data\\sprite\\몬스터\\canon_bullet", kSpriteBullet},
    {22175, "data\\sprite\\몬스터\\RIGEL_BULLET", kSpriteBullet},
    {22176, "data\\sprite\\몬스터\\RIGEL_BULLET", kSpriteBullet},
    {22197, "data\\sprite\\몬스터\\mg_corruption_root_h_bullet", kSpriteBullet},
    {22198, "data\\sprite\\몬스터\\mg_corruption_root_h_bullet", kSpriteBullet},
    {22199, "data\\sprite\\몬스터\\mg_corruption_root_h_bullet", kSpriteBullet},
    {22204, "data\\sprite\\몬스터\\phylla_bullet", kSpriteBullet},
    {22214, "data\\sprite\\몬스터\\mg_corruption_root_bullet", kSpriteBullet},
    {22215, "data\\sprite\\몬스터\\mg_corruption_root_bullet", kSpriteBullet},
    {22216, "data\\sprite\\몬스터\\canon_bullet", kSpriteBullet},
    {22218, "data\\sprite\\몬스터\\canon_bullet", kSpriteBullet},
    {22219, "data\\sprite\\몬스터\\canon_bullet", kSpriteBullet},
    {22226, "data\\sprite\\몬스터\\mg_corruption_root_bullet", kSpriteBullet},
    {22227, "data\\sprite\\몬스터\\mg_corruption_root_bullet", kSpriteBullet},
    {22232, "data\\sprite\\몬스터\\canon_bullet", kSpriteBullet},
    {22234, "data\\sprite\\몬스터\\canon_bullet", kSpriteBullet},
    {22235, "data\\sprite\\몬스터\\canon_bullet", kSpriteBullet},
    {22245, "data\\sprite\\몬스터\\VR_MD_DREGG1_bullet", kSpriteBullet},
    {22246, "data\\sprite\\몬스터\\VR_MD_DREGG2_bullet", kSpriteBullet},
    // La seule de sa classe sans munition à elle : la flèche générique, mais
    // dessinée comme une munition.
    {22256, "data\\sprite\\몬스터\\skel_archer_arrow", kSpriteBullet},
    {22322, "data\\sprite\\몬스터\\EP21_YORKER_Bullet", kSpriteBullet},
    {22326, "data\\sprite\\몬스터\\EP21_YOSTER_D_bullet", kSpriteBullet},
    {22327, "data\\sprite\\몬스터\\EP21_YOSTER_E_bullet", kSpriteBullet},
    {22354, "data\\sprite\\몬스터\\EP21_YORMI_bullet", kSpriteBullet},
    {22355, "data\\sprite\\몬스터\\EP21_YORDOS_Bullet", kSpriteBullet},
    {22359, "data\\sprite\\몬스터\\EP21_YOSCOPUS_Bullet", kSpriteBullet},
    {22366, "data\\sprite\\몬스터\\EP21_YORDOS_Bullet", kSpriteBullet},
    {22383, "data\\sprite\\몬스터\\violen_bullet", kSpriteBullet},
    {22384, "data\\sprite\\몬스터\\violen_bullet", kSpriteBullet},
    {22416, "data\\sprite\\몬스터\\EP21_YORDOS_Bullet", kSpriteBullet},
    {22418, "data\\sprite\\몬스터\\EP21_YORMI_bullet", kSpriteBullet},
    {22420, "data\\sprite\\몬스터\\EP21_YOSCOPUS_Bullet", kSpriteBullet},
    {22464, "data\\sprite\\몬스터\\CH1_SHADOW_JAILER_bullet", kSpriteBullet},
    {22467, "data\\sprite\\몬스터\\CH1_SCABIOSA_bullet", kSpriteBullet},
    {22468, "data\\sprite\\몬스터\\CH1_LOBELIA_bullet", kSpriteBullet},
}};

// LES ESPÈCES DONT LE COUP S'ABAT SUR LA CIBLE — celles que l'aiguillage des
// effets d'espèce mène à l'attaque de la Mandragore — et le sprite que le
// client donne à chacune ; la Mandragore
// elle-même, et toute espèce que cette table ne nomme pas, prend
// `mandragora_atk`.
//
// L'Hydre est la seule dont la cadence est FORCÉE : deux tics par image, là où
// les autres jouent à celle de leur fichier.
constexpr float kHydraFramePeriodUnits = 2.0F;
struct CreatureStrike {
  std::uint32_t job;
  std::string_view sprite_base;
  float frame_period_units;
};
constexpr std::array<CreatureStrike, 31> kCreatureStrikes = {{
    {1020, "data\\sprite\\몬스터\\mandragora_atk", 0.0F},
    {1068, "data\\sprite\\몬스터\\hydra_atk", kHydraFramePeriodUnits},
    {1704, "data\\sprite\\몬스터\\tha_odium_ef", 0.0F},
    {1709, "data\\sprite\\몬스터\\tha_odium_ef", 0.0F},
    {1781, "data\\sprite\\몬스터\\drosera_bullet", 0.0F},
    {1884, "data\\sprite\\몬스터\\MAVKA_BULLET", 0.0F},
    {1886, "data\\sprite\\몬스터\\MAVKA_BULLET", 0.0F},
    {1957, "data\\sprite\\몬스터\\ENTWEIHEN_BULLET", 0.0F},
    {1958, "data\\sprite\\몬스터\\ENTWEIHEN_BULLET", 0.0F},
    {1959, "data\\sprite\\몬스터\\ENTWEIHEN_BULLET", 0.0F},
    {1960, "data\\sprite\\몬스터\\ENTWEIHEN_BULLET", 0.0F},
    {1961, "data\\sprite\\몬스터\\ENTWEIHEN_BULLET", 0.0F},
    {1988, "data\\sprite\\몬스터\\NEPENTHES_BULLET", 0.0F},
    // ⚠ Absent de l'installation de Moonlight : le client n'y dessine rien
    // non plus, et nous pas davantage.
    {3732, "data\\sprite\\몬스터\\JP_ABYSS_BOSS_1_atk", 0.0F},
    {3791, "data\\sprite\\몬스터\\jp_2017evt_atk", 0.0F},
    {20491, "data\\sprite\\몬스터\\JP_ABYSS_BOSS_1_atk", 0.0F},
    {20850, "data\\sprite\\몬스터\\summon_creeper_bullet", 0.0F},
    {21947, "data\\sprite\\몬스터\\drosera_bullet", 0.0F},
    {22361, "data\\sprite\\몬스터\\EP21_YORTUS_Bullet", 0.0F},
    {22364, "data\\sprite\\몬스터\\EP21_B_YORTUS_Bullet", 0.0F},
    {22395, "data\\sprite\\몬스터\\MEDJED_bullet", 0.0F},
    {22396, "data\\sprite\\몬스터\\MEDJED_bullet", 0.0F},
    {22401, "data\\sprite\\몬스터\\mg_corruption_root_h_bullet", 0.0F},
    {22402, "data\\sprite\\몬스터\\mg_corruption_root_h_bullet", 0.0F},
    {22404, "data\\sprite\\몬스터\\GUIDE_OF_DEAD_bullet", 0.0F},
    {22408, "data\\sprite\\몬스터\\E_SMART_ANDRE_Bullet", 0.0F},
    {22413, "data\\sprite\\몬스터\\EP21_YORTUS_Bullet", 0.0F},
    {22422, "data\\sprite\\몬스터\\EP21_B_YORTUS_Bullet", 0.0F},
    {22450, "data\\sprite\\몬스터\\CH1_FIRE_FLOWER_bullet", 0.0F},
    {22482, "data\\sprite\\몬스터\\CH1_SHADIEST_bullet", 0.0F},
    {22483, "data\\sprite\\몬스터\\CH1_SHADIEST_bullet", 0.0F},
}};

}  // namespace

bool IsDoramJob(std::uint32_t job) {
  return std::find(kDoramJobs.begin(), kDoramJobs.end(), job) != kDoramJobs.end();
}

std::string_view ArrowSpriteOf(const SpriteCatalog& catalog, const ActorLook& look,
                               bool shoots_souls) {
  if (look.player) {
    if (ShootsArrows(catalog, look)) {
      return kArrowSpriteBase;
    }
    if (ShootsWandBolts(catalog, look)) {
      return kWandBoltSpriteBase;
    }
    return shoots_souls && IsDoramJob(look.job) ? kSoulAttackSpriteBase : std::string_view{};
  }
  if (look.item) {
    return {};
  }
  for (const CreatureArrow& shooter : kCreatureArrows) {
    if (shooter.job == look.job) {
      return shooter.sprite_base;
    }
  }
  return {};
}

ProjectileImageRule ProjectileImageRuleOf(const ActorLook& look) {
  if (look.player || look.item) {
    return ProjectileImageRule::kArrow;
  }
  for (const CreatureArrow& shooter : kCreatureArrows) {
    if (shooter.job == look.job) {
      return shooter.rule;
    }
  }
  return ProjectileImageRule::kArrow;
}

ProjectileFlight ProjectileFlightOf(const SpriteCatalog& catalog, const ActorLook& look) {
  if (ShootsWandBolts(catalog, look)) {
    return ProjectileFlight{.image_rule = ProjectileImageRule::kArrow,
                            .clock_slowdown = kWandBoltSlowdown,
                            .turn_degrees = kWandBoltTurnDegrees};
  }
  return ProjectileFlight{.image_rule = ProjectileImageRuleOf(look)};
}

std::optional<CreatureStrikeSprite> StrikeSpriteOf(const ActorLook& look) {
  if (look.player || look.item) {
    return std::nullopt;
  }
  for (const CreatureStrike& striker : kCreatureStrikes) {
    if (striker.job == look.job) {
      return CreatureStrikeSprite{striker.sprite_base, striker.frame_period_units};
    }
  }
  return std::nullopt;
}

}  // namespace ro::render
