#include "world/item_drops.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "ro/render/effect_table.h"
#include "ro/render/sprite_billboard.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "world/item_dropper.h"

namespace moonclient {
namespace {

// Le mélange de Knuth, et de combien on décale son résultat pour n'en garder
// que vingt-quatre bits : une direction TIRÉE DE L'IDENTIFIANT de l'objet.
// Stable d'une image à l'autre, différente d'un objet au suivant — c'est ce qui
// fait qu'une pluie de butin part en gerbe au lieu de se superposer.
constexpr std::uint32_t kKnuthStride = 2654435761U;
constexpr std::uint32_t kKeptBitsShift = 8;
constexpr float kKeptValues = 16777216.0F;

// Depuis combien de temps le serveur a annoncé cet objet. Un instant ANTÉRIEUR
// ne se produit qu'en rejeu ; le traiter comme l'instant même vaut mieux qu'une
// soustraction non signée, qui vieillirait l'objet de quarante-neuf jours.
[[nodiscard]] std::uint32_t SinceSpawnMs(const ro::sim::GroundItem& item,
                                         std::uint32_t now_ms) noexcept {
  return now_ms > item.since_ms ? now_ms - item.since_ms : 0;
}

// La direction du jaillissement quand personne ne l'a lâché à portée.
[[nodiscard]] float AngleFromId(std::uint32_t item_id) noexcept {
  const std::uint32_t mixed = item_id * kKnuthStride;
  constexpr float kTurn = 2.0F * std::numbers::pi_v<float>;
  return static_cast<float>(mixed >> kKeptBitsShift) * (kTurn / kKeptValues);
}

}  // namespace

const ItemDrops::Start& ItemDrops::StartFor(const ro::sim::World& world, const MapStage& stage,
                                            const ro::sim::Actor& item,
                                            const std::array<float, 3>& resting_ground,
                                            std::uint32_t now_ms) {
  if (const auto found = starts_.find(item.id); found != starts_.end()) {
    return found->second;
  }
  // La purge AVANT l'insertion : elle invaliderait l'entrée qu'on vient de
  // rendre.
  if (starts_.size() >= kItemArcSweepAbove) {
    std::erase_if(starts_, [now_ms](const auto& entry) {
      return now_ms - entry.second.noticed_ms > kItemArcMaxAgeMs;
    });
  }

  Start start;
  start.noticed_ms = now_ms;
  // La taille du corps qui l'a lâché, ou celle d'un personnage ordinaire quand
  // on ne l'a jamais composé.
  float body_pixels = ro::render::kItemArcDefaultBodyPixels;
  const ro::sim::Actor* dropper = DropperNear(world, item, now_ms);
  const auto seen = dropper != nullptr ? bodies_.find(dropper->id) : bodies_.end();
  if (seen != bodies_.end()) {
    // LÀ OÙ SON SPRITE EST DESSINÉ, et non là où un chemin droit le mettrait.
    start.ground = seen->second.ground;
    body_pixels = seen->second.pixels;
  } else if (dropper != nullptr) {
    // Jamais composé — il vient d'entrer en vue, ou il est caché. Sa case vaut
    // mieux que rien, et elle est juste tant qu'il ne marche pas.
    const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(*dropper, now_ms);
    start.ground = GroundPositionAt(stage, cell.x, cell.y);
  } else {
    // À défaut, l'objet part d'à côté de sa case, dans une direction tirée de
    // son identifiant : moins juste, mais toujours mieux qu'une chute
    // verticale, et deux objets de la même salve ne se superposent pas.
    const float angle = AngleFromId(item.id);
    const float kick = settings_.kick_units;
    start.ground = {resting_ground[0] + std::cos(angle) * kick, resting_ground[1],
                    resting_ground[2] + std::sin(angle) * kick};
  }
  start.up_units =
      settings_.source_height_fraction * body_pixels / ro::render::kSpritePixelsPerWorldUnit;
  return starts_.emplace(item.id, start).first->second;
}

std::vector<SownNamedSound> ItemDrops::NoticePillars(const ro::sim::World& world,
                                                     const MapStage& stage, EffectsInPlay& effects,
                                                     std::uint32_t now_ms) {
  std::vector<SownNamedSound> heard;
  // Ce qui a quitté le monde libère son identifiant. ⚠ AVANT de servir les
  // neufs : un objet ramassé et un objet qui tombe sur la même case se suivent
  // de près, et le serveur réattribue.
  std::erase_if(lit_, [&world](std::uint32_t id) { return world.Find(id) == nullptr; });

  for (const auto& [id, item] : world.actors()) {
    // ⚠ CE QUE DIT L'ANNONCE, OU CE QU'ON A RETENU DU MÊME OBJET : un butin
    // retrouvé après une arrivée ne porte plus son mode — voir
    // `ItemPillarMemory`.
    //
    // ⚠ CE N'EST PAS LE SEUL EFFET QUE LE CLIENT JOUE ICI. Le drapeau seul lui
    // en fait chercher un second, propre à l'OBJET, dans sa fiche — une table
    // Lua de l'installation que nous ne lisons pas encore. Un butin dont le mode
    // est nul reste donc muet chez nous.
    const std::uint16_t mode = pillars_.ModeOf(world.map_name(), item);
    if (mode == 0) {
      continue;
    }
    // OÙ LE BUTIN SE TIENT À CETTE IMAGE, et non où il se posera : sa colonne le
    // suit dans sa chute.
    const std::array<float, 3> resting = GroundPositionAt(
        stage, static_cast<float>(item.position.x), static_cast<float>(item.position.y));
    const ItemDropPose pose = PoseAt(world, stage, item, resting, now_ms);
    const glm::vec3 where(pose.ground[0], pose.ground[1], pose.ground[2]);
    // Déjà planté : il n'y a plus qu'à le suivre. Sa boucle, elle, tourne toute
    // seule.
    if (!lit_.insert(id).second) {
      effects.MoveLooping(id, where);
      continue;
    }
    const std::uint16_t effect_id = ro::render::ItemDropPillarEffectId(mode);
    if (effect_id == 0) {
      continue;
    }
    const ro::render::ItemDropPillar pillar = ro::render::ItemDropPillarOf(effect_id);
    const std::string folder(ro::render::kEffectFolder);
    // 🔴 SANS RELÈVEMENT D'ANCRE : la flaque de ce pilier est dessinée à
    // l'ordonnée 240 de son fichier — `kStrOriginY`, donc l'origine même —, et
    // les quatre-vingts pixels que prennent les autres effets la feraient
    // flotter au-dessus du terrain.
    constexpr float kOnTheGround = 0.0F;
    // ⚠ LES DEUX FICHIERS : la colonne et la flaque à son pied. Tous deux
    // BOUCLENT tant que le butin est là, et s'éteignent avec lui.
    // ⚠ DERRIÈRE LES CORPS : une colonne qui signale un butin rare ne doit pas
    // noyer le butin qu'elle montre.
    constexpr bool kBehindActors = true;
    const bool pillar_top = effects.PlayLooping(folder + std::string(pillar.top), where, now_ms,
                                                kOnTheGround, id, kBehindActors);
    const bool pillar_bottom = effects.PlayLooping(folder + std::string(pillar.bottom), where,
                                                   now_ms, kOnTheGround, id, kBehindActors);
    // Ce qui n'a rien pu montrer ne fait rien entendre : un bruit sans image
    // ferait chercher un effet là où l'installation n'a pas le fichier. Et seul
    // un butin qui TOMBE se fait entendre : voir la déclaration.
    if ((pillar_top || pillar_bottom) && item.ground_item->falling) {
      heard.push_back(SownNamedSound{pillar.sound, item.position});
    }
  }
  return heard;
}

ItemDropPose ItemDrops::PoseAt(const ro::sim::World& world, const MapStage& stage,
                               const ro::sim::Actor& item,
                               const std::array<float, 3>& resting_ground, std::uint32_t now_ms) {
  ItemDropPose pose{.ground = resting_ground, .sparkling = false};
  if (!item.ground_item.has_value()) {
    return pose;
  }
  const std::uint32_t elapsed_ms = SinceSpawnMs(*item.ground_item, now_ms);
  pose.sparkling = ro::render::ItemSparklesAt(elapsed_ms);
  // Un objet déjà posé quand on est arrivé ne tombe pas : il scintille, et
  // c'est tout.
  if (!item.ground_item->falling) {
    return pose;
  }

  if (!settings_.arc) {
    pose.ground[1] += ro::render::ItemFallUpUnits(elapsed_ms);
    return pose;
  }

  // 🔴 LA FIN DE L'ARC SE LIT AVANT SON DÉPART, et l'inverse était un cycle : le
  // départ s'efface à la fin du vol, et le relire le RECRÉAIT à l'image
  // suivante, pour l'effacer encore. Chaque butin posé cherchait ainsi son
  // lâcheur soixante fois par seconde, et le disait autant.
  if (ro::render::ItemArcIsOver(settings_, elapsed_ms)) {
    // Posé. On rend la main : l'objet garde la case que le serveur lui a
    // donnée, exactement comme si le client l'y avait fait tomber.
    starts_.erase(item.id);
    return pose;
  }
  // Le départ porte la hauteur à laquelle le butin naît sur le corps de qui l'a
  // lâché ; l'arc ne fait que la faire décroître.
  const Start& start = StartFor(world, stage, item, resting_ground, now_ms);
  const ro::render::ItemArcStep step = ro::render::ItemArcAt(settings_, elapsed_ms, start.up_units);
  if (step.over) {
    starts_.erase(item.id);
    return pose;
  }
  for (std::size_t axis = 0; axis < pose.ground.size(); ++axis) {
    // ⚠ LE SOL SUIT LE MÊME CHEMIN QUE L'OBJET, et il est interpolé plutôt que
    // relu sous chaque point de vol : entre les pieds de l'entité et la case
    // d'arrivée le terrain peut monter ou descendre, et une cloche posée sur la
    // seule altitude d'arrivée ferait rentrer l'objet dans une pente. Le relire
    // au fil du vol le ferait au contraire suivre chaque marche d'un escalier.
    pose.ground[axis] =
        start.ground[axis] + (resting_ground[axis] - start.ground[axis]) * step.advance;
  }
  pose.ground[1] += step.up_units;
  return pose;
}

}  // namespace moonclient
