#include "ro/core/log.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <exception>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "ro/core/build_stamp.h"
#include "ro/core/console.h"
#include "ro/core/utf8_path.h"

namespace ro::core {
namespace {

// LE JOURNAL EN MÉMOIRE, et pourquoi ce n'est pas un puits spdlog.
//
// Un puits reçoit la ligne DÉJÀ MISE EN FORME : horodatage, gravité et message
// collés en une chaîne. Une fenêtre qui veut colorer les erreurs devrait alors
// la redécouper — c'est-à-dire relire ce qu'on venait d'écrire, et se tromper
// le jour où le motif change. On garde donc les trois champs séparés, à
// l'endroit où on les a encore : le point unique par lequel tout passe.
struct Memory {
  std::mutex guard;
  std::deque<LoggedLine> kept;
  // Ce qui a été écrit DEPUIS LE DÉBUT, y compris ce que la borne a fait
  // tomber. C'est ce compteur, et non la taille de la file, qui dit à une
  // fenêtre s'il y a du nouveau.
  std::size_t written = 0;
  std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
};

[[nodiscard]] Memory& TheMemory() {
  // ⚠ CONSTRUITE À LA PREMIÈRE DEMANDE, et non à l'initialisation statique : le
  // journal est écrit depuis des constructeurs globaux dans les tests, et
  // l'ordre entre deux objets statiques de deux unités de compilation n'est pas
  // défini. Une ligne écrite avant la construction de la file serait perdue ou
  // pire.
  static Memory memory;
  return memory;
}

[[nodiscard]] spdlog::level::level_enum SpdlogLevelOf(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return spdlog::level::debug;
    case LogLevel::kInfo:
      return spdlog::level::info;
    case LogLevel::kWarning:
      return spdlog::level::warn;
    case LogLevel::kError:
      return spdlog::level::err;
  }
  return spdlog::level::info;
}

// La ligne qui clôt un journal : voir `EndLogFile`. Sa formulation est relue au
// lancement suivant, donc elle ne change pas sans que la relecture suive.
constexpr std::string_view kSessionEndMarker = "journal fermé proprement";
// L'annonce, en première ligne, que ce journal se clôt par la ligne de fin.
//
// ⚠ SANS ELLE, UNE VERSION ANTÉRIEURE SE LIRAIT COMME UNE MORT MUETTE : elle
// n'écrivait aucune ligne de fin, et le premier lancement après une mise à
// jour aurait crié à l'arrêt sans prévenir. Un journal qui ne l'annonce pas ne
// se juge donc pas sur sa fin.
constexpr std::string_view kSessionEndPromise = "clos par une ligne de fin";
// Ce que le filet écrit sur un plantage : voir `crash_report.cpp`.
constexpr std::string_view kCrashMarker = "PLANTAGE";
// `moonclient.log` devient `moonclient.precedent.log`.
constexpr std::string_view kPreviousLogInfix = ".precedent";
// Ce qu'on relit de la fin du journal précédent : la ligne de fin y est la
// dernière, et la dernière ligne utile se tient juste avant.
constexpr std::uintmax_t kPreviousTailBytes = 4096;
// Ce qu'on relit de son début : la première ligne, qui porte l'annonce.
constexpr std::uintmax_t kPreviousHeadBytes = 1024;

// LA PORTE DU FICHIER : un puits qui en contient un autre, ou aucun.
//
// C'est elle que tient le logger, et non le fichier lui-même : la fermer, c'est
// vider la porte, sous son verrou, sans toucher à la liste des puits du logger.
//
// 🔴 ELLE VIDE SUR DISQUE CHAQUE LIGNE QU'ELLE REÇOIT, avant que le logger ne
// passe au puits suivant. Le vidage du logger, lui, n'a lieu qu'après que TOUS
// ses puits ont écrit : une console qui bloque ou qui plante laissait la ligne
// dans le tampon du fichier, et le journal restait vide.
class FlushingGate final : public spdlog::sinks::dist_sink_mt {
 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    spdlog::sinks::dist_sink_mt::sink_it_(msg);
    flush_();
  }
};

[[nodiscard]] const std::shared_ptr<FlushingGate>& FileGate() {
  static const auto gate = std::make_shared<FlushingGate>();
  return gate;
}

enum class PreviousLogAside : std::uint8_t {
  kNone,   // aucun journal précédent
  kMoved,  // mis de côté, à relire
  kInUse,  // tenu ouvert par un autre processus : il sera écrasé
};

struct PreviousLog {
  PreviousLogAside aside = PreviousLogAside::kNone;
  std::filesystem::path path;
};

[[nodiscard]] PreviousLog SetPreviousLogAside(const std::filesystem::path& file_path) {
  PreviousLog previous;
  std::error_code failed;
  if (!std::filesystem::exists(file_path, failed)) {
    return previous;
  }
  previous.path = file_path;
  previous.path.replace_extension();
  previous.path += kPreviousLogInfix;
  previous.path += file_path.extension();
  // L'avant-dernier s'efface d'abord : `rename` ne remplace pas partout.
  std::filesystem::remove(previous.path, failed);
  std::filesystem::rename(file_path, previous.path, failed);
  previous.aside = failed ? PreviousLogAside::kInUse : PreviousLogAside::kMoved;
  return previous;
}

// La dernière ligne non vide d'un texte, sans son retour chariot.
[[nodiscard]] std::string_view LastLine(std::string_view text) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
    text.remove_suffix(1);
  }
  const std::size_t start = text.rfind('\n');
  return start == std::string_view::npos ? text : text.substr(start + 1);
}

// `count_bytes` octets du fichier à partir de `start_byte`, moins s'il est court.
[[nodiscard]] std::string ReadSpan(std::ifstream& file, std::uintmax_t start_byte,
                                   std::uintmax_t count_bytes) {
  file.clear();
  file.seekg(static_cast<std::streamoff>(start_byte));
  std::string read(static_cast<std::size_t>(count_bytes), '\0');
  file.read(read.data(), static_cast<std::streamsize>(read.size()));
  read.resize(static_cast<std::size_t>(file.gcount()));
  return read;
}

// Dit au journal neuf comment le précédent s'est terminé, quand ce n'est pas
// par une sortie voulue.
void ReviewPreviousLog(const std::filesystem::path& previous) {
  const std::string shown = Utf8FromPath(previous.filename());
  std::error_code failed;
  const std::uintmax_t size_bytes = std::filesystem::file_size(previous, failed);
  if (failed) {
    return;
  }
  if (size_bytes == 0) {
    LogWarning(
        "lancement précédent : son journal ({}) est VIDE — le processus est mort avant sa première "
        "ligne, ou le dossier laissait créer le fichier sans y écrire (antivirus, dossier protégé)",
        shown);
    return;
  }
  std::ifstream file(previous, std::ios::binary);
  const std::string head = ReadSpan(file, 0, std::min(size_bytes, kPreviousHeadBytes));
  const std::uintmax_t tail_start_byte =
      size_bytes > kPreviousTailBytes ? size_bytes - kPreviousTailBytes : 0;
  const std::string tail = ReadSpan(file, tail_start_byte, size_bytes - tail_start_byte);
  if (tail.find(kSessionEndMarker) != std::string::npos) {
    return;
  }
  // ⚠ LE PLANTAGE AVANT L'ANNONCE : une version antérieure l'écrivait déjà.
  if (tail.find(kCrashMarker) != std::string::npos) {
    LogWarning("lancement précédent : il a planté — son rapport plantage-*.txt et {} sont à côté",
               shown);
    return;
  }
  if (head.find(kSessionEndPromise) == std::string::npos) {
    return;
  }
  LogWarning(
      "lancement précédent : arrêté SANS PRÉVENIR, ni fin ni plantage rapporté — tué de "
      "l'extérieur (antivirus, gestionnaire des tâches, arrêt de Windows) ou faute que le filet ne "
      "voit pas. Sa dernière ligne, dans {} : « {} »",
      shown, LastLine(tail));
}

}  // namespace

std::string_view ToString(LogLevel level) noexcept {
  switch (level) {
    case LogLevel::kDebug:
      return "détail";
    case LogLevel::kInfo:
      return "info";
    case LogLevel::kWarning:
      return "alerte";
    case LogLevel::kError:
      return "erreur";
  }
  return "info";
}

Result<void> InitLogging(const std::filesystem::path& file_path) {
  // ⚠ AVANT LE PREMIER PUITS, ET C'EST ICI QUE CELA VA : ce journal POSSÈDE la
  // console, c'est lui qui y écrit du français, et une console qui décode nos
  // octets dans une autre page rend sa moitié affichée illisible. Voir
  // `UseUtf8Console`.
  UseUtf8Console();

  Result<void> opened;
  PreviousLog previous;
  std::vector<spdlog::sink_ptr> sinks;
  // 🔴 LE FICHIER EN PREMIER : les puits s'appellent dans l'ordre, et c'est lui
  // qui doit tenir une ligne si la console, derrière, ne rend pas la main — ce
  // que seul garantit le vidage de sa porte, voir `FlushingGate`.
  sinks.push_back(FileGate());
  if (!file_path.empty()) {
    previous = SetPreviousLogAside(file_path);
    // ⚠ spdlog SIGNALE PAR EXCEPTION un fichier qu'il ne peut ouvrir. Laissée
    // filer, elle tuait le client avant sa première ligne et avant toute boîte.
    try {
      FileGate()->set_sinks(
          {std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path.string(), true)});
    } catch (const std::exception& problem) {
      opened =
          Fail(ErrorCode::kIoFailure,
               std::format("{} ne s'ouvre pas en écriture : {}",
                           Utf8FromPath(std::filesystem::absolute(file_path)), problem.what()));
    }
  }
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

  auto logger = std::make_shared<spdlog::logger>("moonclient", sinks.begin(), sinks.end());
  // `%s:%#` est le fichier et la ligne qui ont écrit. Le motif ne les portait
  // pas : une alerte disait ce qui n'allait pas, jamais d'où elle parlait, et
  // la chercher demandait de deviner sa formulation exacte dans les sources.
  logger->set_pattern("[%H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
  logger->set_level(spdlog::level::debug);
  // TOUT est vidé sur disque à l'écriture, et pas seulement les alertes.
  //
  // ⚠ MESURÉ : un client arrêté brutalement laissait un `moonclient.log` de
  // ZÉRO octet. Les lignes d'information attendaient la destruction du logger,
  // qui n'a jamais lieu quand le processus est tué — ou quand il plante, c'est-
  // à-dire exactement le cas où ce fichier est censé répondre. Le journal
  // s'arrêtait donc juste avant ce qui expliquait tout, et le motif « vider à
  // partir de l'alerte » ne protégeait que la moitié qu'on lit le moins.
  //
  // Le coût est nul à notre débit : un client en écrit quelques dizaines de
  // lignes par partie. Le jour où une boucle en écrirait par image, c'est la
  // boucle qu'il faudra reprendre, pas ce réglage.
  logger->flush_on(spdlog::level::debug);

  spdlog::set_default_logger(std::move(logger));

  // LA PREMIÈRE LIGNE DIT QUI PARLE. Un journal qu'un joueur envoie sans savoir
  // quelle version il jouait oblige à le lui redemander, souvent des jours plus
  // tard, et parfois à chercher une panne déjà corrigée.
  LogInfo("moonclient {} — revision {} (arbre {}) — journal {}", kBuildVersion, kBuildCommit,
          kBuildTree, kSessionEndPromise);
  if (!opened) {
    LogError("journal : {} — rien de ce qui suit n'atteint le disque", Describe(opened.error()));
  }
  switch (previous.aside) {
    case PreviousLogAside::kNone:
      break;
    case PreviousLogAside::kMoved:
      ReviewPreviousLog(previous.path);
      break;
    case PreviousLogAside::kInUse:
      LogWarning(
          "journal : le précédent est encore tenu ouvert — un autre client tourne-t-il déjà dans "
          "ce dossier ? Ce journal-ci l'écrase");
      break;
  }
  return opened;
}

void EndLogFile(std::string_view why) {
  LogInfo("{} : {}", kSessionEndMarker, why);
  // ⚠ PAR LA PORTE, ET NON EN RETIRANT LE PUITS DU LOGGER : un autre fil peut
  // écrire au même instant, et la liste des puits d'un logger ne se modifie pas
  // sous ses pieds. La porte, elle, se vide sous son propre verrou.
  FileGate()->set_sinks({});
}

void WriteLog(LogLevel level, const std::source_location& where, std::string text) {
  spdlog::default_logger_raw()->log(
      spdlog::source_loc{where.file_name(), static_cast<int>(where.line()), where.function_name()},
      SpdlogLevelOf(level), "{}", text);

  // ⚠ LA MÉMOIRE GARDE TOUT, même ce que spdlog vient d'écarter par son
  // niveau. Ce n'est pas une incohérence : le niveau de spdlog gouverne la
  // CONSOLE et le FICHIER, tandis que la fenêtre de journal a son propre
  // filtre, par gravité, que le joueur retourne sans relancer le programme. Une
  // ligne écartée ici ne pourrait plus jamais être montrée.
  Memory& memory = TheMemory();
  const std::lock_guard<std::mutex> held(memory.guard);
  const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - memory.started);
  ++memory.written;
  memory.kept.push_back(LoggedLine{.serial = memory.written,
                                   .level = level,
                                   .since_start_ms = static_cast<std::uint32_t>(since.count()),
                                   .text = std::move(text)});
  while (memory.kept.size() > kKeptLogLines) {
    memory.kept.pop_front();
  }
}

std::vector<LoggedLine> RecentLogLines(std::size_t at_most) {
  Memory& memory = TheMemory();
  const std::lock_guard<std::mutex> held(memory.guard);
  const std::size_t kept = std::min(at_most, memory.kept.size());
  // Les PLUS RÉCENTES : une fenêtre qui suit le journal veut la fin, et
  // rendre le début lui montrerait l'ouverture du programme jusqu'à sa
  // fermeture.
  return {memory.kept.end() - static_cast<std::ptrdiff_t>(kept), memory.kept.end()};
}

std::optional<std::vector<LoggedLine>> TryRecentLogLines(std::size_t at_most) {
  Memory& memory = TheMemory();
  const std::unique_lock<std::mutex> held(memory.guard, std::try_to_lock);
  if (!held.owns_lock()) {
    return std::nullopt;
  }
  const std::size_t kept = std::min(at_most, memory.kept.size());
  return std::vector<LoggedLine>(memory.kept.end() - static_cast<std::ptrdiff_t>(kept),
                                 memory.kept.end());
}

std::size_t LoggedLineCount() {
  Memory& memory = TheMemory();
  const std::lock_guard<std::mutex> held(memory.guard);
  return memory.written;
}

}  // namespace ro::core
