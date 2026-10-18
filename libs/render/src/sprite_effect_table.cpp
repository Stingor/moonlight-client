#include "ro/render/sprite_effect_table.h"

#include <algorithm>
#include <array>

#include "ro/core/sorted_table.h"
#include "ro/render/sprite_catalog.h"

namespace ro::render {
namespace {

// LES MOTS DE RENDU de ces particules. Une particule neuve porte le premier ;
// les autres sont ceux que leur identifiant pose. Voir `effect_render_word.h`.
constexpr std::uint32_t kWordOverEverything = 0x200;
constexpr std::uint32_t kWordSortedEarly = 0x101;
constexpr std::uint32_t kWordEarlyOwnBlend = 0xD;
constexpr std::uint32_t kWordTranslucent = 0x1;
constexpr std::uint32_t kWordOverEverythingUnderFog = 0x209;

// Les périodes d'image, en tics.
constexpr float kPeriodOfNewParticle = 2.0F;
constexpr float kPeriodShort = 4.0F;
constexpr float kPeriodMedium = 6.0F;
constexpr float kPeriodLong = 8.0F;
constexpr float kPeriodText = 12.0F;

// Les avances vers la caméra : voir `EffectClassSpriteDraw::depth_bias`.
constexpr float kBiasSmall = 0.0001F;
constexpr float kBiasBody = 0.0003F;
constexpr float kBiasLarge = 0.001F;

// Les hauteurs, en unités au-dessus des pieds.
constexpr float kLiftLow = 3.0F;
constexpr float kLiftWaist = 8.0F;
constexpr float kLiftFixed = 10.0F;
constexpr float kLiftHead = 25.0F;
constexpr float kLiftPenanceOrdinary = 17.0F;
constexpr float kLiftPenanceDoram = 13.7F;
constexpr float kLiftFrostOrdinary = 0.4F;
constexpr float kLiftFrostDoram = 2.2F;

constexpr float kLargeScale = 1.5F;

// L'aura d'un sujet a une action par couleur.
constexpr std::int32_t kSubjectAuraFirstAction = 0;
constexpr std::int32_t kSubjectAuraSecondAction = 1;
constexpr std::int32_t kSubjectAuraThirdAction = 2;

using enum SpriteEffectPlay;
using enum SpriteEffectLife;

// Un sprite qui suit son porteur, relevé de sa hauteur et du métier.
constexpr SpriteEffectRecipe OnBearer(std::uint32_t effect_id, std::string_view sprite,
                                      float period, std::uint32_t word, float bias, float lift) {
  return SpriteEffectRecipe{.effect_id = effect_id,
                            .sprite = sprite,
                            .frame_period_ticks = period,
                            .render_word = word,
                            .depth_bias = bias,
                            .lift_units = lift,
                            .doram_lift_units = lift,
                            .adds_job_lift = true};
}

// Un sprite joué une fois à la place où il naît.
constexpr SpriteEffectRecipe PlayedOnce(std::uint32_t effect_id, std::string_view sprite,
                                        float bias) {
  return SpriteEffectRecipe{.effect_id = effect_id,
                            .sprite = sprite,
                            .frame_period_ticks = kPeriodShort,
                            .play = kOnce,
                            .life = kUntilPlayed,
                            .render_word = kWordSortedEarly,
                            .depth_bias = bias};
}

// Une bulle noire, aux pieds de son porteur.
constexpr SpriteEffectRecipe AtFeet(std::uint32_t effect_id, std::string_view sprite) {
  return SpriteEffectRecipe{.effect_id = effect_id,
                            .sprite = sprite,
                            .frame_period_ticks = kPeriodMedium,
                            .render_word = kWordTranslucent,
                            .depth_bias = kBiasSmall};
}

// TRIÉE, et la recherche en dépend.
constexpr std::array<SpriteEffectRecipe, 38> kRecipes = {{
    SpriteEffectRecipe{.effect_id = 990,
                       .sprite = "이팩트\\m_ef03",
                       .frame_period_ticks = kPeriodShort,
                       .life = kFixedTicks,
                       .render_word = kWordOverEverything,
                       .scale = kLargeScale},
    OnBearer(1130, "이팩트\\bakuretsu_hadou\\bakuretsu_hadou", kPeriodLong, kWordSortedEarly,
             kBiasBody, kLiftWaist),
    SpriteEffectRecipe{.effect_id = 1147,
                       .sprite = "이팩트\\EF_POPCORN\\EF_POPCORN",
                       .frame_period_ticks = kPeriodLong,
                       .play = kOnce,
                       .life = kUntilPlayed,
                       .render_word = kWordOverEverything,
                       .depth_bias = kBiasBody,
                       .lift_units = kLiftFixed,
                       .doram_lift_units = kLiftFixed},
    OnBearer(1151, "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_NOTHING", kPeriodOfNewParticle,
             kWordOverEverything, kBiasBody, kLiftHead),
    OnBearer(1152, "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_WATER", kPeriodOfNewParticle,
             kWordOverEverything, kBiasBody, kLiftHead),
    OnBearer(1153, "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_GROUND", kPeriodOfNewParticle,
             kWordOverEverything, kBiasBody, kLiftHead),
    OnBearer(1154, "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_FIRE", kPeriodOfNewParticle,
             kWordOverEverything, kBiasBody, kLiftHead),
    OnBearer(1155, "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_WIND", kPeriodOfNewParticle,
             kWordOverEverything, kBiasBody, kLiftHead),
    OnBearer(1156, "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_POISON", kPeriodOfNewParticle,
             kWordOverEverything, kBiasBody, kLiftHead),
    OnBearer(1157, "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_SAINT", kPeriodOfNewParticle,
             kWordOverEverything, kBiasBody, kLiftHead),
    OnBearer(1158, "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_DARKNESS", kPeriodOfNewParticle,
             kWordOverEverything, kBiasBody, kLiftHead),
    OnBearer(1159, "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_TELEKINESIS",
             kPeriodOfNewParticle, kWordOverEverything, kBiasBody, kLiftHead),
    OnBearer(1160, "이팩트\\EF_IMMUNE_PROPERTY\\EF_IMMUNE_PROPERTY_UNDEAD", kPeriodOfNewParticle,
             kWordOverEverything, kBiasBody, kLiftHead),
    SpriteEffectRecipe{.effect_id = 1192,
                       .sprite = "이팩트\\random_opt_spr\\yellow",
                       .frame_period_ticks = kPeriodShort,
                       .render_word = kWordOverEverything,
                       .lift_units = kLiftFixed,
                       .doram_lift_units = kLiftFixed},
    SpriteEffectRecipe{.effect_id = 1211,
                       .sprite = "이팩트\\subject_aura\\subject_aura",
                       .action = kSubjectAuraThirdAction,
                       .frame_period_ticks = kPeriodLong,
                       .render_word = kWordSortedEarly,
                       .depth_bias = kBiasBody,
                       .lift_units = kLiftWaist,
                       .doram_lift_units = kLiftWaist,
                       .adds_job_lift = true},
    SpriteEffectRecipe{.effect_id = 1212,
                       .sprite = "이팩트\\subject_aura\\subject_aura",
                       .action = kSubjectAuraFirstAction,
                       .frame_period_ticks = kPeriodLong,
                       .render_word = kWordSortedEarly,
                       .depth_bias = kBiasBody,
                       .lift_units = kLiftWaist,
                       .doram_lift_units = kLiftWaist,
                       .adds_job_lift = true},
    SpriteEffectRecipe{.effect_id = 1213,
                       .sprite = "이팩트\\subject_aura\\subject_aura",
                       .action = kSubjectAuraSecondAction,
                       .frame_period_ticks = kPeriodLong,
                       .render_word = kWordSortedEarly,
                       .depth_bias = kBiasBody,
                       .lift_units = kLiftWaist,
                       .doram_lift_units = kLiftWaist,
                       .adds_job_lift = true},
    PlayedOnce(1231, "이팩트\\book_of_dimension\\book_of_dimension", 0.0F),
    PlayedOnce(1232, "이팩트\\book_of_creatingstar\\book_of_creatingstar", 0.0F),
    PlayedOnce(1233, "이팩트\\curse_explosion\\curse_explosion", 0.0F),
    PlayedOnce(1234, "이팩트\\soul_reaper\\soul_reaper", 0.0F),
    PlayedOnce(1235, "이팩트\\soul_shadow\\soul_shadow", kBiasBody),
    PlayedOnce(1236, "이팩트\\soul_fairy\\soul_fairy", kBiasBody),
    PlayedOnce(1237, "이팩트\\soul_falcon\\soul_falcon", kBiasBody),
    SpriteEffectRecipe{.effect_id = 1240,
                       .sprite = "이팩트\\digital_space\\digital_space",
                       .frame_period_ticks = kPeriodLong,
                       .render_word = kWordOverEverythingUnderFog,
                       .floats_with_bearer = true},
    PlayedOnce(1242, "이팩트\\soul_explosion\\soul_explosion", kBiasBody),
    OnBearer(1377, "이팩트\\valkyrie_wing\\valkyrie_wing", kPeriodMedium, kWordSortedEarly,
             kBiasLarge, kLiftLow),
    AtFeet(1419, "이팩트\\c_amdarais_effect"),
    AtFeet(1617, "이팩트\\black_bubble\\small\\blackbubble_small"),
    AtFeet(1618, "이팩트\\black_bubble\\midium\\blackbubble_midium"),
    AtFeet(1619, "이팩트\\black_bubble\\large\\blackbubble_large"),
    OnBearer(2394, "이팩트\\Serpent_Shadow\\Serpent_Shadow", kPeriodShort, kWordEarlyOwnBlend,
             kBiasLarge, 0.0F),
    SpriteEffectRecipe{.effect_id = 2420,
                       .sprite = "몬스터\\violen_hit",
                       .frame_period_ticks = kPeriodShort,
                       .play = kOnce,
                       .life = kUntilPlayed,
                       .render_word = kWordOverEverything,
                       .depth_bias = kBiasLarge},
    OnBearer(2424, "이팩트\\C_Aura_Of_Ghost_S\\C_Aura_Of_Ghost_S", kPeriodShort,
             kWordOverEverything, kBiasLarge, 0.0F),
    SpriteEffectRecipe{.effect_id = 2428,
                       .sprite = "이팩트\\Atque_Poenitentia\\Atque_Poenitentia",
                       .frame_period_ticks = kPeriodShort,
                       .render_word = kWordEarlyOwnBlend,
                       .depth_bias = kBiasLarge,
                       .lift_units = kLiftPenanceOrdinary,
                       .doram_lift_units = kLiftPenanceDoram,
                       .adds_job_lift = true},
    SpriteEffectRecipe{.effect_id = 2429,
                       .sprite = "이팩트\\Perm_Frost_Oblivion\\Perm_Frost_Oblivion",
                       .frame_period_ticks = kPeriodShort,
                       .render_word = kWordTranslucent,
                       .depth_bias = kBiasLarge,
                       .lift_units = kLiftFrostOrdinary,
                       .doram_lift_units = kLiftFrostDoram},
    OnBearer(2430, "이팩트\\C_Guide_Of_Dead_Text\\C_Guide_Of_Dead_Text", kPeriodText,
             kWordOverEverything, kBiasLarge, 0.0F),
    OnBearer(2431, "이팩트\\C_Medjed_Text\\C_Medjed_Text", kPeriodText, kWordOverEverything,
             kBiasLarge, 0.0F),
}};

// LES MÉTIERS QUE LE CLIENT RELÈVE : montés, bébés montés, bébés, Doram.
struct JobRange {
  std::uint32_t first = 0;
  std::uint32_t last = 0;
};

constexpr float kMountedLift = 6.0F;
constexpr float kMountedBabyLift = 4.0F;
constexpr float kBabyLift = -1.0F;
constexpr float kDoramLift = -2.0F;

constexpr std::array<JobRange, 6> kMountedJobs = {
    {{13, 13}, {21, 21}, {4014, 4014}, {4022, 4022}, {4080, 4095}, {4278, 4281}}};
constexpr std::array<JobRange, 3> kMountedBabyJobs = {{{4036, 4036}, {4044, 4044}, {4109, 4112}}};
constexpr std::array<JobRange, 15> kBabyJobs = {{{4023, 4029},
                                                 {4030, 4044},
                                                 {4045, 4045},
                                                 {4096, 4112},
                                                 {4158, 4182},
                                                 {4191, 4191},
                                                 {4193, 4193},
                                                 {4194, 4196},
                                                 {4205, 4210},
                                                 {4220, 4220},
                                                 {4222, 4238},
                                                 {4241, 4242},
                                                 {4244, 4244},
                                                 {4247, 4247},
                                                 {4248, 4248}}};

template <std::size_t N>
[[nodiscard]] constexpr bool Within(const std::array<JobRange, N>& ranges, std::uint32_t job) {
  return std::ranges::any_of(
      ranges, [job](const JobRange& range) { return job >= range.first && job <= range.last; });
}

}  // namespace

const SpriteEffectRecipe* SpriteEffectRecipeFor(std::uint32_t effect_id) {
  return core::RowWithKey(kRecipes, effect_id, &SpriteEffectRecipe::effect_id);
}

std::span<const SpriteEffectRecipe> SpriteEffectRecipes() { return kRecipes; }

float JobEffectLift(std::uint32_t job) {
  // Les bébés montés passent AVANT les montés, et les deux avant les bébés.
  if (Within(kMountedBabyJobs, job)) {
    return kMountedBabyLift;
  }
  if (Within(kMountedJobs, job)) {
    return kMountedLift;
  }
  if (Within(kBabyJobs, job)) {
    return kBabyLift;
  }
  if (IsDoramJob(job)) {
    return kDoramLift;
  }
  return 0.0F;
}

float SpriteEffectLiftUnits(const SpriteEffectRecipe& recipe,
                            std::optional<std::uint32_t> bearer_job) {
  if (!bearer_job.has_value()) {
    return recipe.lift_units;
  }
  const float base = IsDoramJob(*bearer_job) ? recipe.doram_lift_units : recipe.lift_units;
  return recipe.adds_job_lift ? base + JobEffectLift(*bearer_job) : base;
}

}  // namespace ro::render
