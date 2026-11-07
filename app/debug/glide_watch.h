#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/server_clock.h"
#include "ro/sim/world_fwd.h"

namespace moonclient {

// CE QUE LE PERSONNAGE MONTRE À UNE IMAGE : où il se tient, et quelle image de
// quelle motion il joue. `held_frame` est vide pour une motion qui se déroule.
struct ShownStep {
  float cell_x = 0.0F;
  float cell_y = 0.0F;
  std::string_view motion;
  std::size_t animation = 0;
  std::optional<std::size_t> held_frame;
  // Ce que le chemin dit à cet instant : l'acteur marche-t-il encore ?
  bool walking = false;
  // Un glissement voulu, celui de Body Relocation : il tient une image exprès.
  bool glide_wanted = false;
  float travelled_cells = 0.0F;
  std::uint32_t now_ms = 0;
};

// Ce qu'une image a changé à l'épisode en cours.
enum class GlideEvent : std::uint8_t { kNone, kBegan, kEnded };

// LA GLISSADE : le personnage avance de plusieurs cases sans que son image change.
//
// Une marche change d'image environ toutes les demi-cases, si bien que trois
// cases sous la même image ne se produisent pas par hasard. Le veilleur ne
// corrige rien : il désigne l'instant où relever l'état de l'acteur.
class GlideWatch {
 public:
  // Au-delà, l'image a tenu trop longtemps pour une marche.
  static constexpr float kGlideCells = 3.0F;
  // Au-delà en une seule image, c'est un téléport : aucune marche ne couvre cela.
  static constexpr float kLargestStepCells = 15.0F;
  // Un défaut durable reviendrait à chaque clic : le journal n'en garde que le début.
  static constexpr std::uint32_t kMostLoggedEpisodes = 20;

  GlideEvent Observe(const ShownStep& step);

  [[nodiscard]] const std::optional<ShownStep>& last() const noexcept { return last_; }
  [[nodiscard]] bool gliding() const noexcept { return gliding_; }
  // Depuis le dernier changement d'image.
  [[nodiscard]] float cells_on_same_image() const noexcept { return cells_; }
  [[nodiscard]] std::uint32_t same_image_since_ms() const noexcept { return since_ms_; }
  [[nodiscard]] std::uint32_t episodes() const noexcept { return episodes_; }
  // L'épisode que l'événement vient de clore : sa distance et sa durée.
  [[nodiscard]] float ended_cells() const noexcept { return ended_cells_; }
  [[nodiscard]] std::uint32_t ended_ms() const noexcept { return ended_ms_; }

 private:
  std::optional<ShownStep> last_;
  bool gliding_ = false;
  float cells_ = 0.0F;
  std::uint32_t since_ms_ = 0;
  std::uint32_t episodes_ = 0;
  float ended_cells_ = 0.0F;
  std::uint32_t ended_ms_ = 0;
};

// Un fait sur la marche d'un acteur, déjà écrit : le journal les aligne sur une
// ligne, la console en fait un objet.
struct WalkFact {
  std::string_view key;
  std::string value;
};

// TOUT CE QUI DÉCIDE DE LA MOTION ET DE L'IMAGE, à cet instant.
[[nodiscard]] std::vector<WalkFact> WalkFactsOf(const ro::sim::Actor& actor,
                                                const ro::sim::ServerClock& clock,
                                                std::uint32_t now_ms);

// « motion=kWalk image=0 … »
[[nodiscard]] std::string DescribeStep(const ShownStep& step);
[[nodiscard]] std::string JoinFacts(const std::vector<WalkFact>& facts);

// Montre l'image composée au veilleur, et écrit au journal ce qu'il en conclut.
void NoteShownStep(GlideWatch& watch, const ro::sim::World& world,
                   const std::optional<ShownStep>& step);

}  // namespace moonclient
