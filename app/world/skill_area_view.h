#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/formats/image.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/render/skill_area.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/vfs.h"

#include "world/map_stage.h"

namespace moonclient {

// LA ZONE DES SORTS, posée sur le relief.
//
// Elle réunit les deux moitiés que rien d'autre ne tient ensemble : la figure
// (`ro::render::skill_area.h`) sait quelles cases peindre et ignore où elles
// sont ; la carte sait où sont les cases et ignore tout des sorts.
//
// ⚠ LA TAILLE NE VIENT PAS DU PAQUET. Le serveur n'annonce que la compétence,
// son rang et la case visée ; c'est la fiche du client qui dit combien de cases
// — voir `ro::sim::SkillScaleAt`. Sans arbre des compétences, aucune zone ne se
// dessine, et c'est le cas normal d'un client qui n'a pas fini de charger.
class SkillAreas {
 public:
  explicit SkillAreas(ro::vfs::Vfs& vfs) : vfs_(&vfs) {}

  // TOUT CE QUI SE PEINT CETTE IMAGE, les deux moments d'un seul tenant.
  //
  // 🔴 UN SEUL APPEL, ET C'EST UNE CONTRAINTE. Les deux moments partagent le
  // tampon des quads : les composer par deux appels laisserait le premier span
  // désigner un vecteur que le second a réalloué. Ils partagent aussi le motif,
  // donc l'image — un seul lot, donc un seul appel de dessin.
  //
  //   · l'INCANTATION : ce que le serveur annonce, pour tout le monde autour.
  //     ⚠ Elle s'ÉTEINT D'ELLE-MÊME à l'échéance — le serveur donne une durée
  //     et ne redit rien à la fin, exactement comme pour la barre ;
  //   · le SORT ARMÉ : ce que le joueur tient en main et n'a pas encore lancé.
  //     Aucun paquet n'existe alors, personne d'autre ne le voit, et le client
  //     natif ne sait pas le montrer.
  //
  // `armed` peut être nul — rien en main —, et `hovered_cell` vide quand le
  // curseur ne désigne aucune case. Le résultat vit jusqu'au prochain appel.
  [[nodiscard]] std::span<const ro::render::DrawnGroundQuads> Areas(
      const ro::render::SkillAreaLook& look, const ro::sim::World& world,
      const ro::sim::SkillTree* skills, const MapStage& stage, std::uint32_t now_ms,
      const ro::sim::KnownSkill* armed, std::optional<ro::sim::CellPosition> hovered_cell);

 private:
  // Pousse les cases d'une zone centrée sur `centre`. Sans effet quand la
  // compétence n'en déclare aucune à ce rang, ce qui est le cas ordinaire.
  void PushArea(const ro::render::SkillAreaLook& look, const ro::sim::SkillTree& skills,
                const MapStage& stage, std::uint16_t skill_id, std::uint16_t level,
                ro::sim::CellPosition centre);

  // L'image d'un motif, lue une fois. ⚠ L'ÉCHEC SE RETIENT AUSSI : sans cela on
  // relirait l'archive à chaque image pour un fichier qui n'y est pas.
  [[nodiscard]] const ro::formats::Image* TextureFor(std::string_view path);

  ro::vfs::Vfs* vfs_ = nullptr;
  std::map<std::string, std::optional<ro::formats::Image>, std::less<>> textures_;
  std::vector<ro::render::GroundQuad> quads_;
  std::vector<ro::render::DrawnGroundQuads> lots_;
  // Le chemin que le lot désigne doit survivre à l'appel : le renderer indexe
  // son téléversement dessus.
  std::string texture_path_;
};

}  // namespace moonclient
