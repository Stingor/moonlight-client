#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/image.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/map_emitter.h"
#include "ro/render/sprite_billboard.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// ── LA FUMÉE DES CHEMINÉES ──────────────────────────────────────────────────
//
// Les objets 44 du RSW — 152 sur prontera, 32 sur geffen, 33 sur morocc — sont
// l'effet de fumée du client : quatre particules d'un
// SPRITE qui montent en tournant, à la place de l'objet. La table `effecttool`
// de la carte n'y entre pas.

// Le sprite des cheminées, sans extension : un `.spr` et un `.act`.
//
// ⚠ « 이팩트 », avec un 팩 : c'est l'orthographe du dossier de l'archive et des
// octets du client (`C0 CC C6 D1 C6 AE`), non celle du mot usuel.
inline constexpr std::string_view kChimneySmokeSprite = "data\\sprite\\이팩트\\굴뚝연기";

// Combien de bouffées porte une cheminée.
inline constexpr std::size_t kChimneyPuffs = 4;

// L'IMAGE D'UNE BOUFFÉE : le premier calque de la première image du sprite,
// et l'image qu'il désigne.
struct ChimneySmokeLook {
  formats::ActLayer layer;
  formats::Image image;
};

// Charge le sprite des cheminées, ou rien quand l'installation ne l'a pas.
[[nodiscard]] std::optional<ChimneySmokeLook> LoadChimneySmoke(vfs::Vfs& vfs);

// La clé sous laquelle le renderer garde son image.
[[nodiscard]] const std::string& ChimneySmokeTextureKey();

// UNE CHEMINÉE : le portage de l'effet de fumée du client.
//
// 🔴 SES BOUFFÉES VIVENT PAR CYCLES DE CINQUANTE PAS, ENSEMBLE. Nées avec un âge
// nul, elles ne se montrent qu'à leur première renaissance, au pas cinquante :
// avant, le sprite n'est pas encore posé. Chaque renaissance les ramène au pied
// de la cheminée, les penche au hasard de 75 à 104 degrés sous la verticale et
// les fait monter de 0,3 à 0,5 unité par pas ; elles tournent de trois quarts
// de degré par pas, et s'effacent
// de quinze d'alpha par pas passé le trente-quatrième.
//
// ⚠ ET LE CLIENT N'EN PEINT QU'UNE PARTIE : à chaque renaissance, il tire
// combien des quatre se dessinent, entre une et quatre.
class MapChimney {
 public:
  // La cheminée d'un objet 44, à sa place dans le repère du client.
  //
  // ⚠ ET SON PORTEUR, zéro pour une cheminée du RSW. Celle qu'un effet annoncé
  // sur un acteur fait naître le SUIT — sa place se recale sur la sienne à
  // chaque passe — et meurt quand il n'est plus là : voir
  // `MapEffects::FollowOwners`.
  explicit MapChimney(glm::vec3 native_position, std::uint32_t owner_id = 0);

  // La recale sur la place de son porteur : ses bouffées suivent, chacune à
  // son écart.
  void MoveTo(glm::vec3 native_position) noexcept { position_ = native_position; }
  [[nodiscard]] std::uint32_t owner_id() const noexcept { return owner_id_; }

  // Un pas du jeu : le tri par distance, puis,
  // à portée, la mise à jour. Loin, ses bouffées se figent.
  // `reach_units` : la portée au-delà de laquelle elle se cache.
  void Tick(glm::vec3 native_player, std::uint32_t& draws,
            float reach_units = kMapEffectReachUnits);

  [[nodiscard]] bool drawn() const noexcept { return ticked_ && !hidden_; }

  // Les quads de ses bouffées visibles, dans NOTRE repère.
  void AppendQuads(const MapEffectFrame& frame, const BillboardBasis& basis,
                   const ChimneySmokeLook& look, std::vector<EffectWorldQuad>& quads) const;

  [[nodiscard]] glm::vec3 native_position() const noexcept { return position_; }
  // Combien de bouffées le client dessine à cet instant.
  [[nodiscard]] std::int32_t drawn_count() const noexcept { return drawn_count_; }

 private:
  struct Puff {
    // L'écart au pied de la cheminée, dans le repère du client.
    glm::vec3 offset{0.0F, 0.0F, 0.0F};
    // De combien elle monte par pas, le long de la troisième ligne de
    // `direction`.
    float rise = 0.0F;
    NativeMatrix direction = kNativeIdentity;
    float size = 1.0F;
    float roll_degrees = 0.0F;
    float roll_step_degrees = 0.0F;
    std::int32_t alpha = 255;
    std::int32_t age_steps = 0;
    // Faux tant que sa première renaissance n'a pas posé le sprite.
    bool dressed = false;
  };

  glm::vec3 position_{0.0F, 0.0F, 0.0F};
  std::uint32_t owner_id_ = 0;
  std::array<Puff, kChimneyPuffs> puffs_{};
  // Champ 204 : un au constructeur.
  std::int32_t drawn_count_ = 1;
  bool hidden_ = false;
  bool ticked_ = false;
};

}  // namespace ro::render
