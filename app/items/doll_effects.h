#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/render/sprite_renderer.h"
#include "ro/render/sprite_vignette.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/ui/description_window.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "items/doll_body.h"
#include "items/doll_wearing.h"
#include "items/item_doll.h"
#include "world/actor_ghosts.h"
#include "world/map_stage.h"

namespace moonclient {

// L'EFFET DE CHAPEAU SUR LE PANTIN D'UN APERÇU : l'aura d'un costume, sa pluie
// d'or, son anneau — ce que la pièce montre en plus de son sprite, ou à sa place.
//
// 🔴 LES MÊMES PIÈCES QUE LA PARTIE, dans un monde à part. Un porteur factice,
// posé à la case du joueur, porte l'effet ; le moteur d'effets le sème par
// `NoticeHatEffects` comme sur n'importe qui, et `DrawEffectLayers` en a l'ordre
// de peinture. Les deux familles passent donc par là — le fichier `.str` comme
// l'effet dessiné en code —, sans rien toucher au vrai personnage ni au monde du
// jeu : le porteur n'existe que dans ce monde-ci.
//
// 🔴 ET LA CAMÉRA TOMBE SUR LE PANTIN : un pixel de sprite vaut un septième
// d'unité, pour un effet comme pour un corps, et le cadrage du pantin dit de
// combien il a grandi et où sont ses pieds. Voir `effects/doll_camera.h`.
//
// ⚠ LA PEINTURE SE FAIT PAR UN RAPPEL DE L'INTERFACE, pas ici : voir
// `ro::ui::DollOverlay`.
//
// ⚠ UNE INSTANCE PAR PANTIN MONTRÉ EN MÊME TEMPS : elle ne porte qu'un
// ensemble d'effets à la fois. Les aperçus de la boutique et de la description
// se partagent la leur — un seul se survole —, le portrait de statut, toujours
// à l'écran, a la sienne.
//
// ⚠ NI DÉPLACÉ NI COPIÉ : le moteur d'effets garde l'adresse du catalogue et de
// la troupe, et le rappel celle de l'objet.
class DollEffects {
 public:
  explicit DollEffects(ro::vfs::Vfs& vfs);
  DollEffects(const DollEffects&) = delete;
  DollEffects& operator=(const DollEffects&) = delete;
  DollEffects(DollEffects&&) = delete;
  DollEffects& operator=(DollEffects&&) = delete;
  ~DollEffects() = default;

  // LE PANTIN QUE LES EFFETS SUIVENT, à cette image.
  struct DollPose {
    // Où ses pieds tombent dans la boîte de `canvas`, et de combien il a grandi.
    ro::render::VignetteFraming framing;
    DollCanvas canvas;
    // Le corps qu'il montre : quand un effet change l'acteur, c'est lui que le
    // rappel repeint, à la place de l'image composée — voir
    // `ComposeChangedDollBody`. Sa tête déplacée emporte ce qui s'y pose.
    DollBodySource body;
    // LA DIRECTION DU .ACT QU'IL MONTRE : le porteur s'y tourne, et un effet
    // qui suit l'orientation de son porteur la suit.
    std::size_t sprite_direction = 0;
  };

  // FAIT VIVRE CES EFFETS DE CHAPEAU sur le pantin `doll`, et rend le rappel
  // qui les peindra. Rien sans effet connu, sans cadrage ou sans joueur : ce
  // qui jouait s'oublie alors, et le rappel rendu est vide.
  //
  // `worn` : RANGÉS ET SANS DOUBLON. Un ensemble qui change repart de zéro —
  // voir `DollWearing`.
  struct Shown {
    ro::ui::DollOverlay overlay;
    // L'image composée du pantin ne se pose pas : le rappel peint le corps.
    bool body_overlaid = false;
  };
  [[nodiscard]] Shown Advance(std::span<const ro::sim::HatEffectId> worn, const DollPose& doll,
                              const ro::sim::World& world, const MapStage& stage,
                              std::uint32_t now_ms);

  // L'APERÇU D'UNE PIÈCE : l'effet de CE RANG sur le pantin de `doll`, dans la
  // direction où la molette l'a tourné, et dont le rappel se pose sur lui. Rien
  // sans rang ou sans pantin.
  //
  // ⚠ APRÈS LE PANTIN, dont `framing` est le cadrage : c'est lui qui efface
  // chaque image le rappel de la précédente.
  void Advance(ro::ui::DescriptionDollState& doll, std::int16_t ordinal,
               const ro::render::VignetteFraming& framing, const DollBody* body,
               const ro::sim::World& world, const MapStage& stage, std::uint32_t now_ms);

  // Oublie ce qui joue : plus aucun pantin ne montre d'effet.
  void Rest();
  // OUBLIE LES TEXTURES téléversées : le skin a changé, et un même chemin
  // désigne une autre image. ⚠ ENTRE DEUX IMAGES.
  void ForgetTextures();

 private:
  // Le rappel de l'interface, et ce qu'il fait une fois son peintre retrouvé.
  static void Paint(const ImDrawList* list, const ImDrawCmd* command);
  void PaintInto(const ro::ui::DollOverlayCall& call);
  // Jette ce que la partie entendrait ou ferait trembler : un pantin est muet,
  // et son écran ne bouge pas. ⚠ CE QUE LES EFFETS FONT À L'ACTEUR, lui,
  // s'inscrit sur le porteur : c'est ce qui rapetisse, éclaire ou double le
  // pantin.
  void Drain(std::uint32_t now_ms);

  ro::render::EffectCatalog catalog_;
  EffectNodesInPlay nodes_;
  EffectsInPlay effects_;
  // Sans renderer — un pilote qui refuse le programme —, le pantin se montre
  // sans effet, comme la partie.
  std::optional<ro::render::EffectRenderer> effect_renderer_;
  std::optional<ro::render::SpriteRenderer> sprites_;
  std::optional<ro::render::GroundQuadRenderer> ground_quads_;
  // LE MONDE DU PORTEUR, et son planificateur : sans grille, il ne marche pas.
  ro::sim::World stand_;
  ro::sim::WalkPlanner planner_{nullptr};
  ro::render::Camera camera_;
  // LE DÉPLACEMENT DE LA TÊTE DU PORTEUR à cette image, sous son numéro : le
  // moteur garde l'adresse de la table, que `SeeScene` lui passe.
  std::map<std::uint32_t, glm::vec2> head_shifts_;
  // Ce que le moteur a rendu à cette image, peint par le rappel.
  std::span<const ro::render::DrawnEffect> played_;
  // LE CORPS DU PANTIN QUE SES EFFETS CHANGENT, rémanences en tête, peint par le
  // rappel derrière les effets de devant ; vide quand l'image composée suffit.
  // `body_` est la copie que `body_draws_` désigne.
  DollBody body_;
  std::vector<ro::render::DrawnActor> body_draws_;
  ActorGhosts ghosts_;
  std::vector<ro::render::ActorEffectPlay> bearer_effects_;
  // La carte où le porteur se tient, lue au moment de peindre les cases de
  // terrain. Nulle au repos.
  const MapStage* stage_ = nullptr;
  // Ce que le porteur porte, et où : vide au repos. Ce qui change repart de
  // zéro.
  DollWearing wearing_;
  // La toile de la dernière image, que le rappel retrouve.
  DollCanvas canvas_;
};

}  // namespace moonclient
