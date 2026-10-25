#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/camera.h"
#include "ro/render/floating_number.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_run.h"
#include "ro/vfs/vfs.h"

// LES CHIFFRES DE DÉGÂTS DE L'ARÈNE : un nombre par monstre touché, fusionné
// sur les coups rapprochés, coloré par l'efficacité du coup (U14).
namespace moonclient {

// Deux coups sur un même monstre à moins de cet écart ne font qu'un nombre.
inline constexpr std::uint64_t kArenaNumberMergeMs = 200;
// Au-delà, le plus petit nombre cède sa place.
inline constexpr std::size_t kMostArenaNumbers = 48;

// LES SEUILS DE LA COULEUR (U14), en efficacité du coup.
inline constexpr std::int32_t kStrongHitFromPct = 150;
inline constexpr std::int32_t kWeakHitUpToPct = 50;

inline constexpr std::array<std::uint8_t, 3> kStrongHitColor = {255, 150, 0};
inline constexpr std::array<std::uint8_t, 3> kPlainHitColor = {255, 255, 255};
inline constexpr std::array<std::uint8_t, 3> kWeakHitColor = {160, 160, 160};
inline constexpr std::array<std::uint8_t, 3> kHeroHurtColor = {255, 0, 0};
inline constexpr float kStrongHitScale = 1.5F;
inline constexpr float kPlainHitScale = 1.0F;
inline constexpr float kWeakHitScale = 0.7F;

// L'ALLURE D'UN NOMBRE : sa couleur, sa taille, et « Miss » pour zéro dégât.
struct ArenaNumberStyle {
  std::array<std::uint8_t, 3> color = kPlainHitColor;
  float scale = kPlainHitScale;
  bool miss = false;
  [[nodiscard]] bool operator==(const ArenaNumberStyle&) const = default;
};

// L'allure d'un coup porté (`hero_hurt` faux) ou encaissé par le héros.
[[nodiscard]] ArenaNumberStyle ArenaNumberStyleOf(std::int32_t amount, std::int32_t efficiency_pct,
                                                  bool hero_hurt) noexcept;

struct ArenaNumber {
  // Le monstre touché ; zéro pour le héros.
  std::uint32_t foe_serial = 0;
  bool hero_hurt = false;
  ro::sim::FieldPoint at;
  std::int32_t amount = 0;
  // La plus forte efficacité des coups fusionnés.
  std::int32_t efficiency_pct = 0;
  std::uint64_t born_ms = 0;
};

// LA RÉSERVE DES NOMBRES VIVANTS, sans GPU.
class ArenaNumberPool {
 public:
  // Un coup sur le monstre `foe_serial`, ou sur le héros si `hero_hurt`.
  void Add(std::uint32_t foe_serial, bool hero_hurt, ro::sim::FieldPoint at, std::int32_t amount,
           std::int32_t efficiency_pct, std::uint64_t now_ms);
  // Retire les nombres éteints à `now_ms`.
  void Expire(std::uint64_t now_ms);
  void Clear() noexcept { live_.clear(); }
  [[nodiscard]] std::span<const ArenaNumber> live() const noexcept { return live_; }

 private:
  std::vector<ArenaNumber> live_;
};

// LES CHIFFRES PEINTS : la réserve, composée par le sprite des chiffres du jeu
// dans des corps réutilisés, et peinte après la scène, sans épreuve de
// profondeur — un nombre ne passe jamais derrière un monstre.
class ArenaNumbers {
 public:
  // Le sol sous un point de la simulation, en coordonnées du monde.
  using GroundOf = std::function<glm::vec3(ro::sim::FieldPoint)>;

  // Lit le sprite des chiffres et celui des marques. Sans eux, aucun nombre ne
  // paraît, et le dire suffit : la partie se joue.
  [[nodiscard]] ro::core::Result<void> Open(ro::vfs::Vfs& vfs);

  // Les coups et les blessures de ces nouvelles, à `now_ms` de présentation.
  void Notice(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
              std::uint64_t now_ms);
  // Une partie neuve sur l'arène gardée : aucun nombre de la précédente.
  void Restart() noexcept;
  // Compose les nombres vivants à `now_ms`.
  void Compose(std::uint64_t now_ms, const GroundOf& ground_of);
  void Draw(ro::render::SpriteRenderer& sprites, const ro::render::Camera& camera,
            float aspect_ratio) const;

  [[nodiscard]] const ArenaNumberPool& pool() const noexcept { return pool_; }
  [[nodiscard]] std::span<const ro::render::DrawnActor> drawn() const noexcept { return drawn_; }

 private:
  ArenaNumberPool pool_;
  std::vector<ro::render::LoadedActorSprite> sprites_;
  std::vector<ro::render::ComposedActor> composed_;
  std::vector<ro::render::DrawnActor> drawn_;
};

}  // namespace moonclient
