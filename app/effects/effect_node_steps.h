#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_node_pool.h"

#include "effects/effect_screen_node_in_play.h"

// LES PAS FIXES DE LA TROUPE PROCÉDURALE, et leur LISSAGE.
//
// La troupe bat par tics de seize millisecondes (`kEffectTickMs`), et chaque
// image fait battre les tics dus depuis la précédente avant de composer.
//
// LE LISSAGE, éteint par défaut, ne touche pas à ces tics : il garde une copie
// de la troupe ET DES NŒUDS D'ÉCRAN telle qu'elle était AVANT le dernier tic,
// la compose à son tour avec la caméra de l'image, et mêle les deux
// compositions dans la part du tic déjà écoulée — voir
// `ro/render/effect_step_blend.h` pour ce qui se mêle et ce qui ne se mêle pas.
// Un nœud se reconnaît d'un tic à l'autre par son numéro
// (`PooledEffectNode::pool_serial`, `EffectScreenNodeInPlay::serial`).
//
// Trois sorties se mêlent ainsi : les quads et les sprites du monde, les cases
// de terrain de la grille de visée, et les quads d'écran.
//
// ⚠ UN NŒUD QUI BAT À CHAQUE IMAGE (`EffectBeatsEveryFrame`) NE SE MÊLE PAS :
// il avance déjà à chaque image, et le tirer vers le tic d'avant le ferait
// reculer.
// ⚠ UN NŒUD ACCROCHÉ À UN PORTEUR qui marche est emmené à chaque image, entre
// deux tics : sa copie d'avant le tic est déplacée d'autant avant d'être mêlée,
// sans quoi il traînerait derrière lui. À l'écran, le déplacement est celui du
// porteur projeté.
// ⚠ LES COORDONNÉES DE TEXTURE DE LA GRILLE DE VISÉE SE MÊLENT, seules de leur
// espèce : elles ne choisissent pas une image d'une planche, elles TOURNENT
// d'un degré par tic autour de son centre, et c'est tout son mouvement.
namespace moonclient {

class EffectNodesInPlay;

class EffectNodeSteps {
 public:
  // Allume ou éteint le lissage. Éteint, la copie se libère et la composition
  // est celle d'un client sans lissage.
  void Enable(bool on) noexcept { enabled_ = on; }
  [[nodiscard]] bool enabled() const noexcept { return enabled_; }

  // FAIT BATTRE LA TROUPE des tics dus depuis la dernière image, et garde la
  // copie d'avant le dernier quand le lissage est allumé. Rend le nombre de tics.
  std::int32_t Beat(EffectNodesInPlay& view, std::uint32_t now_ms,
                    const ro::render::Camera& camera);

  // CE QU'UN NŒUD A COMPOSÉ dans le monde : ses lots de quads, ses poses de
  // sprite et ses cases de terrain, retenus de la construction à la
  // destruction de ce témoin. Il se pose en tête du tour de boucle qui compose
  // le nœud, et rien n'est retenu lissage éteint.
  class NodeOutputs {
   public:
    NodeOutputs(EffectNodeSteps& steps, const EffectNodesInPlay& view,
                const ro::render::PooledEffectNode& node);
    ~NodeOutputs();
    NodeOutputs(const NodeOutputs&) = delete;
    NodeOutputs& operator=(const NodeOutputs&) = delete;
    NodeOutputs(NodeOutputs&&) = delete;
    NodeOutputs& operator=(NodeOutputs&&) = delete;

   private:
    EffectNodeSteps* steps_;
    const EffectNodesInPlay* view_;
  };

  // LE MÊME TÉMOIN POUR LES QUADS D'ÉCRAN, d'un nœud de la troupe — le voile,
  // la nappe — ou d'un nœud d'écran.
  class ScreenOutputs {
   public:
    ScreenOutputs(EffectNodeSteps& steps, const EffectNodesInPlay& view,
                  const ro::render::PooledEffectNode& node);
    ScreenOutputs(EffectNodeSteps& steps, const EffectNodesInPlay& view,
                  const EffectScreenNodeInPlay& node);
    ~ScreenOutputs();
    ScreenOutputs(const ScreenOutputs&) = delete;
    ScreenOutputs& operator=(const ScreenOutputs&) = delete;
    ScreenOutputs(ScreenOutputs&&) = delete;
    ScreenOutputs& operator=(ScreenOutputs&&) = delete;

   private:
    EffectNodeSteps* steps_ = nullptr;
    const EffectNodesInPlay* view_;
  };

  // MÊLE la composition de l'image à celle de la troupe d'avant le dernier tic.
  // S'appelle une fois les nœuds composés, avant que les poses ne deviennent
  // des acteurs à peindre. Ne fait rien lissage éteint.
  void Smooth(EffectNodesInPlay& view, std::uint32_t now_ms, const ro::render::Camera& camera);

  // LA MÊME CHOSE POUR LES QUADS D'ÉCRAN, avant que leurs lots ne se recollent.
  void SmoothScreen(EffectNodesInPlay& view, const ro::render::Camera& camera, float viewport_width,
                    float viewport_height, bool dark_veil);

  // Oublie la copie : la troupe vient d'être vidée.
  void Forget();

  // La part du tic écoulée à cette image : voir `ro::render::EffectStepAlpha`.
  [[nodiscard]] float alpha() const noexcept { return alpha_; }

 private:
  // Les sorties d'un nœud, en rangs dans les listes de la vue.
  struct NodeSpan {
    std::uint64_t serial = 0;
    glm::vec3 anchor_ground_position{0.0F, 0.0F, 0.0F};
    bool beats_every_frame = false;
    std::size_t first_lot = 0;
    std::size_t end_lot = 0;
    std::size_t first_pose = 0;
    std::size_t end_pose = 0;
    std::size_t first_cells = 0;
    std::size_t end_cells = 0;
  };
  // Les quads d'écran d'un nœud. La clé distingue la troupe des nœuds d'écran,
  // dont les numéros se comptent à part.
  struct ScreenSpan {
    std::uint64_t key = 0;
    glm::vec3 bearer_ground_position{0.0F, 0.0F, 0.0F};
    bool beats_every_frame = false;
    std::size_t first_lot = 0;
    std::size_t end_lot = 0;
  };

  void OpenNode(const EffectNodesInPlay& view, const ro::render::PooledEffectNode& node);
  void CloseNode(const EffectNodesInPlay& view);
  void OpenScreen(const EffectNodesInPlay& view, std::uint64_t key, glm::vec3 bearer,
                  bool beats_every_frame);
  void CloseScreen(const EffectNodesInPlay& view);
  // Numérote les nœuds d'écran qui ne le sont pas encore.
  void IdentifyScreenNodes(EffectNodesInPlay& view);
  [[nodiscard]] std::vector<NodeSpan>& Recording() noexcept {
    return composing_previous_ ? previous_spans_ : spans_;
  }
  [[nodiscard]] std::vector<ScreenSpan>& ScreenRecording() noexcept {
    return composing_previous_ ? previous_screen_spans_ : screen_spans_;
  }

  bool enabled_ = false;
  // VRAI LE TEMPS DE COMPOSER LA COPIE : la composition repasse alors par
  // `EffectNodesInPlay::Drawn` ou `ScreenQuads`, qui ne doivent ni battre ni
  // lisser.
  bool composing_previous_ = false;
  bool has_previous_ = false;
  float alpha_ = 0.0F;
  std::uint64_t last_screen_serial_ = 0;
  ro::render::EffectNodePool previous_pool_;
  std::vector<EffectScreenNodeInPlay> previous_screen_nodes_;
  std::vector<NodeSpan> spans_;
  std::vector<NodeSpan> previous_spans_;
  std::vector<ScreenSpan> screen_spans_;
  std::vector<ScreenSpan> previous_screen_spans_;
};

}  // namespace moonclient
