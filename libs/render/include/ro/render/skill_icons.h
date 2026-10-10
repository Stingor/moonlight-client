#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/render/icon_texture.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LES ICÔNES DE COMPÉTENCE, LUES À LA DEMANDE.
//
// ⚠ PAS TOUTES AU DÉMARRAGE, et ce n'est pas une paresse : les tables du client
// portent mille huit cent quatre-vingt-sept compétences, et un grimoire n'en
// montre jamais plus d'une cinquantaine à la fois. Les lire toutes coûterait une
// seconde de démarrage pour des bitmaps que personne ne regardera.
//
// ⚠ ET UNE ABSENCE SE RETIENT. Une compétence dont l'installation n'a pas
// l'icône laisse une case vide ; sans la retenir, on redemanderait le même
// fichier manquant à chaque image, soixante fois par seconde.

class SkillIcons {
 public:
  // Le VFS est passé par référence et doit vivre plus longtemps : les icônes s'y
  // lisent au fil du jeu, pas une fois pour toutes.
  explicit SkillIcons(vfs::Vfs& vfs) : vfs_(&vfs) {}

  // L'icône d'une compétence, par son nom INTERNE. Rend une icône inutilisable
  // quand l'installation ne l'a pas — ce qui n'est pas une panne : le grimoire
  // montre alors une case sans image, et le niveau reste lisible.
  [[nodiscard]] const IconTexture& Get(std::uint16_t skill_id, std::string_view idname);

  [[nodiscard]] std::size_t loaded_count() const noexcept { return loaded_.size(); }

  // OUBLIE TOUT : le skin a changé. Les icônes se redemandent à chaque image.
  void Clear() { loaded_.clear(); }

 private:
  vfs::Vfs* vfs_ = nullptr;
  std::unordered_map<std::uint16_t, IconTexture> loaded_;
};

}  // namespace ro::render
