#include "chat/chat_log_file.h"

#include <array>
#include <cstdio>
#include <system_error>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

namespace moonclient {
namespace {

// 🔴 LE BOM UTF-8, ET IL N'EST PAS DÉCORATIF. Nos lignes sont en UTF-8 ; sans
// lui, le Bloc-notes les lit dans l'encodage du système et tous les accents —
// et tout le coréen — sortent en charabia. Le fichier du client d'origine est
// écrit dans l'encodage du système : il n'avait pas ce choix à faire.
constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";

// ⚠ CRLF, MÊME SUR LE SYSTÈME QUI N'EN A PAS BESOIN. Ce fichier est fait pour
// être ouvert par le joueur, avec l'éditeur qu'il a sous la main — et sur
// Windows, celui-là affiche encore une seule ligne interminable quand il ne
// trouve que des sauts simples.
constexpr std::string_view kLineEnd = "\r\n";

// Ce que le système refuse dans un nom de fichier, plus les deux caractères qui
// mènent ailleurs qu'au dossier voulu.
constexpr std::string_view kForbiddenInName = "\\/:*?\"<>|";
// Sous cette valeur, un octet est une commande de terminal et non un caractère.
constexpr unsigned char kFirstPrintable = 0x20;

// Un nom qui finit par un espace ou un point n'est pas créable sous Windows, et
// le nom rendu est alors silencieusement celui d'à côté.
[[nodiscard]] bool TrailingRefused(char letter) { return letter == ' ' || letter == '.'; }

[[nodiscard]] std::filesystem::path DumpPathFor(const std::filesystem::path& folder,
                                                std::string_view stem) {
  // Rotation à la manière du client : le nom simple, puis `_001`, `_002`… Deux
  // `/savechat` d'affilée ne doivent pas s'écraser l'un l'autre — c'est
  // typiquement ce qu'on fait avant et après un évènement.
  // ⚠ LE NOM D'ONGLET OU DE SALON EST EN UTF-8 : passé tel quel au chemin, il
  // serait lu dans la page ANSI et « Général » deviendrait « GÃ©nÃ©ral ».
  std::filesystem::path path =
      folder / ro::core::PathFromUtf8(std::string(kChatDumpPrefix) + std::string(stem) +
                                      std::string(kChatLogSuffix));
  std::error_code missing;
  for (int rank = 1; rank < kLargestChatDumpRotation && std::filesystem::exists(path, missing);
       ++rank) {
    std::array<char, 16> suffix{};
    std::snprintf(suffix.data(), suffix.size(), "_%03d", rank);
    path = folder / ro::core::PathFromUtf8(std::string(kChatDumpPrefix) + std::string(stem) +
                                           suffix.data() + std::string(kChatLogSuffix));
  }
  return path;
}

void AppendLine(std::string& content, const PastChatLine& line) {
  content += ChatLogLine(line);
  content += kLineEnd;
}

// Le dossier des dumps, créé au besoin.
//
// ⚠ RÉCURSIF, contrairement au `_mkdir` du client : c'est ce qui rend son
// `/savechat` muet quand un dossier intermédiaire manque.
[[nodiscard]] ChatLogDump StartDump(const std::filesystem::path& client_root) {
  ChatLogDump done{.folder = ChatLogFolder(client_root)};
  std::error_code failed;
  std::filesystem::create_directories(done.folder, failed);
  return done;
}

// Écrit un dump sous le premier nom libre, et le compte dans `done`.
void WriteDump(ChatLogDump& done, std::string_view stem, std::string_view content) {
  const std::filesystem::path path = DumpPathFor(done.folder, stem);
  if (ro::core::WriteFileAtomically(path, content).has_value()) {
    ++done.files;
  } else {
    ro::core::LogWarning("journal de conversation : {} n'a pas pu être écrit",
                         ro::core::Utf8FromPath(path));
  }
}

}  // namespace

std::filesystem::path ChatLogFolder(const std::filesystem::path& client_root) {
  return client_root / kChatLogFolderName;
}

std::string SafeFileStem(std::string_view name) {
  std::string safe;
  safe.reserve(name.size());
  for (const char letter : name) {
    const auto raw = static_cast<unsigned char>(letter);
    safe.push_back(raw < kFirstPrintable || kForbiddenInName.find(letter) != std::string_view::npos
                       ? '_'
                       : letter);
  }
  while (!safe.empty() && TrailingRefused(safe.back())) {
    safe.pop_back();
  }
  return safe.empty() ? std::string(kFallbackFileStem) : safe;
}

std::string ChatLogLine(const PastChatLine& line) {
  std::array<char, 16> stamp{};
  std::snprintf(stamp.data(), stamp.size(), "[%02u:%02u:%02u] ", line.hour, line.minute,
                line.second);
  return std::string(stamp.data()) + line.text;
}

ChatLogDump DumpChatLogs(const std::filesystem::path& client_root, std::span<const ChatTab> tabs,
                         std::span<const ChatLogPart> parts) {
  ChatLogDump done = StartDump(client_root);
  for (const ChatTab& tab : tabs) {
    std::string content(kUtf8Bom);
    int kept = 0;
    for (const ChatLogPart& part : parts) {
      // Le titre attend d'avoir quelque chose à coiffer : l'écrire d'abord
      // laisserait un repère seul dans un fichier où rien n'a passé le filtre.
      bool titled = part.heading.empty();
      for (const PastChatLine& line : part.lines) {
        if (!tab.Accepts(line.type)) {
          continue;
        }
        if (!titled) {
          content += part.heading;
          content += kLineEnd;
          titled = true;
        }
        AppendLine(content, line);
        ++kept;
      }
    }
    if (kept == 0) {
      continue;  // un onglet vide ne mérite pas un fichier vide
    }
    WriteDump(done, SafeFileStem(tab.name), content);
  }
  return done;
}

ChatLogDump DumpChatRoomLog(const std::filesystem::path& client_root, std::string_view room_title,
                            std::span<const PastChatLine> lines) {
  if (lines.empty()) {
    return ChatLogDump{.folder = ChatLogFolder(client_root)};
  }
  ChatLogDump done = StartDump(client_root);
  std::string content(kUtf8Bom);
  for (const PastChatLine& line : lines) {
    AppendLine(content, line);
  }
  WriteDump(done, std::string(kChatRoomDumpPrefix) + SafeFileStem(room_title), content);
  return done;
}

void ChatLogWriter::Open(const std::filesystem::path& client_root, std::string_view day,
                         std::string_view mark) {
  Close();
  folder_ = ChatLogFolder(client_root);
  day_ = day;
  mark_ = mark;
  serving_ = true;
}

bool ChatLogWriter::Begin() {
  std::error_code failed;
  std::filesystem::create_directories(folder_, failed);
  const std::filesystem::path path =
      folder_ / (std::string(kChatLogPrefix) + day_ + std::string(kChatLogSuffix));

  std::error_code missing;
  // ⚠ LE BOM NE S'ÉCRIT QU'À LA CRÉATION. Le remettre en rouvrant le journal du
  // jour le ferait paraître au MILIEU du fichier, où il n'est plus une marque
  // d'encodage mais un caractère invisible que l'éditeur montre.
  const bool starting = !std::filesystem::exists(path, missing);
  file_.open(path, std::ios::binary | std::ios::app);
  if (!file_.is_open()) {
    // ⚠ DÉSARMÉ POUR DE BON, et c'est `ChatTabsView::KeepLog` qui le rend vrai :
    // tant qu'il relisait `open()` pour savoir s'il avait déjà agi, il réarmait
    // ce service à l'image suivante et cette plainte repartait à chaque ligne
    // dite. Le journal est un service : son absence ne doit ni empêcher de
    // jouer, ni se plaindre indéfiniment.
    ro::core::LogWarning(
        "journal de conversation : {} ne s'ouvre pas — la conversation de cette partie ne sera "
        "pas conservée",
        ro::core::Utf8FromPath(path));
    serving_ = false;
    return false;
  }
  path_ = path;
  if (starting) {
    file_ << kUtf8Bom;
  }
  if (!mark_.empty()) {
    file_ << mark_ << kLineEnd;
  }
  ro::core::LogInfo("journal de conversation : ouvert dans {}", ro::core::Utf8FromPath(path_));
  return true;
}

void ChatLogWriter::Write(const PastChatLine& line) {
  if (!serving_) {
    return;
  }
  if (!file_.is_open() && !Begin()) {
    return;
  }
  file_ << ChatLogLine(line) << kLineEnd;
  // ⚠ VIDÉ À CHAQUE LIGNE, et c'est tout l'intérêt du journal : ce qui est dit
  // doit être sur le disque avant la suite. Un tampon gardé en mémoire
  // disparaîtrait précisément dans le cas où l'on tient à relire — la sortie
  // brutale.
  file_.flush();
}

void ChatLogWriter::Close() {
  if (file_.is_open()) {
    file_.close();
  }
  path_.clear();
  folder_.clear();
  day_.clear();
  mark_.clear();
  serving_ = false;
}

}  // namespace moonclient
