#include "chat/chat_history_file.h"

#include <algorithm>
#include <optional>
#include <span>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"

#include "chat/chat_tabs.h"
#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Les champs. ⚠ GRAVÉS : les changer perd l'historique de tous les joueurs, sans
// que rien ne le dise.
constexpr const char* kSavedAtField = "saved_at";
constexpr const char* kLinesField = "lines";
// Le numéro du type, celui des cases de filtre — voir `ro::sim::ChatType`.
constexpr const char* kTypeField = "type";
// Aucun type : la ligne qui le porte s'ignore.
constexpr int kNoType = -1;
constexpr const char* kTimeField = "time";
constexpr const char* kTextField = "text";
// Les derniers destinataires, dans l'ordre où la box les propose.
constexpr const char* kWhisperedField = "recipients";
// Les lignes tapées, de la plus ancienne à la plus récente.
constexpr const char* kTypedField = "typed";
constexpr const char* kColorField = "color";

// L'heure d'une ligne, « HH:MM:SS ». Écrite plutôt que déduite : une ligne
// d'hier soir n'a pas d'horloge de boucle à consulter.
constexpr std::size_t kTimeTextSize = 8;
constexpr std::size_t kHourAt = 0;
constexpr std::size_t kMinuteAt = 3;
constexpr std::size_t kSecondAt = 6;
constexpr int kTimePartDigits = 2;

constexpr const char* kFileHeader =
    "# Ce qui s'est dit la partie d'avant.\n"
    "#\n"
    "# ATTENTION : ce fichier contient les CHUCHOTEMENTS EN CLAIR. L'option qui\n"
    "# l'écrit est décochée par défaut, dans les réglages de la conversation —\n"
    "# la décocher efface ce fichier.\n"
    "#\n"
    "# « type » est le numéro de la case de filtre de la ligne, de 0 à 25, dans\n"
    "# l'ordre des « filter » de chat_tabs.yml. Une ligne sans type valable est\n"
    "# IGNORÉE : elle ne saurait plus dans quel onglet aller.\n"
    "#\n"
    "# « color » est celle que la ligne avait à l'écran, en 0xRRGGBB ; absente,\n"
    "# c'est la couleur ordinaire du fil.\n"
    "#\n"
    "# « recipients » est la liste des derniers pseudos à qui l'on a chuchoté,\n"
    "# le plus récent en tête. Elle dit À QUI l'on parle en privé, et vit donc\n"
    "# sous la même option que le reste de ce fichier.\n"
    "#\n"
    "# « typed » est ce qu'on a TAPÉ, de la plus ancienne à la plus récente —\n"
    "# ce que la flèche ↑ rappelle dans la barre de saisie. Elle a sa PROPRE\n"
    "# case dans les options, cochée par défaut, et porte les chuchotements\n"
    "# tapés comme tout le reste : décocher la case efface ce fichier.\n";

// Lit deux chiffres à cet endroit-là. Rend zéro sur tout ce qui n'en est pas —
// une heure abîmée vaut mieux qu'une ligne perdue.
[[nodiscard]] std::uint8_t TwoDigitsAt(std::string_view text, std::size_t at) {
  if (at + kTimePartDigits > text.size()) {
    return 0;
  }
  int value = 0;
  for (std::size_t step = 0; step < kTimePartDigits; ++step) {
    const char digit = text[at + step];
    if (digit < '0' || digit > '9') {
      return 0;
    }
    value = value * 10 + (digit - '0');
  }
  return static_cast<std::uint8_t>(value);
}

// Lit une liste de textes — destinataires, lignes tapées. Ce qui n'est pas une
// suite, ou pas un scalaire, ou vide, n'entre pas : un fichier qu'on peut
// ouvrir à la main se relit sans jamais coûter la partie.
void ReadTexts(const YAML::Node& listed, std::vector<std::string>* into) {
  if (!listed || !listed.IsSequence()) {
    return;
  }
  for (const YAML::Node& entry : listed) {
    if (!entry.IsScalar()) {
      continue;
    }
    std::string read = entry.as<std::string>(std::string());
    if (!read.empty()) {
      into->push_back(std::move(read));
    }
  }
}

// Écrit une liste de textes, ET SEULEMENT SI ELLE N'EST PAS VIDE : une clé vide
// dans un fichier que le joueur peut ouvrir se lit comme une liste qu'on aurait
// perdue.
//
// ⚠ `flow` DÉCIDE DE LA FORME, et ce n'est pas un goût : une poignée de pseudos
// tient sur une ligne et s'y lit mieux, une liste de phrases tapées n'y tiendrait
// pas. La forme des destinataires est celle qu'ils avaient — la changer
// réécrirait le fichier de tous les joueurs pour rien.
void WriteTexts(YAML::Emitter& out, const char* field, std::span<const std::string> texts,
                bool flow) {
  if (texts.empty()) {
    return;
  }
  out << YAML::Key << field << YAML::Value;
  if (flow) {
    out << YAML::Flow;
  }
  out << YAML::BeginSeq;
  for (const std::string& text : texts) {
    out << text;
  }
  out << YAML::EndSeq;
}

}  // namespace

std::filesystem::path ChatHistoryPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kChatHistoryFileName;
}

PastChat LoadChatHistory(const std::filesystem::path& client_root) {
  PastChat past;
  const std::filesystem::path path = ChatHistoryPath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return past;  // première partie, ou option décochée : on joue sans
  }
  YAML::Node whole;
  try {
    whole = LoadYamlFile(path);
  } catch (const YAML::Exception& broken) {
    // La seule exception qu'on attrape, et pour la même raison qu'ailleurs : un
    // fichier abîmé ne doit pas empêcher de jouer.
    ro::core::LogWarning(
        "historique de conversation : fichier illisible ({}) — la conversation de la partie "
        "d'avant ne sera pas rendue",
        broken.what());
    return past;
  }
  // 🔴 UNE RACINE QUI N'EST PAS UNE TABLE N'A PAS DE CHAMPS : l'indexer lève
  // `BadSubscript` HORS du `try`, et un fichier réduit à un mot empêchait de
  // lancer le jeu.
  if (!whole.IsMap()) {
    return past;
  }
  past.saved_at = whole[kSavedAtField].as<std::string>(std::string());
  ReadTexts(whole[kWhisperedField], &past.whispered);
  ReadTexts(whole[kTypedField], &past.typed);
  const YAML::Node listed = whole[kLinesField];
  if (!listed || !listed.IsSequence()) {
    return past;
  }
  for (const YAML::Node& entry : listed) {
    if (!entry.IsMap()) {
      continue;
    }
    // ⚠ LA LIGNE SANS TYPE EST IGNORÉE, pas rattachée d'office à un type par
    // défaut : elle ne sait plus dans quel onglet aller, et la poser partout
    // serait pire que de ne pas la poser. C'est le sort des lignes aux anciennes
    // clés, dont le genre ne dit pas la case.
    const std::optional<ro::sim::ChatType> type = ChatTypeOf(entry[kTypeField].as<int>(kNoType));
    if (!type.has_value()) {
      continue;
    }
    PastChatLine line;
    line.type = *type;
    line.text = entry[kTextField].as<std::string>(std::string());
    if (line.text.empty()) {
      continue;
    }
    const std::string at = entry[kTimeField].as<std::string>(std::string());
    if (at.size() == kTimeTextSize) {
      line.hour = TwoDigitsAt(at, kHourAt);
      line.minute = TwoDigitsAt(at, kMinuteAt);
      line.second = TwoDigitsAt(at, kSecondAt);
    }
    // Absente, elle vaut zéro : la couleur ordinaire du fil.
    line.color = entry[kColorField].as<std::uint32_t>(0U);
    past.lines.push_back(std::move(line));
  }
  return past;
}

bool SaveChatHistory(const std::filesystem::path& client_root,
                     const std::vector<PastChatLine>& lines, int keep, std::string_view saved_at,
                     std::span<const std::string> whispered, std::span<const std::string> typed) {
  const auto kept =
      static_cast<std::size_t>(std::clamp(keep, kSmallestChatHistory, kLargestChatHistory));
  const std::size_t first = lines.size() > kept ? lines.size() - kept : 0;

  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kSavedAtField << YAML::Value << std::string(saved_at);
  WriteTexts(out, kWhisperedField, whispered, /*flow=*/true);
  WriteTexts(out, kTypedField, typed, /*flow=*/false);
  out << YAML::Key << kLinesField << YAML::Value << YAML::BeginSeq;
  for (std::size_t rank = first; rank < lines.size(); ++rank) {
    const PastChatLine& line = lines[rank];
    out << YAML::BeginMap;
    out << YAML::Key << kTypeField << YAML::Value << static_cast<int>(line.type);
    out << YAML::Key << kTimeField << YAML::Value
        << ro::core::Printed("%02d:%02d:%02d", line.hour, line.minute, line.second);
    out << YAML::Key << kTextField << YAML::Value << line.text;
    // ⚠ ÉCRITE SEULEMENT QUAND IL Y EN A UNE. Un « couleur: 0 » sur chaque
    // ligne alourdirait un fichier que le joueur peut ouvrir, pour dire
    // exactement ce que son absence dit déjà.
    if (line.color != 0) {
      out << YAML::Key << kColorField << YAML::Value << line.color;
    }
    out << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;

  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(ChatHistoryPath(client_root), text);
      !written) {
    ro::core::LogWarning(
        "historique de conversation : écriture impossible ({}) — ce qui s'est dit cette partie "
        "ne sera pas retrouvé à la prochaine",
        ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

void ForgetChatHistory(const std::filesystem::path& client_root) {
  std::error_code ignored;
  // L'échec est SANS SUITE : le fichier n'existait pas, ou le dossier est en
  // lecture seule. Dans les deux cas, insister n'apprendrait rien au joueur.
  std::filesystem::remove(ChatHistoryPath(client_root), ignored);
}

}  // namespace moonclient
