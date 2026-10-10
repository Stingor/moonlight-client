#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>

#include "ro/proto/common_packets.h"
#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/sim/style.h"
#include "ro/ui/character_seats.h"
#include "ro/ui/character_select_window.h"
#include "ro/ui/seat_layout.h"

#include "world/actor_styles.h"

namespace moonclient {

// LES CONVIVES DU BANQUET : vingt-cinq pantins, composés et téléversés.
//
// ⚠ C'EST `app` QUI LES TIENT, et il ne pouvait en être autrement : la liste
// arrive en `proto`, les sprites vivent dans `render`, et la fenêtre qui les
// pose est dans `ui`, qui ne connaît ni l'un ni l'autre. Cette classe est la
// couture — elle lit une ligne de liste et rend une image.
//
// ⚠ UN PANTIN PAR SIÈGE, ET NON PAR PERSONNAGE : une vignette ne retient
// qu'une texture, et c'est le SIÈGE qui est le point fixe de la scène. Tourner
// la page fait changer d'occupant sans changer de place, et la vignette se
// recompose alors comme elle le ferait pour un changement d'apparence.
class BanquetDolls {
 public:
  explicit BanquetDolls(ro::render::ActorWardrobe& wardrobe) : wardrobe_(&wardrobe) {}
  // ⚠ REND À LA GARDE-ROBE LES STYLES DE LA TABLÉE : elle est partagée avec le
  // jeu qui suit, et ces identifiants-là n'y désigneraient plus personne.
  ~BanquetDolls();
  BanquetDolls(const BanquetDolls&) = delete;
  BanquetDolls& operator=(const BanquetDolls&) = delete;

  // LES COULEURS DES CONVIVES, par identifiant de personnage — celles que le
  // jeu a gardées pour la tablée, voir `character/character_styles.h`.
  void UseStyles(const std::map<std::uint32_t, ro::sim::StyleVariants>& kept);

  // UN CONVIVE DESSINÉ : son image, et le nombre d'images de l'action qu'il
  // joue — le menu de pose ne propose que celles qui existent.
  struct Seated {
    ro::ui::SkinImage image;
    int frames = 0;
  };

  // LE PANTIN DE CE SIÈGE, prêt à poser, dans la pose que le siège demande.
  //
  // Rend une image inutilisable tant que les sprites ne répondent pas — la
  // scène montre alors sa silhouette d'attente, qui ne ment pas.
  [[nodiscard]] Seated Seat(int seat, const ro::proto::CharacterInfo& character,
                            std::uint8_t account_sex, const ro::ui::CharacterSeat& place,
                            const ro::ui::SeatFrame& frame, std::uint32_t elapsed_ms);

 private:
  // LA HAUTEUR D'UN PERSONNAGE STANDARD, en pixels de sprite, pour cette
  // action. Zéro tant que les sprites ne répondent pas.
  //
  // 🔴 C'EST ELLE QUI DONNE À LA TABLÉE UNE ÉCHELLE COMMUNE. `CharacterSeat`
  // dit la hauteur d'un personnage STANDARD à ce siège ; pour la traduire en
  // échelle, il faut savoir ce que « standard » mesure. On le MESURE, sur un
  // novice masculin nu, dans la pose du siège : ainsi la hauteur d'un siège
  // garde le même sens quel que soit le corps qui l'occupe.
  //
  // ⚠ DANS LA POSE DU SIÈGE, et non debout : un personnage assis est bien plus
  // court, et le rapporter à une référence debout rapetisserait tout le
  // banquet d'un quart. ⚠ MAIS TOUJOURS DE FACE : prendre l'orientation du
  // siège ferait varier l'échelle quand on tourne le convive à la molette.
  [[nodiscard]] float StandardSpanUnits(int animation);

  ro::render::ActorWardrobe* wardrobe_;
  // Le livre des couleurs, par identifiant de PERSONNAGE et non d'acteur : la
  // tablée n'a pas d'acteurs. Et le registre qui les pose, à part de celui du
  // jeu.
  ro::sim::StyleBook styles_;
  ActorStyles dressing_;
  std::array<std::optional<ro::render::ActorVignette>, ro::ui::kSeatCount> puppets_;
  // Une mesure par type d'action : chère, et elle ne change jamais.
  std::array<float, ro::ui::kSeatAnimationTypes> standard_span_units_ = {};
};

}  // namespace moonclient
