#include "effects/effect_bench_view.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_arc_ribbon_seed.h"
#include "ro/render/effect_falling_seed.h"
#include "ro/render/effect_glow_sprite_seed.h"
#include "ro/render/effect_layered_ring_seed.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_particle_ring_seed.h"
#include "ro/render/effect_particle_seed.h"
#include "ro/render/effect_ribbon_wave_seed.h"
#include "ro/render/effect_scatter_seed.h"
#include "ro/render/effect_seeded_ring.h"
#include "ro/render/effect_spiral_fan_seed.h"
#include "ro/render/effect_table.h"
#include "ro/render/effect_tinted_quad_seed.h"
#include "ro/render/effect_track_seed.h"
#include "ro/render/effect_turning_ring_seed.h"
#include "ro/render/effect_white_burst_seed.h"
#include "ro/render/skill_unit_table.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_node_view.h"

namespace moonclient {
namespace {

constexpr std::string_view kBenchTitle = "Banc d'effets";

// Le nom court d'une figure, tel qu'il tient dans la colonne de droite.
[[nodiscard]] std::string_view ShapeName(ro::render::EffectNodeShape shape) {
  switch (shape) {
    case ro::render::EffectNodeShape::kRingBand:
      return "cone";
    case ro::render::EffectNodeShape::kRingCylinder:
      return "couronne";
    case ro::render::EffectNodeShape::kCrossedPlanes:
      return "croix";
    case ro::render::EffectNodeShape::kOrientedQuad:
      return "quad";
    case ro::render::EffectNodeShape::kCrossSpikes:
      return "pointes";
    case ro::render::EffectNodeShape::kUvSphere:
      return "sphere";
    case ro::render::EffectNodeShape::kSprite:
      break;
  }
  return "sprite";
}

// Le DERNIER segment d'un chemin, sans son extension : c'est le seul morceau
// qui tienne dans la colonne, et c'est celui qui nomme.
[[nodiscard]] std::string_view ShortName(std::string_view path) {
  const std::size_t slash = path.find_last_of("\\/");
  if (slash != std::string_view::npos) {
    path.remove_prefix(slash + 1);
  }
  const std::size_t dot = path.find_last_of('.');
  if (dot != std::string_view::npos) {
    path = path.substr(0, dot);
  }
  return path;
}

}  // namespace

// Le nom d'une famille d'effet d'acteur, pour la ligne du banc.
[[nodiscard]] std::string_view FamilyName(ro::render::ActorEffectFamily family) {
  switch (family) {
    case ro::render::ActorEffectFamily::kRenderFlag:
      return "drapeau";
    case ro::render::ActorEffectFamily::kTint:
      return "teinte";
    case ro::render::ActorEffectFamily::kSpriteClone:
      return "rémanence";
    case ro::render::ActorEffectFamily::kAppearance:
      return "apparence";
    case ro::render::ActorEffectFamily::kOrder:
      return "ordre";
    case ro::render::ActorEffectFamily::kSpin:
      return "rotation";
  }
  return "?";
}

// CE QU'UN EFFET D'ACTEUR CHANGE, en une ligne lisible.
//
// ⚠ ELLE DIT AUSSI CE QUI N'EST PAS PORTÉ. Un drapeau dont aucune pièce ne sait
// faire quelque chose se clique et ne montre rien ; sans le dire, on conclurait
// que le portage est cassé alors qu'il n'a jamais eu lieu. Une absence nommée
// se corrige, une absence muette se redécouvre.
[[nodiscard]] std::string ActorChangeDetail(const ro::render::ActorEffectChange& change) {
  std::string detail;
  if (change.render_bits != 0) {
    detail += "bits 0x";
    // Le mot de rendu s'écrit en hexadécimal : c'est ainsi qu'il se lit dans le
    // relevé.
    char chiffres[16] = {};
    std::snprintf(chiffres, sizeof(chiffres), "%X", change.render_bits);
    detail += chiffres;
  }
  if (ro::render::TintsActor(change)) {
    detail += detail.empty() ? "teinte" : " + teinte";
  }
  if (change.clone_period_ticks != 0) {
    detail += (detail.empty() ? "" : " + ") + std::string("rémanence/") +
              std::to_string(change.clone_period_ticks) + " tics";
  }
  if (change.command != 0) {
    detail +=
        (detail.empty() ? "" : " + ") + std::string("ordre ") + std::to_string(change.command);
  }
  if (detail.empty()) {
    detail = "rien de porté";
  }
  return detail;
}

void EffectBenchPanel::Rebuild() {
  entries_.clear();

  // TOUT CE QUE NOUS SAVONS NOMMER, des quatre tables à la fois : les deux du
  // client qui donnent un fichier, celle qui donne un patron de nœuds, et celle
  // des anneaux à pistes — qui ne se décrivent pas comme un patron.
  //
  // ⚠ TOUTES, et c'est le point. N'y mettre que les patrons faisait chercher
  // en vain le mur de feu, dont l'effet a un fichier : un banc qui ne montre
  // qu'une moitié fait conclure qu'une chose manque alors qu'elle est là.
  std::map<std::uint32_t, Entry> by_id;

  for (const std::span<const ro::render::EffectEntry> table :
       {ro::render::EffectTable(), ro::render::EffectPrimitiveTable()}) {
    for (const ro::render::EffectEntry& effect : table) {
      Entry& entry = by_id[effect.id];
      if (entry.effect_id != 0) {
        continue;
      }
      entry.effect_id = effect.id;
      entry.has_file = true;
      entry.label = std::to_string(effect.id) + " " + std::string(ShortName(effect.path));
      entry.detail =
          effect.variants > 0 ? "fichier x" + std::to_string(effect.variants) : "fichier";
    }
  }

  // UN EFFET, UNE LIGNE, et non un patron par ligne : c'est l'effet qu'on joue,
  // et ses patrons partent ensemble. Les montrer séparément ferait croire qu'on
  // peut n'en semer qu'un.
  std::uint32_t previous = 0;
  bool first = true;
  for (const ro::render::EffectNodePattern& pattern : ro::render::EffectNodePatternTable()) {
    if (!first && pattern.effect_id == previous) {
      continue;
    }
    first = false;
    previous = pattern.effect_id;

    const std::span<const ro::render::EffectNodePattern> patterns =
        ro::render::EffectNodePatternsFor(pattern.effect_id);
    // CE QUE LA LIGNE DIT : combien de nœuds, de quelles figures, et combien
    // d'images pour ce qui défile. C'est de quoi juger sans revenir au relevé.
    std::string detail;
    std::uint16_t images = 0;
    for (const ro::render::EffectNodePattern& each : patterns) {
      const std::string_view name = ShapeName(each.shape);
      if (detail.find(name) == std::string::npos) {
        if (!detail.empty()) {
          detail += '+';
        }
        detail += name;
      }
      images = std::max(images, each.texture_count);
    }
    if (images > 1) {
      detail += " x" + std::to_string(images);
    }

    Entry& entry = by_id[pattern.effect_id];
    // UN EFFET QUI A LES DEUX GARDE SON FICHIER : c'est ce que le client joue,
    // et le patron n'en est que notre portage. Mais la ligne dit les deux, sans
    // quoi on croirait le patron absent.

    if (entry.has_file) {
      entry.detail += " + " + detail;
      continue;
    }
    entry.effect_id = pattern.effect_id;
    entry.label = std::to_string(pattern.effect_id) + " " + std::string(pattern.client_function);
    entry.detail = std::move(detail);
  }

  // LES ANNEAUX À PISTES, qui ne se décrivent pas comme un patron : leur
  // géométrie vient d'un ÉTAT et non des champs du nœud. Ils manqueraient au
  // banc s'il ne lisait que la table des patrons — vingt-trois effets sans
  // ligne, donc sans rien pour dire qu'ils existent.
  std::uint32_t previous_ring = 0;
  bool first_ring = true;
  for (const ro::render::EffectRingNodePattern& ring : ro::render::EffectRingNodePatternTable()) {
    if (!first_ring && ring.effect_id == previous_ring) {
      continue;
    }
    first_ring = false;
    previous_ring = ring.effect_id;

    const std::span<const ro::render::EffectRingNodePattern> rings =
        ro::render::EffectRingNodePatternsFor(ring.effect_id);
    std::string detail = "jupe";
    if (rings.size() > 1) {
      detail += " x" + std::to_string(rings.size());
    }

    Entry& entry = by_id[ring.effect_id];
    // UN EFFET QUI A DÉJÀ UNE LIGNE la garde et l'allonge : fichier ou patron, ce
    // qu'elle disait reste vrai, et l'anneau s'y ajoute.
    if (!entry.label.empty()) {
      entry.detail += " + " + detail;
      continue;
    }
    entry.effect_id = ring.effect_id;
    entry.label = std::to_string(ring.effect_id) + " " + std::string(ring.client_function);
    entry.detail = std::move(detail);
  }

  // LES NŒUDS DE PARTICULES, dernière des tables qui montrent. Ils ne se
  // décrivent pas plus comme un patron que les anneaux : leur géométrie vient
  // d'un état, et leur semis se tire au sort.
  std::uint32_t previous_particle = 0;
  bool first_particle = true;
  for (const ro::render::EffectRisingParticlePattern& particle :
       ro::render::EffectRisingParticlePatternTable()) {
    if (!first_particle && particle.effect_id == previous_particle) {
      continue;
    }
    first_particle = false;
    previous_particle = particle.effect_id;

    const std::span<const ro::render::EffectRisingParticlePattern> all =
        ro::render::EffectRisingParticlePatternsFor(particle.effect_id);
    std::string detail = "particules";
    if (all.size() > 1) {
      detail += " x" + std::to_string(all.size());
    }

    Entry& entry = by_id[particle.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + " + detail;
      continue;
    }
    entry.effect_id = particle.effect_id;
    entry.label = std::to_string(particle.effect_id) + " " + std::string(particle.client_function);
    entry.detail = std::move(detail);
  }

  // LES ANNEAUX QUI TOURNENT, sous-type 50.
  for (const ro::render::EffectTurningRingPattern& ring_index :
       ro::render::EffectTurningRingPatternTable()) {
    Entry& entry = by_id[ring_index.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + anneau tournant";
      continue;
    }
    entry.effect_id = ring_index.effect_id;
    entry.label =
        std::to_string(ring_index.effect_id) + " " + std::string(ring_index.client_function);
    entry.detail = "anneau tournant";
  }

  // LES ONDES DE RUBAN, sous-type 36.
  for (const ro::render::EffectRibbonWavePattern& wave :
       ro::render::EffectRibbonWavePatternTable()) {
    Entry& entry = by_id[wave.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + onde";
      continue;
    }
    entry.effect_id = wave.effect_id;
    entry.label = std::to_string(wave.effect_id) + " " + std::string(wave.client_function);
    entry.detail = "onde";
  }

  // LES BOUFFÉES BLANCHES, second bâtisseur du même sous-type 43. Elles ont
  // leur propre table parce que leur semis diffère, mais leurs nœuds prennent
  // la forme de la particule montante.
  for (const ro::render::EffectWhiteBurstPattern& bouffee :
       ro::render::EffectWhiteBurstPatternTable()) {
    Entry& entry = by_id[bouffee.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + bouffée";
      continue;
    }
    entry.effect_id = bouffee.effect_id;
    entry.label = std::to_string(bouffee.effect_id) + " " + std::string(bouffee.client_function);
    entry.detail = "bouffée";
  }

  // LES ARCS DE RUBAN, le plus gros lot du sous-type 82.
  std::uint32_t previous_arc = 0;
  bool first_arc = true;
  for (const ro::render::EffectArcRibbonNodePattern& arc :
       ro::render::EffectArcRibbonNodePatternTable()) {
    if (!first_arc && arc.effect_id == previous_arc) {
      continue;
    }
    first_arc = false;
    previous_arc = arc.effect_id;

    const std::span<const ro::render::EffectArcRibbonNodePattern> all =
        ro::render::EffectArcRibbonNodePatternsFor(arc.effect_id);
    std::string detail = "arc";
    if (all.size() > 1) {
      detail += " x" + std::to_string(all.size());
    }

    Entry& entry = by_id[arc.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + " + detail;
      continue;
    }
    entry.effect_id = arc.effect_id;
    entry.label = std::to_string(arc.effect_id) + " " + std::string(arc.client_function);
    entry.detail = std::move(detail);
  }

  // LES CARRÉS TEINTÉS du sous-type 47.
  std::uint32_t previous_quad = 0;
  bool first_quad = true;
  for (const ro::render::EffectTintedQuadNodePattern& quad :
       ro::render::EffectTintedQuadNodePatternTable()) {
    if (!first_quad && quad.effect_id == previous_quad) {
      continue;
    }
    first_quad = false;
    previous_quad = quad.effect_id;

    std::string detail = "carre";
    const std::span<const ro::render::EffectTintedQuadNodePattern> all =
        ro::render::EffectTintedQuadNodePatternsFor(quad.effect_id);
    if (all.size() > 1) {
      detail += " x" + std::to_string(all.size());
    }

    Entry& entry = by_id[quad.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + " + detail;
      continue;
    }
    entry.effect_id = quad.effect_id;
    entry.label = std::to_string(quad.effect_id) + " " + std::string(quad.client_function);
    entry.detail = std::move(detail);
  }

  // LES CHAMPS DISPERSÉS du sous-type 33.
  std::uint32_t previous_field = 0;
  bool first_field = true;
  for (const ro::render::EffectScatterNodePattern& champ :
       ro::render::EffectScatterNodePatternTable()) {
    if (!first_field && champ.effect_id == previous_field) {
      continue;
    }
    first_field = false;
    previous_field = champ.effect_id;

    std::string detail = "champ";
    const std::span<const ro::render::EffectScatterNodePattern> all =
        ro::render::EffectScatterNodePatternsFor(champ.effect_id);
    if (all.size() > 1) {
      detail += " x" + std::to_string(all.size());
    }

    Entry& entry = by_id[champ.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + " + detail;
      continue;
    }
    entry.effect_id = champ.effect_id;
    entry.label = std::to_string(champ.effect_id) + " " + std::string(champ.client_function);
    entry.detail = std::move(detail);
  }

  // LES OBJETS QUI TOMBENT du sous-type 87.
  std::uint32_t previous_item = 0;
  bool first_item = true;
  for (const ro::render::EffectFallingNodePattern& objet :
       ro::render::EffectFallingNodePatternTable()) {
    if (!first_item && objet.effect_id == previous_item) {
      continue;
    }
    first_item = false;
    previous_item = objet.effect_id;

    std::string detail = "chute";
    const std::span<const ro::render::EffectFallingNodePattern> all =
        ro::render::EffectFallingNodePatternsFor(objet.effect_id);
    if (all.size() > 1) {
      detail += " x" + std::to_string(all.size());
    }

    Entry& entry = by_id[objet.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + " + detail;
      continue;
    }
    entry.effect_id = objet.effect_id;
    entry.label = std::to_string(objet.effect_id) + " " + std::string(objet.client_function);
    entry.detail = std::move(detail);
  }

  // LES SPRITES DE MONSTRE du sous-type 153.
  for (const ro::render::EffectGlowSpriteNodePattern& monstre :
       ro::render::EffectGlowSpriteNodePatternTable()) {
    Entry& entry = by_id[monstre.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + monstre";
      continue;
    }
    entry.effect_id = monstre.effect_id;
    entry.label = std::to_string(monstre.effect_id) + " " + std::string(monstre.client_function);
    entry.detail = "monstre";
  }

  // LA FAMILLE `Ring_0` dans son entier.
  for (const ro::render::EffectSeededRingPattern& ring_index :
       ro::render::EffectSeededRingPatternTable()) {
    Entry& entry = by_id[ring_index.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + anneau";
      continue;
    }
    entry.effect_id = ring_index.effect_id;
    entry.label =
        std::to_string(ring_index.effect_id) + " " + std::string(ring_index.client_function);
    entry.detail = "anneau";
  }

  // LES ANNEAUX DE PARTICULES du sous-type 31.
  for (const ro::render::EffectParticleRingNodePattern& ring_index :
       ro::render::EffectParticleRingNodePatternTable()) {
    Entry& entry = by_id[ring_index.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + croix d'anneaux";
      continue;
    }
    entry.effect_id = ring_index.effect_id;
    entry.label =
        std::to_string(ring_index.effect_id) + " " + std::string(ring_index.client_function);
    entry.detail = "croix d'anneaux";
  }

  // LES ANNEAUX EN COUCHES du sous-type 23.
  for (const ro::render::EffectLayeredRingNodePattern& ring_index :
       ro::render::EffectLayeredRingNodePatternTable()) {
    Entry& entry = by_id[ring_index.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + anneau en couches";
      continue;
    }
    entry.effect_id = ring_index.effect_id;
    entry.label =
        std::to_string(ring_index.effect_id) + " " + std::string(ring_index.client_function);
    entry.detail = "anneau en couches";
  }

  // LES ÉVENTAILS du sous-type 147.
  for (const ro::render::EffectSpiralFanNodePattern& fan :
       ro::render::EffectSpiralFanNodePatternTable()) {
    Entry& entry = by_id[fan.effect_id];
    if (!entry.label.empty()) {
      entry.detail += " + éventail";
      continue;
    }
    entry.effect_id = fan.effect_id;
    entry.label = std::to_string(fan.effect_id) + " " + std::string(fan.client_function);
    entry.detail = "éventail";
  }

  // CE QU'UNE UNITÉ POSÉE DEMANDE se marque sur la ligne de son effet : c'est
  // par le numéro d'unité qu'on les cherche quand on vient du protocole, et
  // rien d'autre ne relie les deux numérotations.
  for (const ro::render::SkillUnitVisualEntry& unit : ro::render::SkillUnitVisualTable()) {
    const auto found = by_id.find(unit.effect_id);
    if (found == by_id.end()) {
      // L'unité demande un effet que nous ne savons pas nommer : la ligne
      // existe quand même, et elle le dit. Une absence nommée se corrige, une
      // absence muette se redécouvre.
      Entry entry;
      entry.effect_id = unit.effect_id;
      entry.label = std::to_string(unit.effect_id) + " (procédural non relevé)";
      entry.detail = "unité " + std::to_string(unit.unit_id);
      by_id.emplace(unit.effect_id, std::move(entry));
      continue;
    }
    found->second.detail += " / unité " + std::to_string(unit.unit_id);
  }

  // LA NEUVIÈME FAMILLE : les effets qui ne dessinent rien et changent
  // l'ACTEUR. Ils n'ont ni fichier ni patron de nœud, et sans cette boucle le
  // banc les ignorait — c'est exactement la panne qui avait fait chercher le
  // mur de feu en vain, un chemin de plus plus tard.
  for (const ro::render::ActorEffectChange& change : ro::render::ActorEffectChangeTable()) {
    Entry& entry = by_id[change.effect_id];
    if (entry.effect_id != 0) {
      continue;
    }
    entry.effect_id = change.effect_id;
    entry.on_actor = true;
    entry.label = std::to_string(change.effect_id) + " " + std::string(FamilyName(change.family));
    entry.detail = ActorChangeDetail(change);
  }

  entries_.reserve(by_id.size());
  for (auto& [effect_id, entry] : by_id) {
    entries_.push_back(std::move(entry));
  }
}

void EffectBenchPanel::Toggle() {
  open_ = !open_;
  if (open_ && entries_.empty()) {
    Rebuild();
  }
}

ro::render::SkillPanelLayout EffectBenchPanel::Placement(float viewport_width,
                                                         float viewport_height) const {
  return ro::render::SkillPanelPlacement(viewport_width, viewport_height, entries_.size());
}

bool EffectBenchPanel::Scroll(glm::vec2 mouse_px, float wheel_steps, float viewport_width,
                              float viewport_height) {
  if (!open_) {
    return false;
  }
  const ro::render::SkillPanelLayout layout = Placement(viewport_width, viewport_height);
  if (!ro::render::SkillPanelHolds(layout, mouse_px)) {
    return false;
  }
  first_row_ = ro::render::SkillPanelScrolled(layout, entries_.size(), first_row_, wheel_steps);
  return true;
}

bool EffectBenchPanel::Click(EffectsInPlay& effects, EffectNodesInPlay& nodes,
                             ro::sim::World& world, const MapStage& stage,
                             ro::sim::WalkPlanner& planner, glm::vec2 mouse_px,
                             float viewport_width, float viewport_height, std::uint32_t now_ms) {
  if (!open_) {
    return false;
  }
  const ro::render::SkillPanelLayout layout = Placement(viewport_width, viewport_height);
  if (!ro::render::SkillPanelHolds(layout, mouse_px)) {
    return false;
  }
  // LE CLIC EST PRIS dès qu'il tombe dans le panneau, même sur le bandeau ou
  // entre deux lignes : sans cela il traverserait et ferait marcher le
  // personnage sous ce qu'on lit.
  const std::optional<std::size_t> row = ro::render::SkillPanelRowAt(layout, mouse_px);
  if (!row.has_value() || first_row_ + *row >= entries_.size()) {
    return true;
  }
  const Entry& entry = entries_[first_row_ + *row];
  played_ = first_row_ + *row;
  has_played_ = true;

  const ro::sim::Actor* self = world.player();
  const ro::sim::FractionalCell cell =
      self != nullptr ? ro::sim::InterpolatedCell(*self, planner.PathFor(*self), now_ms)
                      : ro::sim::FractionalCell{};
  const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
  const glm::vec3 position(ground[0], ground[1], ground[2]);

  // LA NEUVIÈME FAMILLE SE JOUE SUR SOI, et par le chemin ORDINAIRE : on note le
  // fait dans le monde, exactement comme le ferait un paquet du serveur, et tout
  // ce qui compose une image le voit ensuite — son bruit compris, que `Notice`
  // fait entendre. Un banc qui court-circuiterait ce chemin éprouverait autre
  // chose que ce qui se joue en partie, et sonnerait deux fois.
  if (entry.on_actor) {
    const bool note = world.RecordEffect(world.player_id(), entry.effect_id, now_ms);
    world.RecordChat(ro::sim::ChatKind::kServer, 0,
                     entry.label + " : " + entry.detail +
                         (note ? " — posé sur soi" : " — AUCUN joueur à qui le poser"),
                     now_ms);
    return true;
  }

  // LE BRUIT PART AVEC L'IMAGE, et par le même chemin que celui d'un paquet
  // d'effet nu : le banc ne connaît pas l'audio, il dit ce qu'il a lancé et
  // `app` porte l'un à l'autre. Noté AVANT le semis, parce qu'un effet muet ou
  // qu'on ne sait pas montrer se juge aussi à son bruit — l'entendre sans rien
  // voir dit que c'est la figure qui manque, et non l'identifiant.
  effects.NoteLaunched(entry.effect_id, ground);

  // LE FICHIER D'ABORD quand il y en a un : c'est ce que le client joue, et le
  // patron n'en est que notre portage. Le tirage vient de l'instant, comme
  // partout ailleurs — un effet à trois fichiers n'en montre pas toujours le
  // même, et c'est ainsi qu'on les voit tous à force d'essayer.

  if (entry.has_file) {
    const std::string file = ro::render::EffectFile(entry.effect_id, now_ms);
    const bool played = !file.empty() && effects.Play(std::string(ro::render::kEffectFolder) + file,
                                                      position, now_ms);
    world.RecordChat(
        ro::sim::ChatKind::kServer, 0,
        entry.label + " : " + (played ? file + " joué" : file + " ILLISIBLE ou absent"), now_ms);
    return true;
  }

  const std::size_t count = ro::render::EffectNodePatternsFor(entry.effect_id).size() +
                            ro::render::EffectRingNodePatternsFor(entry.effect_id).size() +
                            ro::render::EffectRisingParticlePatternsFor(entry.effect_id).size() +
                            ro::render::EffectWhiteBurstPatternsFor(entry.effect_id).size() +
                            ro::render::EffectRibbonWavePatternsFor(entry.effect_id).size() +
                            ro::render::EffectTurningRingPatternsFor(entry.effect_id).size() +
                            ro::render::EffectArcRibbonNodePatternsFor(entry.effect_id).size() +
                            ro::render::EffectTintedQuadNodePatternsFor(entry.effect_id).size() +
                            ro::render::EffectScatterNodePatternsFor(entry.effect_id).size() +
                            ro::render::EffectFallingNodePatternsFor(entry.effect_id).size() +
                            ro::render::EffectGlowSpriteNodePatternsFor(entry.effect_id).size();
  const bool sown = nodes.PlayProcedural(entry.effect_id, position);
  // CE QUI A ÉTÉ SEMÉ SE DIT : un nœud qu'on ne voit pas et un nœud qui n'a pas
  // été semé laissent le même écran, et seul le journal les distingue.
  world.RecordChat(ro::sim::ChatKind::kServer, 0,
                   entry.label + " : " + std::to_string(count) + " patron(s), " +
                       (sown ? "semé(s)" : "AUCUN semé"),
                   now_ms);
  return true;
}

std::vector<ro::render::ScreenQuad> EffectBenchPanel::Drawn(const ro::render::FontAtlas& font,
                                                            float viewport_width,
                                                            float viewport_height) {
  if (!open_) {
    return {};
  }
  const ro::render::SkillPanelLayout layout = Placement(viewport_width, viewport_height);
  first_row_ = ro::render::SkillPanelScrolled(layout, entries_.size(), first_row_, 0.0F);

  std::vector<ro::render::SkillPanelRow> rows;
  rows.reserve(entries_.size());
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    rows.push_back(ro::render::SkillPanelRow{entries_[index].label, entries_[index].detail,
                                             has_played_ && index == played_, false});
  }
  return ro::render::ComposeSkillPanel(font, layout, kBenchTitle, rows, first_row_);
}

}  // namespace moonclient
