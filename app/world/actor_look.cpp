// L'APPARENCE D'UN ACTEUR, réduite à ce qui choisit ses fichiers, et la
// disposition de son .act — ce qui ne dépend que de ce que le serveur en a dit.

#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/render/creature_motion.h"
#include "ro/render/shown_job.h"
#include "ro/render/skill_impl.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/strike_play.h"
#include "ro/sim/status_effects.h"

#include "world/actor_view.h"

namespace moonclient {

bool ComposedAsPlayer(const ro::sim::Actor& actor) {
  // 🔴 LE TYPE D'OBJET 1 NE SE COMPOSE PAS, bien qu'il soit rangé avec les
  // joueurs : c'est un joueur DÉGUISÉ en une classe qui n'est pas un métier, et
  // il se dessine d'après cette classe, comme un PNJ. Le serveur pose aussi ce
  // type sur l'interlocuteur fictif d'une commande qui ouvre un dialogue, une
  // classe de PNJ caché posée sur la case du joueur : composé, il dessinerait un
  // novice par-dessus lui.
  constexpr std::uint8_t kDisguisedPlayerObject = 0x1;
  if (actor.kind == ro::sim::ActorKind::kPlayer) {
    return actor.object_type != kDisguisedPlayerObject;
  }
  // UN PNJ VÊTU D'UN MÉTIER se compose comme un joueur : voir `IsPlayerClass`.
  return (actor.kind == ro::sim::ActorKind::kNonPlayer &&
          ro::render::IsPlayerClass(actor.look.job));
}

namespace {

constexpr std::uint16_t kHiddenNpcJob = 111;

}  // namespace

bool IsUnseenClass(const ro::sim::Actor& actor) {
  constexpr std::uint16_t kUnseenJob = 104;
  constexpr std::uint16_t kHiddenWarpJob = 139;
  if (actor.kind == ro::sim::ActorKind::kItem || ComposedAsPlayer(actor)) {
    return false;
  }
  const std::uint16_t job = actor.look.job;
  return job == kUnseenJob || job == kHiddenNpcJob || job == kHiddenWarpJob;
}

bool IsPickedWithoutBody(const ro::sim::Actor& actor) {
  return actor.kind == ro::sim::ActorKind::kNonPlayer && IsUnseenClass(actor) &&
         actor.look.job == kHiddenNpcJob;
}

std::uint32_t ShownJobOf(const ro::sim::Actor& actor) {
  if (!ComposedAsPlayer(actor)) {
    return actor.look.job;
  }
  return ro::render::ShownJobFor(
      actor.look.job, actor.option,
      {.dressed_up = actor.status_effects.contains(ro::sim::kStatusDressUp),
       .madogear = actor.status_effects.contains(ro::sim::kStatusMadogear)});
}

ro::render::ActorLook LookOf(const ro::sim::Actor& actor) {
  ro::render::ActorLook look;
  look.player = ComposedAsPlayer(actor);
  look.item = actor.kind == ro::sim::ActorKind::kItem;
  // UN OBJET AU SOL SE DESSINE PAR SON IDENTIFIANT D'OBJET, sur 32 bits : voir
  // `GroundItem::item_id`.
  look.job =
      look.item && actor.ground_item.has_value() ? actor.ground_item->item_id : ShownJobOf(actor);
  look.sex = actor.look.sex;
  look.hair = actor.look.head;
  look.body_style = actor.look.body_style;
  look.headgears = {actor.look.lower_headgear, actor.look.middle_headgear,
                    actor.look.upper_headgear};
  look.weapon = actor.look.weapon;
  look.shield = actor.look.shield;
  look.robe = actor.look.robe;
  look.cash_mount = actor.status_effects.contains(ro::sim::kStatusAllRiding);
  look.hair_palette = actor.look.head_palette;
  look.body_palette = actor.look.body_palette;
  look.orc_head = (actor.option & ro::sim::kOptionOrcishHead) != 0;
  const auto madogear = actor.status_values.find(ro::sim::kStatusMadogear);
  look.madogear_suit =
      madogear != actor.status_values.end() && madogear->second == ro::sim::kMadogearSuitValue;
  // L'ACCESSOIRE D'UN PET vit dans sa coiffe basse : le serveur l'y pose à
  // l'apparition, et son changement d'état l'y remplace.
  if (actor.familiar == ro::sim::Familiar::kPet) {
    look.pet_accessory = actor.look.lower_headgear;
  }
  return look;
}

ro::render::SpriteLayout LayoutOf(const ro::sim::Actor& actor) {
  return ComposedAsPlayer(actor) ? ro::render::SpriteLayout::kPlayer
                                 : ro::render::SpriteLayout::kCreature;
}

ro::render::CreatureMotionFamily CreatureFamilyOf(const ro::sim::Actor& actor) {
  // LE PNJ, que toute motion remet à l'attente : un non-joueur sans familier, ou
  // toute classe de PNJ ou de portail. Deux échappent à la règle : la tombe d'un
  // boss, et le PNJ au sprite de monstre.
  constexpr std::uint32_t kFirstNpcJob = 45;
  constexpr std::uint32_t kLastNpcJob = 999;
  constexpr std::uint32_t kFirstExtendedNpcJob = 10001;
  constexpr std::uint32_t kLastExtendedNpcJob = 20000;
  constexpr std::uint32_t kBossTombJob = 565;
  constexpr std::uint8_t kMonsterLookingNpcObject = 0xC;
  // Les deux familles de monstres dont l'emploi joue le rang cinq.
  constexpr std::uint8_t kAbrMonsterObject = 0xD;
  constexpr std::uint8_t kBionicMonsterObject = 0xE;
  const std::uint32_t job = actor.look.job;
  const bool npc_object =
      actor.kind == ro::sim::ActorKind::kNonPlayer && actor.familiar == ro::sim::Familiar::kNone;
  const bool npc_job = (job >= kFirstNpcJob && job <= kLastNpcJob) ||
                       (job >= kFirstExtendedNpcJob && job <= kLastExtendedNpcJob);
  if ((npc_object || npc_job) && job != kBossTombJob &&
      actor.object_type != kMonsterLookingNpcObject) {
    return ro::render::CreatureMotionFamily::kNpc;
  }
  if (actor.familiar == ro::sim::Familiar::kHomunculus ||
      actor.familiar == ro::sim::Familiar::kElemental || actor.object_type == kAbrMonsterObject ||
      actor.object_type == kBionicMonsterObject) {
    return ro::render::CreatureMotionFamily::kSkillRank;
  }
  if (actor.familiar == ro::sim::Familiar::kMercenary) {
    return ro::render::CreatureMotionFamily::kMercenary;
  }
  return ro::render::CreatureMotionFamily::kOrdinary;
}

std::optional<std::size_t> ActionAnimationOf(const ro::sim::Actor& actor, std::int8_t action_state,
                                             ro::render::SpriteLayout layout) {
  if (layout == ro::render::SpriteLayout::kPlayer) {
    return ro::render::ActionAnimationFor(action_state, layout, ShownJobOf(actor));
  }
  return ro::render::CreatureActionAnimationFor(action_state, CreatureFamilyOf(actor),
                                                actor.look.job);
}

std::optional<std::size_t> PosedAnimationOf(const ro::render::SpriteCatalog* catalog,
                                            const ro::sim::Actor& actor, std::int8_t action_state,
                                            ro::render::SpriteLayout layout,
                                            std::uint32_t posed_ms) {
  if (catalog == nullptr || layout != ro::render::SpriteLayout::kPlayer) {
    return ActionAnimationOf(actor, action_state, layout);
  }
  return ro::render::PosedActionAnimation(*catalog, LookOf(actor), action_state,
                                          ro::render::ChanceTenthAt(posed_ms));
}

}  // namespace moonclient
