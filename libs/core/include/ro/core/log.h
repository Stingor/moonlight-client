#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "ro/core/build_flavor.h"
#include "ro/core/error.h"

namespace ro::core {

// Installe les puits de journalisation : console, fichier si un chemin est
// donné, et la MÉMOIRE — voir `RecentLogLines`. Appelée une fois au démarrage ;
// sans appel, l'écriture se fait sur la console avec les réglages par défaut,
// ce qui convient aux tests.
//
// Rend une erreur si le FICHIER ne s'ouvre pas en écriture. La console et la
// mémoire sont posées quand même : le client continue, mais personne ne lira
// ce qu'il dit, et c'est à l'appelant de le faire savoir au joueur.
//
// 🔴 LE JOURNAL DU LANCEMENT PRÉCÉDENT N'EST PAS ÉCRASÉ : il est mis de côté
// sous `<nom>.precedent<ext>`, puis relu. S'il ne finit ni par la ligne que
// pose `EndLogFile`, ni par un plantage rapporté, le nouveau journal le dit en
// alerte, avec sa dernière ligne. C'est la seule trace d'un processus tué de
// l'extérieur — antivirus, gestionnaire des tâches — que rien d'autre ne voit
// mourir, et le joueur qui relançait l'effaçait.
[[nodiscard]] Result<void> InitLogging(const std::filesystem::path& file_path = {});

// Écrit la ligne de FIN, `why` disant pourquoi, puis ferme le fichier ; la
// console et la mémoire continuent.
//
// ⚠ UN JOURNAL SANS CETTE LIGNE EST CELUI D'UN PROCESSUS MORT SANS PRÉVENIR :
// c'est ce que relit `InitLogging` au lancement suivant. Elle se pose donc sur
// TOUTE sortie voulue — et avant une relance, qui rouvre le même fichier depuis
// un autre processus : deux processus qui y écrivent ensemble s'écrasent.
void EndLogFile(std::string_view why);

// La gravité d'une ligne.
//
// Quatre, et pas davantage : une échelle plus fine ne se respecte pas — chacun
// place le même message ailleurs — et la seule question qu'on pose vraiment à
// un journal est « qu'est-ce qui ne va pas », suivie de « qu'est-ce qui s'est
// passé ».
//
// 🔴 CE QUI SUIT EST LA RÈGLE, et elle n'était écrite nulle part. Sans elle,
// chacun place le même fait à un rang différent — et un filtre par gravité ne
// trie que ce qu'une règle commune y a rangé.
//
// - `kError` : le programme n'a pas pu faire ce qu'on lui demandait ET le
//   joueur y perd quelque chose MAINTENANT, sans repli et sans réessai. Une
//   écriture qui ne se rejouera jamais, une liaison qui tombe, un plantage.
//   Rare.
// - `kWarning` : quelque chose manque ou a échoué, mais on continue sur un
//   repli NOMMÉ. C'est la gravité du « ça marchera, en moins bien » : une
//   ressource absente de l'installation, une table illisible. ⚠ Une écriture
//   atomique ratée que le prochain changement rejouera est ICI, pas au-dessus.
// - `kInfo` : un fait PONCTUEL qu'on relit pour reconstituer une partie —
//   démarrage, connexion, personnage, carte, compte de chargement. Doit rester
//   rare ; un compte qui vaut une constante de compilation n'en est pas un.
// - `kDebug` : le détail utile à qui débogue CE module, bruit pour tous les
//   autres. ⚠ Il n'est pas éteint — `InitLogging` pose `set_level(debug)` — il
//   est RANGÉ : la fenêtre de journal le garde derrière son filtre.
//
// Trois règles de forme, qui valent aux quatre niveaux :
//
// 1. « sujet : ce qui s'est passé, et ce que le joueur y perd ». Un message qui
//    s'arrête à l'erreur (« X : {} ») n'apprend rien à qui lit un journal sans
//    le code — et le repli, lui, était écrit dans le commentaire d'à côté.
// 2. Deux échecs différents ne portent JAMAIS le même libellé : la fenêtre de
//    journal ne garde que le TEXTE, pas le `fichier:ligne`.
// 3. En français accentué, comme le reste du dépôt. Ce n'est pas cosmétique :
//    « effet né sur la cible » écrit sans accent se lit comme une négation.
//
// Et une règle de CADENCE, qui découle de `flush_on(debug)` plus bas : toute
// ligne part sur le disque à l'écriture. Une trace qu'on peut atteindre depuis
// la boucle de jeu doit être bornée par un témoin qui borne le NOMBRE
// d'écritures — un ensemble de « déjà dit », un drapeau qui ne se rebaisse pas
// — jamais par une garde qui n'en borne que la CONDITION. Un `if (absent)`
// reste vrai à chaque image, et s'écrit donc soixante fois par seconde.
enum class LogLevel : std::uint8_t { kDebug, kInfo, kWarning, kError };

[[nodiscard]] std::string_view ToString(LogLevel level) noexcept;

// Une ligne telle qu'on la GARDE, pour qui veut la montrer.
//
// Le texte est celui du message seul, sans horodatage ni gravité : c'est à
// l'affichage de les mettre en forme comme il l'entend, et les coller ici
// obligerait une fenêtre à les découper pour colorer le niveau.
struct LoggedLine {
  // ⚠ SON NUMÉRO D'ÉCRITURE, et il ne se réemploie jamais. C'est ce qui permet
  // à une sélection de TENIR pendant que le journal grandit : désigner une ligne
  // par son rang dans la liste la ferait glisser à chaque ligne écrite, et l'on
  // copierait autre chose que ce qu'on avait cliqué.
  //
  // Le premier vaut UN : zéro reste disponible pour dire « aucune ligne ».
  std::uint64_t serial = 0;
  LogLevel level = LogLevel::kInfo;
  // Millisecondes depuis le démarrage du programme.
  std::uint32_t since_start_ms = 0;
  std::string text;
};

// ⚠ LE FORMAT ET L'ENDROIT D'OÙ IL VIENT, ensemble.
//
// Une fonction variadique ne peut pas porter un paramètre par défaut APRÈS son
// paquet : `std::source_location::current()` n'y a pas de place. L'emballer
// avec le format le fait entrer par la porte de devant, et le site d'appel ne
// voit rien — `LogInfo("{} octets", n)` s'écrit comme avant.
//
// Sans cela, chaque ligne du journal perdrait son `fichier:ligne`, et c'est ce
// qui fait la différence entre « archive ignorée » et « archive ignorée, dit
// par vfs.cpp:208 ».
template <typename... Args>
struct FormatFrom {
  template <typename Text>
    requires std::convertible_to<const Text&, std::format_string<Args...>>
  consteval FormatFrom(  // NOLINT(google-explicit-constructor) — doit être implicite
      const Text& text, std::source_location from = std::source_location::current())
      : format(text), where(from) {}

  std::format_string<Args...> format;
  std::source_location where;
};

// Le point unique par lequel tout passe. Les quatre fonctions ci-dessous ne
// font que le nommer.
void WriteLog(LogLevel level, const std::source_location& where, std::string text);

// ⚠ LA MISE AU POINT N'EXISTE QU'EN DEBUG (`kDevTools`). Ailleurs l'appel ne
// formate rien et n'écrit rien : un relevé semé pour comprendre une image — le
// recensement de la scène — ne doit ni coûter ni remplir le journal du joueur.
template <typename... Args>
void LogDebug([[maybe_unused]] FormatFrom<std::type_identity_t<Args>...> from,
              [[maybe_unused]] Args&&... args) {
  if constexpr (kDevTools) {
    WriteLog(LogLevel::kDebug, from.where, std::format(from.format, std::forward<Args>(args)...));
  }
}

template <typename... Args>
void LogInfo(FormatFrom<std::type_identity_t<Args>...> from, Args&&... args) {
  WriteLog(LogLevel::kInfo, from.where, std::format(from.format, std::forward<Args>(args)...));
}

template <typename... Args>
void LogWarning(FormatFrom<std::type_identity_t<Args>...> from, Args&&... args) {
  WriteLog(LogLevel::kWarning, from.where, std::format(from.format, std::forward<Args>(args)...));
}

template <typename... Args>
void LogError(FormatFrom<std::type_identity_t<Args>...> from, Args&&... args) {
  WriteLog(LogLevel::kError, from.where, std::format(from.format, std::forward<Args>(args)...));
}

// Combien de lignes le journal en mémoire retient au plus.
//
// ⚠ IL EN RETIENT UN NOMBRE BORNÉ, et c'est ce qui en fait autre chose que le
// fichier. Une partie de plusieurs heures en écrit des dizaines de milliers ;
// les garder toutes ferait enfler le programme sans que personne ne remonte
// jamais si loin. Le fichier, lui, garde TOUT — c'est sa raison d'être, et ce
// qui rend cette borne sans conséquence.
inline constexpr std::size_t kKeptLogLines = 4000;

// Les dernières lignes écrites, de la plus ANCIENNE à la plus récente.
//
// Rend au plus `at_most` lignes, et les plus récentes quand il y en a
// davantage : une fenêtre qui suit le journal veut la fin, pas le début.
[[nodiscard]] std::vector<LoggedLine> RecentLogLines(std::size_t at_most = kKeptLogLines);

// LA MÊME CHOSE, SANS JAMAIS ATTENDRE : rend rien si le journal est en cours
// d'écriture.
//
// 🔴 C'EST LA SEULE LECTURE PERMISE DEPUIS UN GESTIONNAIRE DE FAUTE. Une faute
// survenue PENDANT une écriture laisse le verrou tenu par le fil même qui
// rapporte : le reprendre l'y bloquerait pour toujours, et le client se
// figerait au lieu d'écrire son rapport.
[[nodiscard]] std::optional<std::vector<LoggedLine>> TryRecentLogLines(
    std::size_t at_most = kKeptLogLines);

// Combien de lignes ont été écrites DEPUIS LE DÉBUT, celles que la borne a déjà
// fait tomber comprises.
//
// C'est le témoin qu'une fenêtre lit pour savoir s'il y a du nouveau sans
// recopier toute la liste à chaque image.
[[nodiscard]] std::size_t LoggedLineCount();

}  // namespace ro::core
