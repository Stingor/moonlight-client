#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "ro/ui/options_window.h"

#include "game/profile_keeper.h"
#include "game/settings_file.h"
#include "game/settings_profile_file.h"

namespace moonclient {

// ── LES PROFILS D'HABILLAGE : `savedata/skin_look_profiles.yml` ────────────
//
// Des habillages nommés — tout ce que porte `look.yaml` —, qu'on enregistre,
// qu'on pose, qu'on copie et qu'on supprime, comme les profils de réglages et
// par la même page.
//
// ⚠ COMMUNS À TOUS LES SKINS : un thème « Nuit » qu'on s'est fait se pose sur
// n'importe quel dossier. Le dossier du skin ne garde que l'habillage présent.
//
// ⚠ PAS DE PROFIL PAR PERSONNAGE : l'habillage habille aussi l'écran de
// connexion, avant tout personnage. LE DERNIER PROFIL POSÉ est retenu à la
// place, avec l'habillage du skin, et reposé au démarrage suivant.
inline constexpr std::string_view kSkinLookProfileFileName = "skin_look_profiles.yml";

[[nodiscard]] std::filesystem::path SkinLookProfilePath(const std::filesystem::path& client_root);

// L'habillage, vu comme une famille de profils.
class SkinLookPort final : public ProfilePort<SettingsValues> {
 public:
  SkinLookPort(std::filesystem::path profile_path, SettingsFile& look)
      : profile_path_(std::move(profile_path)), look_(look) {}

  [[nodiscard]] std::uint64_t Revision() override { return look_.revision(); }
  [[nodiscard]] SettingsValues Capture() override { return look_.SharedValues(); }
  void Pose(const SettingsValues& profile) override;
  [[nodiscard]] SettingsValues Defaults() override { return look_.SharedDefaults(); }
  [[nodiscard]] bool Load(SettingsProfileBook& into) override;
  void Save(const SettingsProfileBook& from) override;

 private:
  std::filesystem::path profile_path_;
  SettingsFile& look_;
};

// LES PROFILS D'HABILLAGE, VIVANTS. Au démarrage, le profil `remembered` est
// reposé : celui que le joueur avait posé en dernier. ⚠ RIEN QUAND IL N'Y EN A
// PAS : un skin qui n'a encore rien retenu garde son propre habillage, et
// « Default » ne vient pas le recouvrir.
class SkinLookProfiles {
 public:
  SkinLookProfiles(std::filesystem::path profile_path, SettingsFile& look,
                   std::string_view remembered)
      : port_(std::move(profile_path), look), keeper_(port_, &Text) {
    keeper_.Start(!remembered.empty(), remembered);
  }

  void Tick(std::uint32_t now_ms) { keeper_.Tick(now_ms); }
  void Flush() { keeper_.Flush(); }
  [[nodiscard]] ro::ui::OptionSectionBody Body() { return keeper_.Body(); }
  [[nodiscard]] const SettingsProfileBook& book() const noexcept { return keeper_.book(); }
  [[nodiscard]] const std::string& active() const noexcept { return keeper_.active(); }
  // Le geste de la page, appliqué au prochain `Tick`.
  void Ask(ro::ui::ProfilesPageAction asked) { keeper_.Ask(std::move(asked)); }

 private:
  [[nodiscard]] static ProfileFamilyText Text();

  SkinLookPort port_;
  ProfileKeeper<SettingsValues> keeper_;
};

}  // namespace moonclient
