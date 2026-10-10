#include "ro/core/log.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "ro/core/build_stamp.h"
#include "ro/core/console.h"

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

void InitLogging(const std::filesystem::path& file_path) {
  // ⚠ AVANT LE PREMIER PUITS, ET C'EST ICI QUE CELA VA : ce journal POSSÈDE la
  // console, c'est lui qui y écrit du français, et une console qui décode nos
  // octets dans une autre page rend sa moitié affichée illisible. Voir
  // `UseUtf8Console`.
  UseUtf8Console();

  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  if (!file_path.empty()) {
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path.string(), true));
  }

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
  LogInfo("moonclient {} — revision {} (arbre {})", kBuildVersion, kBuildCommit, kBuildTree);
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
