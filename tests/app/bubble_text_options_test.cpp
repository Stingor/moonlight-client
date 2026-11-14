// LA TAILLE DU TEXTE DES BULLES : sa clé, sa rangée, ses bornes.

#include "options/bubble_text_options.h"

#include <filesystem>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_bubble.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "support/temp_directory.h"

using moonclient::BubbleTextScaleRow;
using moonclient::ChangeBubbleTextScale;
using moonclient::DeclareBubbleTextScale;
using moonclient::SettingsFile;
using ro::test_support::TempDirectory;
using ro::ui::ChatBubbleLook;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

namespace {

// L'identifiant que le catalogue donnerait à la rangée : la page le recopie
// tel quel.
constexpr int kRowId = 4242;
// Les bornes et le défaut, écrits ici plutôt que relus : un essai qui reprend
// la constante qu'il vérifie ne vérifie rien.
constexpr int kDefaultPercent = 100;
constexpr int kLeastPercent = 80;
constexpr int kMostPercent = 160;

[[nodiscard]] ChatBubbleLook Loaded(const TempDirectory& folder, std::string_view text) {
  folder.WriteText("settings.yml", text);
  ChatBubbleLook look;
  SettingsFile file(folder.path() / "settings.yml");
  DeclareBubbleTextScale(file, look);
  file.Load();
  return look;
}

}  // namespace

TEST_CASE("Taille des bulles : cent pour cent par défaut, un curseur de 80 à 160", "[app][bulle]") {
  const ChatBubbleLook look;
  CHECK(look.text_scale_percent == kDefaultPercent);
  CHECK(ro::ui::ChatBubbleScale(look) == 1.0F);
  const OptionRowView row = BubbleTextScaleRow(kRowId, look);
  CHECK(row.id == kRowId);
  CHECK(row.kind == OptionKind::kSlider);
  CHECK(row.label != nullptr);
  CHECK(row.help != nullptr);
  CHECK(row.value == static_cast<float>(kDefaultPercent));
  CHECK(row.lowest == static_cast<float>(kLeastPercent));
  CHECK(row.highest == static_cast<float>(kMostPercent));
}

TEST_CASE("Taille des bulles : le curseur arrondit et borne", "[app][bulle]") {
  ChatBubbleLook look;
  ChangeBubbleTextScale(look, 123.4F);
  CHECK(look.text_scale_percent == 123);
  ChangeBubbleTextScale(look, 500.0F);
  CHECK(look.text_scale_percent == kMostPercent);
  ChangeBubbleTextScale(look, 10.0F);
  CHECK(look.text_scale_percent == kLeastPercent);
}

TEST_CASE("Taille des bulles : la clé anglaise se relit", "[app][bulle]") {
  const TempDirectory folder;
  const ChatBubbleLook look = Loaded(folder, "bubbles_text_scale: 130\n");
  CHECK(look.text_scale_percent == 130);
  CHECK(BubbleTextScaleRow(kRowId, look).value == 130.0F);
}

TEST_CASE("Taille des bulles : une valeur relue hors bornes y est ramenée", "[app][bulle]") {
  const TempDirectory folder;
  const ChatBubbleLook huge = Loaded(folder, "bubbles_text_scale: 400\n");
  CHECK(BubbleTextScaleRow(kRowId, huge).value == static_cast<float>(kMostPercent));
  CHECK(ro::ui::ChatBubbleScale(huge) == 1.6F);

  const ChatBubbleLook tiny = Loaded(folder, "bubbles_text_scale: -5\n");
  CHECK(BubbleTextScaleRow(kRowId, tiny).value == static_cast<float>(kLeastPercent));
  CHECK(ro::ui::ChatBubbleScale(tiny) == 0.8F);
}

TEST_CASE("Taille des bulles : le réglage survit à l'aller-retour", "[app][bulle]") {
  const TempDirectory folder;
  const std::filesystem::path path = folder.path() / "settings.yml";
  {
    ChatBubbleLook look;
    SettingsFile file(path);
    DeclareBubbleTextScale(file, look);
    file.Load();
    ChangeBubbleTextScale(look, 145.0F);
    file.Touch();
    file.Flush();
  }
  ChatBubbleLook reread;
  SettingsFile again(path);
  DeclareBubbleTextScale(again, reread);
  again.Load();
  CHECK(reread.text_scale_percent == 145);
}
