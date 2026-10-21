#pragma once

#include <algorithm>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

#include "world/map_stage.h"

// OÙ LA CAMÉRA REGARDE À CETTE IMAGE, et ce qui la secoue.
//
// Deux choses la déplacent sans que le joueur y touche : le personnage qu'elle
// suit, et la SECOUSSE D'ÉCRAN qu'un effet demande. Elles vivent ensemble parce
// que la seconde s'applique PAR-DESSUS la première, et qu'il faut savoir si la
// première a replacé le point visé pour savoir si la seconde doit se retirer.
namespace moonclient {

// LA SECOUSSE D'ÉCRAN, celle qu'`EF_SCREEN_QUAKE` demande.
//
// C'est l'identifiant le plus émis du lot des effets d'acteur : les instances
// et les quêtes s'en servent pour faire trembler le sol. Il ne dessine rien —
// il envoie une COMMANDE au mode de jeu (0x7A), qui remue la caméra pendant six
// cent cinquante millisecondes.
//
// ⚠ ELLE NE S'ADDITIONNE PAS À ELLE-MÊME. Le client accumule son tremblement
// dans les champs de sa caméra et les restaure à la fin depuis la valeur qu'elle
// VISAIT ; la nôtre n'a pas de valeur visée, et un tremblement qui s'ajoute sans
// jamais se retrancher dériverait — la caméra reculerait et piquerait du nez
// pour de bon. L'objet retient donc ce qu'il a posé et le retire avant de poser
// la suite.
class ScreenShake {
 public:
  // Ce qui a été posé sur la caméra à l'image précédente, à retirer.
  [[nodiscard]] glm::vec3 look_at_offset() const noexcept { return look_at_offset_; }

  // Ce que la secousse a posé sur l'élévation à cette image, en degrés.
  [[nodiscard]] float pitch_offset() const noexcept { return pitch_offset_; }

  [[nodiscard]] bool shaking() const noexcept {
    return distance_offset_ != 0.0F || pitch_offset_ != 0.0F;
  }

  // Retire ce que l'image précédente avait posé sur le recul et l'élévation.
  // Le point visé se retire à part : le suivi du personnage le réécrit en
  // entier, et le retirer une seconde fois le déplacerait pour de vrai.
  void Undo(ro::render::Camera& camera);

  // Une compétence demande le tremblement jusqu'à cet instant.
  void Request(std::uint32_t until_ms) noexcept {
    requested_until_ms_ = std::max(requested_until_ms_, until_ms);
  }

  // Secoue la caméra si un effet en cours le demande, et ne fait rien sinon.
  //
  // 🔴 LE TREMBLEMENT SE CADENCE AU TEMPS, et non aux images : son signe
  // s'inverse et son bruit se retire soixante fois par seconde, quelle que soit
  // la cadence de l'écran. Un même instant donne la même secousse à 60, 144 ou
  // 240 images par seconde — c'est ce qui fait vibrer plutôt que dériver, sans
  // qu'un écran rapide ne la change en bourdonnement.
  void Apply(ro::render::Camera& camera, const ro::sim::World& world, std::uint32_t now_ms);

  // LES TREMBLEMENTS PERMIS, que `/quake` bascule. Faux : `Apply` ne pose
  // RIEN sur la caméra, quoi qu'un effet ou une compétence ait demandé, et la
  // demande en cours s'oublie au lieu de reprendre quand on les rallume.
  bool enabled = true;

 private:
  glm::vec3 look_at_offset_{0.0F, 0.0F, 0.0F};
  float distance_offset_ = 0.0F;
  float pitch_offset_ = 0.0F;
  std::uint32_t requested_until_ms_ = 0;
};

// LE SUIVI SOUPLE, celui que `/camera` débranche dans le client natif.
//
// ⚠ LA CAMÉRA NE COLLE PAS AU PERSONNAGE. Le client garde un point visé
// COURANT et un point visé CIBLE, et rapproche le premier du second d'un
// DIXIÈME à chaque image. Le réglage 0x36 — la commande `/camera` — supprime
// ce rattrapage et
// fait prendre la cible directement ; sa case est DÉCOCHÉE par défaut, et c'est
// le rattrapage qui donne à RO son flottement.
//
// Un client qui colle se remarque tout de suite : la carte part au quart de
// tour à chaque pas, et le jeu en devient fatigant.
class CameraTracking {
 public:
  // Rapproche le point visé de la cible, ou l'y pose d'un coup si elle a sauté
  // — ou si le suivi rigide est demandé.
  //
  // ⚠ LE RATTRAPAGE SE COMPTE EN TEMPS, pas en images. Le client, lui, retire un
  // dixième du retard à chaque image de SA boucle ; la nôtre tourne plus vite, et
  // reprendre son dixième tel quel rendrait la caméra presque collée.
  void Follow(ro::render::Camera& camera, const glm::vec3& target, std::uint32_t now_ms);

  // OUBLIE OÙ LA CAMÉRA REGARDAIT : la prochaine image s'y posera au lieu d'y
  // glisser.
  //
  // 🔴 C'EST LE CHANGEMENT DE CARTE QUI L'APPELLE, et il le DOIT. Le seuil de
  // `Follow` se dérive de ce qu'une MARCHE accumule comme retard ; il ne peut
  // donc rien dire d'un décor entièrement remplacé, où les coordonnées
  // d'arrivée sont souvent voisines de celles du départ. Sans cette remise à
  // zéro, la caméra glisserait à travers la carte neuve pendant un dixième de
  // seconde. Voir TeleportUnits, dans camera_chase.h.
  void Reset() noexcept { started_ = false; }

  // Le suivi RIGIDE, que `/camera` bascule. Faux par défaut, comme le réglage
  // du jeu : la caméra rattrape.
  bool fixed = false;

 private:
  glm::vec3 look_at_{0.0F, 0.0F, 0.0F};
  std::uint32_t last_ms_ = 0;
  bool started_ = false;
};

// Ce que la caméra garde d'une image à l'autre : où elle en est de son
// rattrapage, et ce que la secousse lui avait posé.
struct CameraAim {
  CameraTracking tracking;
  ScreenShake shake;
};

// Place la caméra pour cette image : elle rattrape le personnage, puis tremble.
//
// Elle vise sa position INTERPOLÉE, la même que celle où son sprite est peint :
// viser la case d'arrivée ferait sauter la carte d'un demi-pas à chaque cellule
// franchie, et viser la case de départ la ferait traîner.
void AimCamera(ro::render::Camera& camera, const MapStage& stage, const ro::sim::World& world,
               ro::sim::WalkPlanner& planner, std::uint32_t now_ms, CameraAim& aim);

}  // namespace moonclient
