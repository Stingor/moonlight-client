#include "game/system_fonts.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
#include <vector>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/core/utf8_path.h"

namespace moonclient {
namespace {

using ro::ui::FontFamilySpec;

// Ce que FreeType sait ouvrir, en minuscules.
constexpr std::array<std::string_view, 3> kLooseFontExtensions = {".ttf", ".otf", ".ttc"};

#ifdef _WIN32
// Le dossier des polices de Windows. ⚠ ÉCRIT UNE FOIS : une famille ne répète
// pas le chemin, elle nomme son fichier.
constexpr std::string_view kFontFolder = "C:\\Windows\\Fonts\\";
#else
// LES DOSSIERS OÙ LINUX RANGE SES POLICES : chaque distribution les classe à sa
// façon dessous (`truetype/dejavu/`, `dejavu-sans-fonts/`, `TTF/`…), si bien
// qu'un fichier se cherche par son NOM, dans toute l'arborescence.
constexpr std::array<std::string_view, 2> kSystemFontFolders = {"/usr/share/fonts",
                                                                "/usr/local/share/fonts"};
// Ceux de l'utilisateur, sous son dossier personnel.
constexpr std::array<std::string_view, 2> kUserFontFolders = {".local/share/fonts", ".fonts"};

// Chaque fichier de police trouvé, par son nom en minuscules. Relevé une fois.
[[nodiscard]] const std::map<std::string, std::string>& FontFilesByName() {
  static const std::map<std::string, std::string> files = [] {
    std::vector<std::filesystem::path> folders(kSystemFontFolders.begin(),
                                               kSystemFontFolders.end());
    if (const char* home = std::getenv("HOME"); home != nullptr) {
      for (const std::string_view user_folder : kUserFontFolders) {
        folders.push_back(std::filesystem::path(home) / user_folder);
      }
    }
    std::map<std::string, std::string> found;
    for (const std::filesystem::path& folder : folders) {
      std::error_code code;
      for (auto entry = std::filesystem::recursive_directory_iterator(folder, code);
           !code && entry != std::filesystem::recursive_directory_iterator();
           entry.increment(code)) {
        if (entry->is_regular_file(code)) {
          // ⚠ LE PREMIER TROUVÉ GARDE LE NOM : les dossiers du système passent
          // avant ceux de l'utilisateur.
          found.emplace(ro::core::AsciiLower(ro::core::Utf8FromPath(entry->path().filename())),
                        ro::core::Utf8FromPath(entry->path()));
        }
      }
    }
    return found;
  }();
  return files;
}
#endif

// Les octets UTF-8 d'une chaîne `char8_t`, dans une chaîne ordinaire.
[[nodiscard]] std::string Utf8Of(const std::u8string& text) {
  return std::string(text.begin(), text.end());
}

// Un fichier de ce dossier-là, gardé à une adresse STABLE.
//
// ⚠ UN `deque`, ET SURTOUT PAS UN `vector` : les vues que la table distribue
// pointent dans ces chaînes-là. Un vector se RÉALLOUE, et la vue rendue au
// premier appel désignerait un tampon libéré dès le douzième — une famille sur
// deux chargerait n'importe quoi, et seulement sur les machines qui les ont
// toutes. Un deque ne déplace jamais ce qu'il contient déjà.
//
// Sous Linux, le chemin est celui où le fichier a été trouvé, ou vide.
[[nodiscard]] const std::string& Kept(std::string_view file) {
  static std::deque<std::string> kept;
#ifdef _WIN32
  kept.emplace_back(std::string(kFontFolder) + std::string(file));
#else
  const std::map<std::string, std::string>& files = FontFilesByName();
  const auto found = files.find(ro::core::AsciiLower(file));
  kept.emplace_back(found == files.end() ? std::string() : found->second);
#endif
  return kept.back();
}

// ⚠ LA TABLE EST BÂTIE UNE FOIS, PAS ÉCRITE EN `constexpr` : chaque entrée doit
// porter un chemin COMPLET, et le composer à la construction évite de répéter
// « C:\Windows\Fonts\ » trente-six fois — où une faute de frappe ne se verrait
// que sur la machine qui n'a pas cette police-là.
//
// ⚠ UNE TABLE PAR SYSTÈME, ET LE RANG EST CELUI DE LA TABLE : c'est lui que les
// réglages retiennent. Celle de Windows ne bouge donc pas d'une ligne.
[[nodiscard]] const std::vector<FontFamilySpec>& AllFamilies() {
#ifdef _WIN32
  static const std::vector<FontFamilySpec> families = {
      // ── LE DÉFAUT, ET LE SEUL CHOIX COMPLET ──────────────────────────────
      // Latin accentué, ponctuation typographique ET coréen. Son antislash-won
      // est repris ailleurs.
      {ro::core::TrText("Malgun Gothic"), Kept("malgun.ttf"), Kept("malgunbd.ttf"), {}},
      // ── LES LATINES, toutes livrées avec Windows 10 et 11 ─────────────────
      // ⚠ Tahoma n'a PAS d'italique : le fichier n'existe pas. Ce n'est pas un
      // oubli de la table, et la variante retombe sur le normal.
      {ro::core::TrText("Tahoma"), Kept("tahoma.ttf"), Kept("tahomabd.ttf"), {}},
      {ro::core::TrText("Verdana"), Kept("verdana.ttf"), Kept("verdanab.ttf"),
       Kept("verdanai.ttf")},
      {ro::core::TrText("Segoe UI"), Kept("segoeui.ttf"), Kept("segoeuib.ttf"),
       Kept("segoeuii.ttf")},
      {ro::core::TrText("Consolas"), Kept("consola.ttf"), Kept("consolab.ttf"),
       Kept("consolai.ttf")},
      {ro::core::TrText("Arial"), Kept("arial.ttf"), Kept("arialbd.ttf"), Kept("ariali.ttf")},
      {ro::core::TrText("Trebuchet MS"), Kept("trebuc.ttf"), Kept("trebucbd.ttf"),
       Kept("trebucit.ttf")},
      {ro::core::TrText("Candara"), Kept("candara.ttf"), Kept("candarab.ttf"),
       Kept("candarai.ttf")},
      {ro::core::TrText("Corbel"), Kept("corbel.ttf"), Kept("corbelb.ttf"), Kept("corbeli.ttf")},
      // ⚠ Franklin Gothic Medium EST le gras de sa famille : Windows ne livre
      // que « Medium » et « Medium Italic », rien de plus gras.
      {ro::core::TrText("Franklin Gothic"), Kept("framd.ttf"), {}, Kept("framdit.ttf")},
      {ro::core::TrText("Georgia"), Kept("georgia.ttf"), Kept("georgiab.ttf"),
       Kept("georgiai.ttf")},
      {ro::core::TrText("Times New Roman"), Kept("times.ttf"), Kept("timesbd.ttf"),
       Kept("timesi.ttf")},
  };
#else
  static const std::vector<FontFamilySpec> families = {
      // ── LE DÉFAUT : DejaVu Sans, installée presque partout ──────────────────
      // Latin accentué et ponctuation typographique ; pas de coréen.
      {ro::core::TrText("DejaVu Sans"), Kept("DejaVuSans.ttf"), Kept("DejaVuSans-Bold.ttf"),
       Kept("DejaVuSans-Oblique.ttf")},
      {ro::core::TrText("Noto Sans"), Kept("NotoSans-Regular.ttf"), Kept("NotoSans-Bold.ttf"),
       Kept("NotoSans-Italic.ttf")},
      {ro::core::TrText("Liberation Sans"), Kept("LiberationSans-Regular.ttf"),
       Kept("LiberationSans-Bold.ttf"), Kept("LiberationSans-Italic.ttf")},
      // Le coréen, quand la distribution l'a : la seule de la table qui l'ait.
      {ro::core::TrText("Noto Sans CJK"),
       Kept("NotoSansCJK-Regular.ttc"),
       Kept("NotoSansCJK-Bold.ttc"),
       {}},
      {ro::core::TrText("DejaVu Sans Mono"), Kept("DejaVuSansMono.ttf"),
       Kept("DejaVuSansMono-Bold.ttf"), Kept("DejaVuSansMono-Oblique.ttf")},
      {ro::core::TrText("Liberation Serif"), Kept("LiberationSerif-Regular.ttf"),
       Kept("LiberationSerif-Bold.ttf"), Kept("LiberationSerif-Italic.ttf")},
  };
#endif
  return families;
}

[[nodiscard]] bool Exists(std::string_view path) {
  if (path.empty()) {
    return false;
  }
  std::error_code ignored;
  return std::filesystem::exists(std::filesystem::path(path), ignored);
}

}  // namespace

std::span<const FontFamilySpec> SystemFontFamilies() { return AllFamilies(); }

std::span<const int> InstalledFontFamilies() {
  // ⚠ RELEVÉ UNE FOIS, comme l'atlas qui s'en sert : une police installée en
  // cours de partie n'y entrerait pas davantage, et un menu qui l'offrirait
  // proposerait un choix sans effet.
  static const std::vector<int> installed = [] {
    std::vector<int> ranks;
    const std::vector<FontFamilySpec>& families = AllFamilies();
    for (std::size_t rank = 0; rank < families.size(); ++rank) {
      if (Exists(families[rank].regular)) {
        ranks.push_back(static_cast<int>(rank));
      }
    }
    return ranks;
  }();
  return installed;
}

int FontFamilyAtMenuRank(int menu_rank) {
  return FontFamilyAtMenuRank(menu_rank, InstalledFontFamilies());
}

int MenuRankOfFontFamily(int family) {
  return MenuRankOfFontFamily(family, InstalledFontFamilies());
}

std::string_view EmojiFontSource() {
  // Segoe UI Emoji est la police d'emoji de Windows, en couleur, installée
  // partout depuis Windows 8.1 ; Noto Color Emoji, celle des distributions
  // Linux. Une seule candidate par système : il n'y en a pas de seconde à
  // espérer.
#ifdef _WIN32
  static const std::string_view kCandidate = Kept("seguiemj.ttf");
#else
  static const std::string_view kCandidate = Kept("NotoColorEmoji.ttf");
#endif
  return Exists(kCandidate) ? kCandidate : std::string_view();
}

std::string_view BackslashFontSource() {
  // L'ordre est celui de la préférence : une police à chasse proportionnelle et
  // largement installée. La première trouvée gagne.
#ifdef _WIN32
  static const std::array<std::string_view, 3> kCandidates = {Kept("tahoma.ttf"), Kept("arial.ttf"),
                                                              Kept("segoeui.ttf")};
#else
  static const std::array<std::string_view, 3> kCandidates = {
      Kept("DejaVuSans.ttf"), Kept("LiberationSans-Regular.ttf"), Kept("NotoSans-Regular.ttf")};
#endif
  for (const std::string_view candidate : kCandidates) {
    if (Exists(candidate)) {
      return candidate;
    }
  }
  return {};
}

ro::ui::FontSetup ReadableFontSetup(int chosen, std::vector<FontFamilySpec>& usable) {
  // ⚠ LES FAMILLES INTROUVABLES SONT RETIRÉES, pas laissées vides, et par le
  // MÊME relevé que les menus : le rang d'une famille dans l'atlas est alors sa
  // place dans le menu, et une entrée qui ne dessinerait rien n'y figure pas.
  // Celle du joueur disparue, le défaut reprend.
  usable.clear();
  const std::vector<FontFamilySpec>& families = AllFamilies();
  for (const int rank : InstalledFontFamilies()) {
    const FontFamilySpec& family = families[static_cast<std::size_t>(rank)];
    usable.push_back(
        FontFamilySpec{.label = family.label,
                       .regular = family.regular,
                       .bold = Exists(family.bold) ? family.bold : std::string_view(),
                       .italic = Exists(family.italic) ? family.italic : std::string_view()});
  }
  // Moins de zéro reste la police intégrée : c'est un choix, pas un rang.
  return ro::ui::FontSetup{
      .families = usable,
      .chosen = usable.empty() || chosen < 0 ? -1 : MenuRankOfFontFamily(chosen),
      .size_pixels = ro::ui::kDefaultFontPixels,
      .backslash_source = BackslashFontSource(),
      .emoji_source = EmojiFontSource()};
}

std::vector<LooseFontFile> FindLooseFonts(const std::filesystem::path& folder) {
  std::vector<LooseFontFile> found;
  std::error_code failed;
  // ⚠ L'AVANCE PAR `increment` ET NON PAR `++` : l'opérateur lève sur un
  // dossier illisible, et une police ne vaut pas un plantage au démarrage.
  for (std::filesystem::directory_iterator entry(folder, failed);
       !failed && entry != std::filesystem::directory_iterator(); entry.increment(failed)) {
    std::error_code unread;
    if (!entry->is_regular_file(unread)) {
      continue;
    }
    std::string extension = Utf8Of(entry->path().extension().u8string());
    std::ranges::transform(extension, extension.begin(), [](char letter) {
      return static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    });
    if (std::ranges::find(kLooseFontExtensions, extension) == kLooseFontExtensions.end()) {
      continue;
    }
    // ⚠ EN UTF-8, NOM COMME CHEMIN : le nom s'écrit dans l'interface, et le
    // chemin se relit comme tel — une police « Époque.ttf » ne doit pas tomber
    // dans la page de code du système.
    found.push_back(LooseFontFile{.label = Utf8Of(entry->path().stem().u8string()),
                                  .path = Utf8Of(entry->path().u8string())});
  }
  std::ranges::sort(found, {}, &LooseFontFile::label);
  return found;
}

}  // namespace moonclient
