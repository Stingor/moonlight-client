#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_freetype.h>

#include "ro/core/log.h"
#include "ro/ui/font_setup.h"
#include "ro/ui/ui_context.h"

namespace ro::ui {
namespace {

// ── CE QU'ON DEMANDE À CHAQUE POLICE ─────────────────────────────────────────
//
// ⚠ CES PLAGES NE SERVENT QUE LES DOS SANS CHARGEMENT À LA DEMANDE. Depuis
// ImGui 1.92, un dos qui sait mettre sa texture à jour — le nôtre le sait —
// charge les glyphes au fil de l'eau, et la source couvre alors tout ce que le
// fichier contient. Les déclarer ne coûte rien et documente ce à quoi on tient.
//
// ⚠ ET L'ESSAI `tests/ui/font_coverage_test.cpp` LES VÉRIFIE. Une plage retirée
// ici sans que personne ne s'en aperçoive rendrait les « ? » de l'interface,
// exactement comme avant — et c'est un défaut qui ne se voit qu'en jouant.
constexpr ImWchar kLatinRanges[] = {
    0x0020, 0x00FF,  // latin de base et son supplément : les accents
    0x0152, 0x0153,  // Œ œ, le « cœur » et le « bœuf » du français
    0x2010, 0x203A,  // tirets, apostrophes et guillemets courbes, « … », puce
    0xFFFD, 0xFFFD,  // le carré « caractère manquant », propre
    0,
};

// L'antislash, et lui seul.
constexpr ImWchar kBackslashOnly[] = {0x005C, 0x005C, 0};

// UN FICHIER DE POLICE lu en mémoire : celui des emoji, une fois par chemin, et
// ceux du dossier du jeu.
//
// 🔴 `AddFontFromFileTTF` RELIT ET DUPLIQUE LE FICHIER À CHAQUE SOURCE,
// et `seguiemj.ttf` pèse plus de douze mégaoctets. Douze familles en trois
// graisses, c'est un demi-gigaoctet pour une police d'appoint. On le lit donc
// en mémoire, et `FontDataOwnedByAtlas = false` dit à l'atlas de ne PAS le
// libérer : sans cela il le ferait une fois par source.
//
// ⚠ ET UN FICHIER VIDE NE DOIT JAMAIS ATTEINDRE `AddFontFromFileTTF`, qui
// abandonne le programme par assertion. Lu ici, il rend vide et s'écarte.
//
// Rend vide si le fichier manque ou ne se lit pas en entier : rien à charger.
//
// ⚠ LE CHEMIN SE LIT EN UTF-8, comme ImGui lit les siens : celui d'une police
// du dossier du jeu peut porter des accents, que la page de code du système
// ne sait pas toujours dire.
[[nodiscard]] std::vector<char> ReadFontFile(std::string_view path) {
  const std::filesystem::path file_path(std::u8string(path.begin(), path.end()));
  std::error_code missing;
  if (path.empty() || !std::filesystem::exists(file_path, missing)) {
    return {};
  }
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file) {
    return {};
  }
  const std::streamoff size = file.tellg();
  if (size <= 0) {
    return {};
  }
  std::vector<char> kept(static_cast<std::size_t>(size));
  file.seekg(0);
  if (!file.read(kept.data(), size)) {
    return {};  // lecture partielle : ne rien fusionner
  }
  return kept;
}

// À appeler JUSTE APRÈS l'ajout de la police à enrichir : `MergeMode` fusionne
// dans la DERNIÈRE police ajoutée à l'atlas, pas dans une police nommée.
void MergeEmoji(std::span<const char> file, float size_pixels, const ImFontConfig& base) {
  if (file.empty()) {
    return;
  }
  ImFontConfig emoji = base;
  emoji.MergeMode = true;
  emoji.GlyphExcludeRanges = nullptr;  // l'antislash-won ne la concerne pas
  emoji.FontDataOwnedByAtlas = false;  // 🔴 tampon partagé, cf. ci-dessus
  // ⚠ `LoadColor` OU RIEN. Une police d'emoji porte ses couleurs dans une table
  // à part ; sans ce drapeau, FreeType n'en grave que le contour monochrome. Le
  // drapeau est bien PAR SOURCE : seuls les emoji sont chargés en couleur.
  emoji.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
  // 🔴 AUCUNE PLAGE, ET C'EST DÉLIBÉRÉ. Depuis ImGui 1.92 les glyphes se
  // chargent à la demande quand le dos sait mettre sa texture à jour — le nôtre
  // le sait. Les déclarer ferait prégraver deux mille glyphes COULEUR d'un coup,
  // au risque de faire déborder la texture — et un atlas qui déborde, ce n'est
  // pas du texte moche, c'est du texte ABSENT, partout.
  ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
      const_cast<char*>(file.data()), static_cast<int>(file.size()), size_pixels, &emoji, nullptr);
}

// Ajoute un fichier à l'atlas, ou rend nul. Un chemin vide n'est pas une erreur :
// c'est une variante que la famille ne livre pas.
//
// ⚠ L'EXISTENCE SE VÉRIFIE ICI, ET C'EST OBLIGATOIRE. `AddFontFromFileTTF` ne
// rend PAS nul sur un fichier absent : il lève une assertion « Could not load
// font file! » et abandonne le programme. Un chemin venu d'un fichier de
// réglages, ou d'une machine où cette police n'est pas installée, ferait donc
// planter le client au démarrage — pour une POLICE.
[[nodiscard]] ImFont* AddFile(std::string_view path, float size_pixels, const ImFontConfig& shape) {
  if (path.empty()) {
    return nullptr;
  }
  std::error_code ignored;
  if (!std::filesystem::exists(std::filesystem::path(path), ignored)) {
    return nullptr;
  }
  // ⚠ `AddFontFromFileTTF` VEUT UNE CHAÎNE C : la vue qu'on reçoit n'en est pas
  // une, et son zéro terminal n'est pas garanti.
  const std::string name(path);
  ImFontConfig config = shape;
  return ImGui::GetIO().Fonts->AddFontFromFileTTF(name.c_str(), size_pixels, &config, kLatinRanges);
}

}  // namespace

void UiContext::UseFonts(const FontSetup& setup) {
  MakeCurrent();
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->Clear();
  fonts_ = {};
  map_sign_font_ = nullptr;
  family_count_ = std::min(setup.families.size(), kMostFontFamilies);
  family_labels_ = {};
  // ⚠ APRÈS `Clear` SEULEMENT : l'atlas ne lit plus leurs octets.
  loose_fonts_.clear();
  // ⚠ LE FICHIER D'EMOJI SE RELIT QUAND SON CHEMIN CHANGE, et seulement alors.
  // Après `Clear` : l'atlas ne tient plus rien dessus, on peut le remplacer. Il
  // doit survivre à l'atlas — FreeType garde une face ouverte dessus —, ce que
  // l'ordre de destruction garantit : `~UiContext` détruit le contexte ImGui
  // AVANT ses membres.
  if (!emoji_read_ || emoji_path_ != setup.emoji_source) {
    emoji_bytes_ = ReadFontFile(setup.emoji_source);
    emoji_path_ = std::string(setup.emoji_source);
    emoji_read_ = true;
  }

  // LA POLICE INTÉGRÉE EN PREMIER, toujours bâtie : c'est le choix « sans
  // fichier », et c'est aussi ce qui laisse un contexte utilisable quand aucune
  // famille n'est trouvée.
  builtin_font_ = io.Fonts->AddFontDefault();

  ImFontConfig shape;
  // ⚠ `SizePixels` LAISSÉ À ZÉRO POUR LES FUSIONS, et ce n'est pas un oubli : la
  // police intégrée a une taille de RÉFÉRENCE implicite, et ImGui refuse par
  // assertion qu'on lui fusionne une source à taille explicite. À zéro, la
  // source reprend celle de sa cible — sans quoi des points de suspension
  // seraient plus grands que le texte qui les porte.
  shape.OversampleH = 1;
  shape.OversampleV = 1;
  shape.PixelSnapH = true;

  for (std::size_t rank = 0; rank < family_count_; ++rank) {
    const FontFamilySpec& family = setup.families[rank];
    family_labels_[rank] = std::string(family.label);
    // ── L'ANTISLASH EST EXCLU DE LA PRINCIPALE ──────────────────────────────
    // 🔴 PAR `GlyphExcludeRanges`, ET SURTOUT PAS PAR UN TROU DANS LES PLAGES.
    // Depuis ImGui 1.92 les glyphes se chargent à la demande et les plages sont
    // LEGACY : découper autour de 0x5C n'exclurait rien du tout, la police le
    // fournirait quand même, et comme elle est la première source son won
    // gagnerait. C'est le piège de cette version.
    ImFontConfig main = shape;
    const bool exclude_backslash = rank == 0 && !setup.backslash_source.empty();
    if (exclude_backslash) {
      main.GlyphExcludeRanges = kBackslashOnly;
    }
    const auto style = [this, rank](FontStyle which) -> ImFont*& {
      return fonts_[rank][static_cast<std::size_t>(which)];
    };
    style(FontStyle::kRegular) = AddFile(family.regular, setup.size_pixels, main);

    // La latine qui fournit l'antislash, fusionnée JUSTE APRÈS : `MergeMode`
    // fusionne dans la DERNIÈRE police ajoutée à l'atlas, pas dans une police
    // nommée. Une seule ligne entre les deux et c'est une autre qu'on enrichit.
    if (exclude_backslash && style(FontStyle::kRegular) != nullptr) {
      ImFontConfig lend = shape;
      lend.MergeMode = true;
      lend.GlyphExcludeRanges = nullptr;  // c'est ELLE qui doit fournir 0x5C
      // Même garde que ci-dessus : ce chemin-là vient d'`app`, pas de nous.
      std::error_code missing;
      if (std::filesystem::exists(std::filesystem::path(setup.backslash_source), missing)) {
        const std::string source(setup.backslash_source);
        io.Fonts->AddFontFromFileTTF(source.c_str(), setup.size_pixels, &lend, kBackslashOnly);
      }
    }
    // 🔴 LES EMOJI SE FUSIONNENT DANS CHACUNE, pas seulement dans la
    // régulière. Une police ImGui ne connaît que SES sources, et la grasse est
    // une police SÉPARÉE : un emoji dans un passage en gras sortirait en
    // losange, et l'on croirait à un défaut intermittent.
    if (style(FontStyle::kRegular) != nullptr) {
      MergeEmoji(emoji_bytes_, setup.size_pixels, shape);
    }
    style(FontStyle::kBold) = AddFile(family.bold, setup.size_pixels, shape);
    if (style(FontStyle::kBold) != nullptr) {
      MergeEmoji(emoji_bytes_, setup.size_pixels, shape);
    }
    style(FontStyle::kItalic) = AddFile(family.italic, setup.size_pixels, shape);
    if (style(FontStyle::kItalic) != nullptr) {
      MergeEmoji(emoji_bytes_, setup.size_pixels, shape);
    }
  }

  // LES POLICES DU DOSSIER DU JEU, chacune lue en mémoire d'abord : c'est ce qui
  // écarte un fichier vide avant qu'ImGui ne l'avale. Un fichier que FreeType ne
  // sait pas ouvrir fait rendre nul à l'atlas, qui défait son ajout.
  // ⚠ LA PLACE EST RÉSERVÉE D'AVANCE : l'atlas garde l'adresse des octets.
  const std::size_t loose_count = std::min(setup.loose.size(), kMostLooseFonts);
  loose_fonts_.reserve(loose_count);
  for (const FontFamilySpec& loose : setup.loose.first(loose_count)) {
    LooseFont kept{.label = std::string(loose.label), .bytes = ReadFontFile(loose.regular)};
    if (kept.bytes.empty()) {
      core::LogWarning("police du dossier du jeu illisible : {}", loose.regular);
      continue;
    }
    ImFontConfig config = shape;
    config.FontDataOwnedByAtlas = false;
    kept.font =
        io.Fonts->AddFontFromMemoryTTF(kept.bytes.data(), static_cast<int>(kept.bytes.size()),
                                       setup.size_pixels, &config, kLatinRanges);
    if (kept.font == nullptr) {
      core::LogWarning("police du dossier du jeu refusée par FreeType : {}", loose.regular);
      continue;
    }
    loose_fonts_.push_back(std::move(kept));
  }

  // LA POLICE DU BANDEAU DU NOM DE LA CARTE, en tout-ou-rien : chaque pixel
  // d'une lettre est plein ou vide, comme le jeu la grave. Le fort hinting est
  // celui que FreeType destine à ce mode-là ; sans lui, les fûts d'un même
  // caractère tombent à des épaisseurs inégales.
  ImFontConfig aliased = shape;
  aliased.FontLoaderFlags |=
      ImGuiFreeTypeLoaderFlags_Monochrome | ImGuiFreeTypeLoaderFlags_MonoHinting;
  map_sign_font_ = AddFile(setup.map_sign_source, setup.size_pixels, aliased);

  ChooseFont(setup.chosen);

  // ⚠ CE QUI A ÉTÉ BÂTI SE DIT, et ce n'est pas du bavardage : une police qui
  // ne s'applique pas ne laisse AUCUNE trace — l'interface reste simplement
  // celle d'avant, et l'on cherche dans le réglage un défaut qui est dans
  // l'atlas. Une ligne au démarrage tranche la question.
  const bool loaded = chosen_font_ >= 0 && FontFamilyUsable(chosen_font_);
  const ImFont* drawing = ImGui::GetIO().FontDefault;
  // ⚠ LA TAILLE EN FAIT PARTIE, et il a fallu une comparaison d'écrans pour s'en
  // apercevoir : une police chargée sans taille sort à vingt pixels, pas à celle
  // qu'on croyait demander, et rien ne le dit.
  core::LogInfo(
      "polices : {} famille(s) et {} du dossier du jeu, choix {} — {}, {} px, dessin avec {} ; "
      "bandeau de carte : {}",
      family_count_, loose_fonts_.size(), chosen_font_, loaded ? "chargée" : "celle d'ImGui",
      setup.size_pixels, drawing != nullptr ? drawing->GetDebugName() : "(rien)",
      map_sign_font_ != nullptr ? setup.map_sign_source : "police courante");
}

void UiContext::ChooseFont(int family) {
  MakeCurrent();
  // Un rang venu d'un fichier trafiqué ou d'une version future ne doit pas sortir
  // de la table : on retombe sur la première, jamais sur une lecture hors bornes.
  if (family < -1 || family >= static_cast<int>(family_count_)) {
    family = 0;
  }
  chosen_font_ = family;
  ImGui::GetIO().FontDefault = ChosenFont(FontStyle::kRegular);
}

void UiContext::ChooseFontNamed(std::string_view label) {
  for (std::size_t rank = 0; rank < family_count_; ++rank) {
    if (family_labels_[rank] == label) {
      ChooseFont(static_cast<int>(rank));
      return;
    }
  }
  ChooseFont(0);
}

int UiContext::chosen_font() const { return chosen_font_; }

ImFont* UiContext::ChosenFont(FontStyle style) const {
  if (chosen_font_ < 0) {
    return builtin_font_;  // la police intégrée, avec ses trous : c'est un choix
  }
  const auto rank = static_cast<std::size_t>(chosen_font_);
  if (rank >= family_count_) {
    return builtin_font_;
  }
  if (ImFont* wanted = fonts_[rank][static_cast<std::size_t>(style)]; wanted != nullptr) {
    return wanted;
  }
  // Variante absente : le normal fait l'affaire. Tahoma n'a pas d'italique, et
  // un texte non penché vaut mieux qu'un texte manquant.
  if (ImFont* plain = fonts_[rank][static_cast<std::size_t>(FontStyle::kRegular)];
      plain != nullptr) {
    return plain;
  }
  return builtin_font_;
}

bool UiContext::FontFamilyUsable(int family) const {
  if (family < 0) {
    return true;  // la police intégrée est toujours là
  }
  const auto rank = static_cast<std::size_t>(family);
  return rank < family_count_ &&
         fonts_[rank][static_cast<std::size_t>(FontStyle::kRegular)] != nullptr;
}

ImFont* UiContext::FontNamed(std::string_view label, FontStyle style) const {
  for (std::size_t rank = 0; rank < family_count_; ++rank) {
    if (family_labels_[rank] != label) {
      continue;
    }
    // Même repli que la famille choisie : une variante absente prend le normal.
    if (ImFont* wanted = fonts_[rank][static_cast<std::size_t>(style)]; wanted != nullptr) {
      return wanted;
    }
    return fonts_[rank][static_cast<std::size_t>(FontStyle::kRegular)];
  }
  for (const LooseFont& loose : loose_fonts_) {
    if (loose.label == label) {
      return loose.font;
    }
  }
  return nullptr;
}

std::vector<std::string> UiContext::LooseFontNames() const {
  std::vector<std::string> names;
  names.reserve(loose_fonts_.size());
  for (const LooseFont& loose : loose_fonts_) {
    names.push_back(loose.label);
  }
  return names;
}

}  // namespace ro::ui
