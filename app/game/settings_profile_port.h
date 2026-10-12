#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "game/profile_keeper.h"
#include "game/settings_file.h"
#include "game/settings_profile_file.h"
#include "input/hotkeys.h"

namespace moonclient {

// LES TOUCHES DANS UN PROFIL DE RÉGLAGES : une clé par raccourci, ce préfixe
// suivi du nom gravé de l'action — « hotkey_open_inventory ».
//
// ⚠ LA VALEUR EST UN SCALAIRE, comme celle de tout réglage : les modificateurs
// tenus, puis le nom de la touche — « ctrl+alt+F1 ». Une valeur vide retire le
// raccourci.
//
// ⚠ CE PRÉFIXE EST RÉSERVÉ : aucun réglage de `settings.yml` ne doit le porter.
inline constexpr std::string_view kProfileHotkeyPrefix = "hotkey_";

// LE PROFIL DE RÉGLAGES, sans ce qui pousse les valeurs dans le jeu : ce que
// `settings.yml` porte hors de la machine, ET les touches.
//
// 🔴 LES TOUCHES SUIVENT LE PROFIL, et plus le fichier commun `hotkeys.yml` :
// deux personnages sur un même profil ont les mêmes touches, et un personnage
// sur un autre profil a les siennes.
//
// ⚠ `hotkeys.yml` NE S'ÉCRIT PLUS, il se lit encore : c'est ce qu'un profil
// SANS TOUCHES hérite — le profil écrit avant que les touches n'y entrent. Ses
// touches sont celles que le joueur avait, et non celles d'origine.
class SettingsProfilePort : public ProfilePort<SettingsValues> {
 public:
  // `hotkeys` : la table vivante, que poser un profil remplace.
  SettingsProfilePort(std::filesystem::path client_root, SettingsFile& file, Hotkeys& hotkeys);

  // ⚠ LA SOMME DES DEUX RÉVISIONS : chacune ne fait que grandir, donc la somme
  // aussi, et une touche changée réveille le gardien comme un réglage.
  [[nodiscard]] std::uint64_t Revision() override;
  [[nodiscard]] SettingsValues Capture() override;
  void Pose(const SettingsValues& profile) final;
  [[nodiscard]] SettingsValues Defaults() override;
  // ⚠ UN PROFIL SANS TOUCHES REÇOIT CELLES DE `hotkeys.yml` dès la lecture :
  // dupliqué ou écrasé ailleurs, il les emporte avec lui.
  [[nodiscard]] bool Load(SettingsProfileBook& into) override;
  void Save(const SettingsProfileBook& from) override;

 protected:
  // APRÈS QUE LES VALEURS SONT POSÉES, avant que le fichier vivant ne suive :
  // ce que l'appelant pousse dans le jeu.
  virtual void AfterAssign() {}

 private:
  std::filesystem::path client_root_;
  SettingsFile& file_;
  Hotkeys& hotkeys_;
  // Les touches de `hotkeys.yml`, lues une fois.
  Hotkeys inherited_hotkeys_;
};

}  // namespace moonclient
