#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string_view>

#include "ro/ui/options_window.h"

#include "game/profile_keeper.h"
#include "game/settings_file.h"
#include "game/settings_profile_file.h"

namespace moonclient {

class GameOptions;
struct OptionTargets;

// LES RÉGLAGES, vus comme une famille de profils : ceux de `settings.yml`
// hors de la machine. Voir `SettingsFile::MachineScope`.
class SettingsPort final : public ProfilePort<SettingsValues> {
 public:
  SettingsPort(std::filesystem::path client_root, SettingsFile& file, GameOptions& options)
      : client_root_(std::move(client_root)), file_(file), options_(options) {}

  // Où pousser ce qu'on pose, ou nul : les valeurs se posent alors sans être
  // poussées, et l'appelant les poussera lui-même.
  void Aim(const OptionTargets* to) noexcept { to_ = to; }
  // CE QUE LA PARTIE REDESCEND après une pose poussée : les réglages qu'elle
  // ne lit qu'à l'ouverture, et qu'elle remonterait sinon à l'image suivante
  // par-dessus le profil qu'on vient de poser. Vide hors partie.
  void AfterPose(std::function<void()> then) { after_pose_ = std::move(then); }

  [[nodiscard]] std::uint64_t Revision() override { return file_.revision(); }
  [[nodiscard]] SettingsValues Capture() override { return file_.SharedValues(); }
  void Pose(const SettingsValues& profile) override;
  [[nodiscard]] SettingsValues Defaults() override { return file_.SharedDefaults(); }
  [[nodiscard]] bool Load(SettingsProfileBook& into) override;
  void Save(const SettingsProfileBook& from) override;

 private:
  std::filesystem::path client_root_;
  SettingsFile& file_;
  GameOptions& options_;
  const OptionTargets* to_ = nullptr;
  std::function<void()> after_pose_;
};

// LES PROFILS DE RÉGLAGES, VIVANTS. ⚠ RIEN N'EST POSÉ AU DÉMARRAGE :
// `settings.yml` porte déjà l'état de la dernière session ; c'est l'entrée en
// jeu qui pose le profil du personnage.
class SettingsProfiles {
 public:
  SettingsProfiles(std::filesystem::path client_root, SettingsFile& file, GameOptions& options)
      : port_(std::move(client_root), file, options), keeper_(port_, &Text) {}

  void Start() { keeper_.Start(false); }
  // ⚠ SANS POUSSER : la partie applique tous les réglages juste après.
  void EnterCharacter(std::string_view character) {
    port_.Aim(nullptr);
    keeper_.EnterCharacter(character);
  }
  void Tick(const OptionTargets& to, std::uint32_t now_ms) {
    port_.Aim(&to);
    keeper_.Tick(now_ms);
    port_.Aim(nullptr);
  }
  void Flush() { keeper_.Flush(); }
  // Voir `SettingsPort::AfterPose`.
  void AfterPose(std::function<void()> then) { port_.AfterPose(std::move(then)); }
  [[nodiscard]] ro::ui::OptionSectionBody Body() { return keeper_.Body(); }

 private:
  [[nodiscard]] static ProfileFamilyText Text();

  SettingsPort port_;
  ProfileKeeper<SettingsValues> keeper_;
};

}  // namespace moonclient
