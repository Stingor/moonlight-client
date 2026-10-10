#include "ro/core/i18n.h"

#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <utility>

namespace ro::core {
namespace {

// Le code de la langue sans catalogue.
constexpr std::string_view kFrench = "fr";

// Les deux préfixes du format. Voir `LoadLanguage`.
constexpr std::string_view kFrenchPrefix = "fr:";
constexpr std::string_view kTranslatedPrefix = "tr:";

// LE CATALOGUE, et pourquoi il est gardé ici plutôt que passé.
//
// Traduire est demandé depuis chaque libellé de chaque fenêtre — des centaines
// de sites d'appel qui n'ont rien d'autre en commun. Leur faire porter un
// contexte jusque-là coûterait un paramètre à toute la chaîne pour une valeur
// qui ne change qu'au réglage.
//
// ⚠ C'est le seul état ambiant de ce dépôt avec le journal, et pour la même
// raison : la langue est une propriété du PROGRAMME, pas d'un module.
struct Catalogue {
  std::mutex guard;
  std::map<std::string, std::string, std::less<>> entries;
  // Les textes demandés et absents. Un `set` : le même libellé est demandé à
  // chaque image, et on veut la liste, pas le compte des images.
  std::set<std::string, std::less<>> missing;
  std::string language{kFrench};
  std::size_t serial = 0;
};

[[nodiscard]] Catalogue& TheCatalogue() {
  static Catalogue catalogue;
  return catalogue;
}

// Ce qui suit le préfixe, sans le retour chariot d'un fichier venu de Windows,
// et ses échappements rendus.
//
// ⚠ DEUX ÉCHAPPEMENTS, ET DEUX SEULEMENT : `\n` pour un saut de ligne, `\\`
// pour l'antislash lui-même. Le format est UNE LIGNE PAR TEXTE, or la moitié
// des infobulles du jeu en comptent deux ou trois : sans cela, ces textes-là
// ne peuvent pas être des CLÉS, donc pas être traduits du tout. Rien d'autre
// n'est échappé — ni guillemet, ni accolade de gabarit — pour garder le
// fichier lisible et corrigeable à la main.
[[nodiscard]] std::string AfterPrefix(std::string_view line, std::string_view prefix) {
  std::string_view rest = line.substr(prefix.size());
  if (!rest.empty() && rest.front() == ' ') {
    rest.remove_prefix(1);
  }
  while (!rest.empty() && (rest.back() == '\r' || rest.back() == '\n')) {
    rest.remove_suffix(1);
  }
  std::string rendered;
  rendered.reserve(rest.size());
  for (std::size_t at = 0; at < rest.size(); ++at) {
    // ⚠ L'ANTISLASH SE CONSOMME AVEC CE QU'IL ÉCHAPPE : sans cela « \\n »
    // rendrait un saut de ligne au lieu d'un antislash suivi d'un n.
    if (rest[at] == '\\' && at + 1 < rest.size()) {
      const char next = rest[at + 1];
      if (next == 'n') {
        rendered.push_back('\n');
        ++at;
        continue;
      }
      if (next == '\\') {
        rendered.push_back('\\');
        ++at;
        continue;
      }
    }
    rendered.push_back(rest[at]);
  }
  return rendered;
}

// L'inverse d'`AfterPrefix` : ce que l'export écrit pour que la relecture rende
// le texte à l'identique.
//
// 🔴 SANS LUI, UN TEXTE SUR DEUX LIGNES NE SE TRADUISAIT JAMAIS : le gabarit
// le coupait à son saut de ligne, la clé relue ne valait que la première, et
// elle ne correspondait plus à rien.
[[nodiscard]] std::string Escaped(std::string_view text) {
  std::string written;
  written.reserve(text.size());
  for (const char each : text) {
    if (each == '\\') {
      written += "\\\\";
    } else if (each == '\n') {
      written += "\\n";
    } else {
      written.push_back(each);
    }
  }
  return written;
}

}  // namespace

const char* Tr(const char* french) {
  if (french == nullptr) {
    return "";
  }
  Catalogue& catalogue = TheCatalogue();
  const std::lock_guard<std::mutex> held(catalogue.guard);
  // LE CHEMIN DU FRANÇAIS, en un seul test et sans rien chercher.
  if (catalogue.entries.empty()) {
    return french;
  }
  const auto found = catalogue.entries.find(std::string_view(french));
  if (found == catalogue.entries.end()) {
    // ⚠ CHERCHÉ AVANT D'INSÉRER : le même libellé revient à chaque image, et
    // l'insertion construisait une chaîne à chaque fois pour la jeter aussitôt.
    if (const std::string_view asked(french); !catalogue.missing.contains(asked)) {
      catalogue.missing.emplace(asked);
    }
    return french;
  }
  // Une traduction VIDE veut dire « pas encore faite » : on rend le français
  // plutôt qu'un libellé blanc, et l'entrée reste visible dans le fichier.
  return found->second.empty() ? french : found->second.c_str();
}

std::string CurrentLanguage() {
  Catalogue& catalogue = TheCatalogue();
  const std::lock_guard<std::mutex> held(catalogue.guard);
  return catalogue.language;
}

std::size_t LanguageSerial() {
  Catalogue& catalogue = TheCatalogue();
  const std::lock_guard<std::mutex> held(catalogue.guard);
  return catalogue.serial;
}

Result<void> LoadLanguage(const std::filesystem::path& path, std::string language) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return Fail(ErrorCode::kNotFound, "catalogue introuvable : " + path.string());
  }

  std::map<std::string, std::string, std::less<>> read;
  std::string line;
  std::string french;
  bool waiting = false;
  while (std::getline(file, line)) {
    const std::string_view view(line);
    if (view.starts_with('#') || view.empty() || view == "\r") {
      continue;
    }
    if (view.starts_with(kFrenchPrefix)) {
      french = AfterPrefix(view, kFrenchPrefix);
      waiting = true;
      continue;
    }
    if (view.starts_with(kTranslatedPrefix)) {
      if (!waiting) {
        // ⚠ UNE TRADUCTION SANS SON FRANÇAIS EST UN FICHIER ABÎMÉ, et la ranger
        // sous la clé précédente donnerait une interface qui ment. On refuse le
        // catalogue entier : à moitié chargé, personne ne saurait quelle moitié.
        return Fail(ErrorCode::kCorruptData,
                    "catalogue : une traduction sans texte francais — " + path.string());
      }
      read.insert_or_assign(french, AfterPrefix(view, kTranslatedPrefix));
      waiting = false;
    }
  }

  Catalogue& catalogue = TheCatalogue();
  const std::lock_guard<std::mutex> held(catalogue.guard);
  catalogue.entries = std::move(read);
  catalogue.language = std::move(language);
  // LES TEXTES MANQUANTS REPARTENT DE ZÉRO : ils disent ce qui manque À CETTE
  // langue, et les garder ferait accuser un catalogue des trous d'un autre.
  catalogue.missing.clear();
  ++catalogue.serial;
  return {};
}

void UseFrench() {
  Catalogue& catalogue = TheCatalogue();
  const std::lock_guard<std::mutex> held(catalogue.guard);
  catalogue.entries.clear();
  catalogue.missing.clear();
  catalogue.language = kFrench;
  ++catalogue.serial;
}

std::vector<std::string> MissingTexts() {
  Catalogue& catalogue = TheCatalogue();
  const std::lock_guard<std::mutex> held(catalogue.guard);
  return {catalogue.missing.begin(), catalogue.missing.end()};
}

Result<void> ExportMissing(const std::filesystem::path& path) {
  const std::vector<std::string> absent = MissingTexts();
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return Fail(ErrorCode::kIoFailure, "gabarit impossible a ecrire : " + path.string());
  }
  file << "# Textes demandes et absents du catalogue.\n";
  file << "# Completer chaque ligne 'tr:' puis fusionner dans le catalogue.\n\n";
  for (const std::string& text : absent) {
    file << kFrenchPrefix << ' ' << Escaped(text) << '\n';
    file << kTranslatedPrefix << '\n';
  }
  return {};
}

}  // namespace ro::core
