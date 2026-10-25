// LE CONTEXTE D'IMGUI DE L'ÉTAGE, hors de toute trame : son ouverture — l'atlas
// des polices, le dos, le fichier de réglages —, puis les polices et le skin
// qu'on y choisit, et les événements qu'on lui passe.

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/ui/system_fonts.h"

#include "game/layout_windows.h"
#include "game/ui_stage.h"

namespace moonclient {

ro::core::Result<UiStage> UiStage::Open(SDL_Window* window, Windows& windows, int font_family,
                                        const std::filesystem::path& loose_font_folder,
                                        const std::filesystem::path& settings_file) {
  auto ui = std::make_unique<ro::ui::UiContext>();
  // ⚠ L'ATLAS SE BÂTIT AVANT LE DOS, et avant la première trame : ImGui grave
  // ses textures au fil des trames, et refaire l'atlas en cours de route lui
  // reprendrait ce qu'il vient de poser.
  //
  // ⚠ ET CE N'EST PAS UN LUXE : la police intégrée d'ImGui ne connaît que
  // U+0020..U+00FF. Sans celle-ci, tout ce qui dépasse — points de suspension,
  // tiret cadratin, œ — sort en « ? » à l'écran, y compris dans les textes du
  // jeu, que nous n'écrivons pas.
  std::vector<ro::ui::FontFamilySpec> usable_families;
  ro::ui::FontSetup fonts = ro::ui::ReadableFontSetup(font_family, usable_families);
  // LES POLICES DU JOUEUR, lues une fois ici : en déposer une nouvelle demande
  // de relancer le client, comme toute police de l'atlas.
  const std::vector<ro::ui::LooseFontFile> loose_files = ro::ui::FindLooseFonts(loose_font_folder);
  std::vector<ro::ui::FontFamilySpec> loose_fonts;
  for (const ro::ui::LooseFontFile& file : loose_files) {
    loose_fonts.push_back(ro::ui::FontFamilySpec{.label = file.label, .regular = file.path});
  }
  fonts.loose = loose_fonts;
  ui->UseFonts(fonts);
  ro::core::Result<ro::ui::UiBackendGl> backend = ro::ui::UiBackendGl::Attach(*ui, window);
  if (!backend.has_value()) {
    return std::unexpected(backend.error());
  }

  // OÙ LES RÉGLAGES VIVENT : l'appelant le décide — voir `ClientShell::OpenWindow`.
  if (!settings_file.empty()) {
    ui->UseSettingsFile(settings_file);
  }
  // LES INTERRUPTEURS DU PROFIL DE DISPOSITION : voir `RememberLayoutWindows`.
  RememberLayoutWindows(*ui, windows);

  return UiStage(std::move(ui), std::move(*backend));
}

UiStage::UiStage(std::unique_ptr<ro::ui::UiContext> ui, ro::ui::UiBackendGl backend)
    : ui_(std::move(ui)), backend_(std::move(backend)) {}

ImFont* UiStage::DamageFont(std::string_view name, bool bold) const {
  // ⚠ UN NOM INCONNU — une police retirée du dossier depuis — RETOMBE SUR CELLE
  // DE L'INTERFACE, comme un nom vide : le réglage garde le sien, et la
  // retrouve si le fichier revient.
  const ro::ui::FontStyle style = bold ? ro::ui::FontStyle::kBold : ro::ui::FontStyle::kRegular;
  ImFont* named = name.empty() ? nullptr : ui_->FontNamed(name, style);
  return named != nullptr ? named : ui_->ChosenFont(style);
}

void UiStage::ChooseFont(int family) {
  // ⚠ PAR LE NOM : le réglage compte dans la table entière, l'atlas dans les
  // familles trouvées, et une famille absente de la machine décalerait tout ce
  // qui la suit d'un cran. Hors de la table — la police intégrée, ou un fichier
  // trafiqué —, c'est la règle du contexte qui tranche.
  const std::span<const ro::ui::FontFamilySpec> families = ro::ui::SystemFontFamilies();
  if (family < 0 || family >= static_cast<int>(families.size())) {
    ui_->ChooseFont(family);
    return;
  }
  ui_->ChooseFontNamed(families[static_cast<std::size_t>(family)].label);
}

void UiStage::TakeEvent(const SDL_Event& event) const {
  // Le verdict de `ProcessEvent` — « ImGui l'a pris » — n'est pas consulté :
  // il vaut vrai pour tout événement de souris ou de clavier, y compris ceux
  // qui ne visent aucune fenêtre. Ce qui tranche est le laissez-passer, qui
  // sait ce qu'il y a SOUS le curseur.
  static_cast<void>(backend_.ProcessEvent(event));
}

void UiStage::UseSkin(const ro::render::SkinTextures& textures) {
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    const ro::render::SkinTextures::Piece& piece =
        textures.Get(static_cast<ro::render::SkinPiece>(rank));
    // ⚠ UNE PIÈCE ABSENTE SE VIDE : l'ancienne texture est morte avec l'ancien skin.
    skin_.Set(static_cast<ro::render::SkinPiece>(rank),
              piece.valid()
                  ? ro::ui::SkinImage{.id = static_cast<ImTextureID>(piece.texture.name()),
                                      .width = static_cast<float>(piece.width),
                                      .height = static_cast<float>(piece.height)}
                  : ro::ui::SkinImage{});
  }
}

}  // namespace moonclient
