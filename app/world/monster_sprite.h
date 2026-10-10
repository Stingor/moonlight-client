#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/ui/skin_art.h"
#include "ro/vfs/vfs.h"

#include "world/monster_model.h"

namespace moonclient {

// ── LE SPRITE DE LA FICHE D'UN MONSTRE, QUI SE MANIPULE ─────────────────────
//
// La molette le TOURNE : on prend la pose orientée du .act, jamais un miroir,
// qui mettrait l'ombre du mauvais côté. Un clic le TITILLE : il encaisse,
// riposte une fois sur trois, et fait le mort à la sixième — puis se relève, et
// le compte repart de zéro.
//
// 🔴 LE SON APPARTIENT À L'IMAGE, PAS À L'ACTION. Chaque image d'un .act porte
// un son, ou aucun ; une attaque pose son wav sur son image de CONTACT. C'est
// donc le DESSIN qui déclenche le son, quand il atteint l'image qui le porte —
// et jamais l'animation d'attente, dont certains .act portent aussi un son :
// la fiche se mettrait à couiner seule, en boucle.
class MonsterSprite {
 public:
  // `models` : de quoi montrer un acteur 3D. Nul, un tel acteur se dit
  // « modèle 3D » sans image — c'est le cas de l'aperçu au survol.
  explicit MonsterSprite(ro::render::ActorWardrobe& wardrobe, ro::vfs::Vfs* models = nullptr)
      : wardrobe_(&wardrobe) {
    if (models != nullptr) {
      model_.emplace(*models);
    }
  }

  // Ce que la fiche montre à cette image.
  struct Shown {
    ro::ui::SkinImage image;
    // Le sursaut du monstre titillé, en pixels de sprite, vers la droite.
    float shake_px = 0.0F;
    // Ni `.spr` ni `.act` à montrer.
    bool missing = false;
    // Et c'est parce que la classe se dessine en modèle 3D.
    bool model = false;
  };

  // Où et comment le montrer.
  struct Framing {
    std::uint32_t sprite_class = 0;
    // L'orientation du .act, 0 de face, dans le sens des aiguilles d'une montre.
    int direction = 0;
    // Faire tourner l'attente, ou tenir sa première image comme le jeu.
    bool animate = true;
    int box_width = 0;
    int box_height = 0;
    // L'accessoire d'un pet, tel que son acteur le porte ; zéro hors d'un pet
    // ou sans accessoire. Il change le .act du corps.
    std::uint16_t pet_accessory = 0;
    // Comment la silhouette tombe dans sa boîte : debout par défaut, centrée
    // sans agrandir pour une case de liste. Voir `ro::render::VignetteFit`.
    ro::render::VignetteFit fit = ro::render::VignetteFit::kStanding;
  };

  // Change de monstre, ou d'accessoire de pet. ⚠ LA RÉACTION EN VOL SE PERD :
  // son action désigne une pose de l'ANCIEN .act. L'orientation, elle,
  // appartient à la fenêtre.
  void Show(std::uint32_t sprite_class, std::uint16_t pet_accessory = 0);

  // Le clic. ⚠ IGNORÉ PENDANT LE TEMPS MORT, et en silence : c'est une
  // réaction, pas une commande.
  void Poke(int direction, std::uint32_t now_ms);

  // L'image de cette image de jeu, et les sons qu'elle vient d'atteindre.
  [[nodiscard]] Shown Compose(const Framing& framing, std::uint32_t now_ms);

  // Les wav à jouer, relevés par `Compose` — chemins relatifs au dossier des
  // sons, en UTF-8. Vidés par qui les joue.
  [[nodiscard]] std::vector<std::string> TakeSounds();

  // Tout oublier : changement de carte.
  void Forget();

 private:
  // Une animation du .act, et sa direction.
  struct Pose {
    std::size_t animation = 0;
    std::size_t direction = 0;
  };

  [[nodiscard]] const ro::formats::ActAnimation* BodyAnimation();
  // La pose demandée si le .act la porte, l'attente dans la même direction
  // sinon, et l'action zéro en dernier recours — la seule qui existe forcément.
  [[nodiscard]] Pose PoseFor(std::size_t animation, int direction);
  [[nodiscard]] const ro::formats::ActAction* ActionOf(const Pose& pose);
  // Joue les sons de l'image `frame` de la réaction, si elle vient d'y arriver.
  void HearFrame(const ro::formats::ActAction& action, std::size_t frame);
  // Le sursaut de la réaction à cet instant : il s'éteint avec elle.
  [[nodiscard]] float ShakeAt(std::uint32_t now_ms) const;
  // Un acteur 3D : l'image de son modèle, rendue pour ce cadre.
  [[nodiscard]] Shown ComposeModel(const Framing& framing, std::uint32_t now_ms);

  ro::render::ActorWardrobe* wardrobe_;
  std::optional<ro::render::ActorVignette> vignette_;
  std::optional<MonsterModel> model_;
  // L'acteur est un modèle 3D : la chatouille n'a pas d'action à jouer.
  bool is_model_ = false;
  std::uint32_t sprite_class_ = 0;
  std::uint16_t pet_accessory_ = 0;
  // L'ÉCHELLE QUE L'ATTENTE A PRISE POUR TENIR DANS LE CADRE, zéro tant
  // qu'aucune n'est dessinée. Imposée pendant une réaction : son enveloppe
  // n'est pas celle de l'attente, et l'ajuster à part ferait changer le
  // monstre de taille sous le clic.
  float idle_scale_ = 0.0F;

  // ── LA RÉACTION EN COURS ──────────────────────────────────────────────────
  std::optional<Pose> poke_;
  std::uint32_t poke_start_ms_ = 0;
  // La fin de l'ANIMATION : elle ne boucle pas.
  std::uint32_t poke_anim_end_ms_ = 0;
  // La fin de la RÉACTION : celle de l'animation, sauf pour la mort, où le
  // monstre reste à terre avant de se relever.
  std::uint32_t poke_end_ms_ = 0;
  bool poke_frozen_ = false;
  // L'image tenue pendant la mort : la dernière VISIBLE, car une animation de
  // mort se termine sur des images vides — le corps disparaît.
  std::size_t poke_freeze_frame_ = 0;
  // Le prochain clic accepté : sans ce temps mort, un clic répété relance
  // l'animation à chaque image et empile les sons.
  std::uint32_t poke_ready_at_ms_ = 0;
  int poke_count_ = 0;
  // La dernière image jouée de la réaction : le son part au CHANGEMENT d'image.
  std::optional<std::size_t> poke_frame_;
  // Le son de coup de repli attend l'image que le .act marque comme le coup.
  bool hit_pending_ = false;

  std::vector<std::string> sounds_;
};

}  // namespace moonclient
