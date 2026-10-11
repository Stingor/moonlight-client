#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// UNE FAMILLE DE PROFILS NOMMÉS, sous les réglages d'une page d'options :
// choisir le profil du personnage, l'enregistrer sous un autre nom, en
// dupliquer, renommer, réinitialiser ou supprimer un.
//
// ⚠ ELLE NE POSSÈDE AUCUN PROFIL. Elle montre les noms qu'on lui passe et rend
// le geste du joueur, PAR NOM ; c'est l'appelant qui pose et qui écrit.

// Un nom de profil tient dans ce nombre d'octets, zéro compris.
inline constexpr std::size_t kProfileNameBytes = 33;

struct ProfilesPageView {
  // Le titre, l'aide et l'identité ImGui de la famille, déjà traduits.
  const char* title = "";
  const char* help = "";
  const char* id = "";
  // Les noms des profils — ce que les gestes rendent —, et leurs libellés
  // montrés, au même rang.
  std::span<const std::string> names;
  std::span<const char* const> labels;
  // Le rang du profil posé.
  int active = 0;
  // Le rang du profil qui ne se renomme ni ne se supprime.
  int fixed = 0;
  // Le personnage en jeu, ou nul hors partie : le choix d'un profil ne se
  // retient alors pour personne.
  const char* character = nullptr;
  // Plus aucun profil ne peut naître.
  bool full = false;
};

enum class ProfileGesture : std::uint8_t {
  kNone,
  // Pose le profil `target`, et le retient pour le personnage.
  kLoad,
  // Enregistre l'état présent sous `new_name`, et le pose.
  kSaveAs,
  // Copie le profil `target` sous `new_name`, sans le poser.
  kDuplicate,
  // Renomme le profil `target` en `new_name`.
  kRename,
  // Remet le profil `target` aux défauts du client.
  kReset,
  // Écrase le profil `target` par l'état présent, sans le poser.
  kOverwrite,
  // Supprime le profil `target`.
  kDelete,
};

struct ProfilesPageState {
  // Le profil désigné dans la liste, qui peut n'être pas le posé ; moins un
  // pour « celui qui est posé ».
  int selected = -1;
  std::array<char, kProfileNameBytes> name{};
  // Le geste qui attend sa confirmation : `kReset`, `kOverwrite` ou `kDelete`,
  // sinon rien.
  ProfileGesture confirming = ProfileGesture::kNone;
};

struct ProfilesPageAction {
  ProfileGesture gesture = ProfileGesture::kNone;
  std::string target;
  std::string new_name;
};

[[nodiscard]] ProfilesPageAction DrawProfilesPage(const ProfilesPageView& view,
                                                  ProfilesPageState& state, const SkinArt& art);

}  // namespace ro::ui
