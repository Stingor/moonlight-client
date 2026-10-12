#include "game/settings_file.h"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "game/yaml_load.h"

namespace moonclient {
namespace {

// L'en-tête du fichier. Il s'adresse à qui l'ouvrira dans un éditeur de texte —
// c'est le cas d'usage qui a fait choisir YAML plutôt qu'un format compact.
constexpr std::string_view kFileHeader =
    "# Les réglages de moonclient.\n"
    "#\n"
    "# Une clé absente ou illisible prend sa valeur d'origine ; une clé que ce\n"
    "# client ne connaît pas est CONSERVÉE telle quelle. Le fichier se relit et\n"
    "# se corrige donc à la main sans rien perdre.\n";

// Lit le document, ou rend une carte vide. `broken` dit si c'est un ÉCHEC DE
// LECTURE — un fichier abîmé — et non une simple absence : les deux mènent aux
// défauts, mais seul le premier interdit de réécrire.
[[nodiscard]] YAML::Node ReadWhole(const std::filesystem::path& path, bool* broken) {
  if (broken != nullptr) {
    *broken = false;
  }
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return YAML::Node(YAML::NodeType::Map);
  }
  try {
    YAML::Node whole = LoadYamlFile(path);
    return whole.IsMap() ? whole : YAML::Node(YAML::NodeType::Map);
  } catch (const YAML::Exception& failed) {
    // ⚠ LA SEULE EXCEPTION QU'ON ATTRAPE, et elle vient d'une bibliothèque qui
    // n'offre pas d'autre voie. Un fichier de réglages abîmé ne doit pas
    // empêcher de jouer.
    // ⚠ DEUX CONSÉQUENCES, ET LA SECONDE NE SE DEVINE PAS : les valeurs par
    // défaut s'appliquent, ET `load_failed_` étant posé, plus rien ne s'écrira
    // de toute la partie — pour ne pas écraser un fichier que le joueur peut
    // encore réparer.
    ro::core::LogWarning(
        "réglages : {} illisible ({}) — les valeurs par défaut s'appliquent, et le fichier ne "
        "sera pas réécrit de la partie",
        ro::core::Utf8FromPath(path), failed.what());
    if (broken != nullptr) {
      *broken = true;
    }
    return YAML::Node(YAML::NodeType::Map);
  }
}

// La place qu'on réserve par ligne écrite. ⚠ UNE RÉSERVE, PAS UNE BORNE : une
// ligne plus longue s'écrit quand même, elle coûte juste la réallocation que
// cette valeur sert à éviter.
constexpr std::size_t kRoomPerLine = 48;

// UN INTERTITRE DE FICHIER : « # ── Le son ─────────── ». La largeur est celle
// du code du dépôt, pour que les deux se lisent dans la même fenêtre.
constexpr std::size_t kIntertitleWidth = 78;
constexpr std::string_view kIntertitleOpening = "# ── ";
constexpr std::string_view kIntertitleRule = "─";

// LE GROUPE OÙ VA CE QUE CE CLIENT NE CONNAÎT PAS. ⚠ NOMMÉ, et non laissé en
// queue sans titre : sans lui, un joueur qui trouve une clé inconnue au bas de
// son fichier croit à un reste oublié, et l'efface.
constexpr const char* kForeignSection = "Écrit par une autre version, et conservé tel quel";

[[nodiscard]] std::string Intertitle(std::string_view title) {
  std::string line(kIntertitleOpening);
  line += title;
  line += ' ';
  // ⚠ LA LARGEUR SE COMPTE EN CARACTÈRES, PAS EN OCTETS : le filet et les
  // accents sont en UTF-8, donc `size()` compterait trois fois le tiret cadratin
  // et rendrait des lignes deux fois trop courtes.
  std::size_t letters = 0;
  for (const char byte : line) {
    letters += (static_cast<unsigned char>(byte) & 0xC0U) != 0x80U ? 1 : 0;
  }
  while (letters < kIntertitleWidth) {
    line += kIntertitleRule;
    ++letters;
  }
  line += '\n';
  return line;
}

// UNE VALEUR, ÉCRITE COMME YAML L'ATTEND.
//
// ⚠ UN NOMBRE S'ÉCRIT AU PLUS COURT QUI SE RELISE À L'IDENTIQUE : c'est ce que
// `std::format` donne d'un flottant, là où six décimales fixes perdraient la
// dernière et rendraient l'aller-retour inexact.
[[nodiscard]] std::string AsYaml(bool value) { return value ? "true" : "false"; }
[[nodiscard]] std::string AsYaml(int value) { return std::format("{}", value); }
[[nodiscard]] std::string AsYaml(std::uint32_t value) { return std::format("{}", value); }
[[nodiscard]] std::string AsYaml(float value) { return std::format("{}", value); }

// 🔴 UNE CHAÎNE SE CITE TOUJOURS, et ce n'est pas de la prudence : sans
// guillemets, « oui », « 12 » ou « ~ » se relisent en booléen, en nombre ou en
// nul. Deux échappements suffisent dans une chaîne à guillemets doubles.
[[nodiscard]] std::string AsYaml(const std::string& value) {
  std::string quoted;
  quoted.reserve(value.size() + 2);
  quoted += '"';
  for (const char letter : value) {
    if (letter == '"' || letter == '\\') {
      quoted += '\\';
    }
    quoted += letter;
  }
  quoted += '"';
  return quoted;
}

// LA FORME CANONIQUE D'UN SCALAIRE : celle que rend `YAML::Dump`. Un profil
// relu du disque la porte ; la donner aussi à ce qu'on relève fait qu'une
// valeur inchangée se compare égale à elle-même.
[[nodiscard]] std::string Canonical(const std::string& text) {
  try {
    return YAML::Dump(YAML::Load(text));
  } catch (const YAML::Exception&) {
    return text;
  }
}

}  // namespace

SettingsFile::SettingsFile(std::filesystem::path path) : path_(std::move(path)) {}

void SettingsFile::Add(std::string key, Live live, Value fallback) {
  // ⚠ CE QU'ON DÉCLARE CESSE D'ÊTRE ÉTRANGER. Déclarer APRÈS la lecture n'est
  // pas l'ordre à suivre — le défaut vient de la valeur présente à la
  // déclaration —, mais rien ne l'empêche, et sans cette ligne la clé
  // sortirait DEUX FOIS : une par le relevé des étrangères, une par la table.
  // ⚠ SANS COÛT DANS L'ORDRE NORMAL : `foreign_` est alors vide.
  std::erase_if(foreign_, [&key](const auto& pair) { return pair.first == key; });
  entries_.push_back(Entry{.key = std::move(key),
                           .live = live,
                           .fallback = std::move(fallback),
                           .heading = std::move(pending_section_),
                           .machine = machine_depth_ > 0,
                           .page = page_});
  pending_section_.clear();
}

void SettingsFile::Section(std::string title) { pending_section_ = std::move(title); }

void SettingsFile::Retire(std::string key) {
  // ⚠ MÊME GARDE QUE `Add`, et pour la même raison : appelée après la lecture,
  // elle doit encore pouvoir défaire ce que le relevé a gardé.
  std::erase_if(foreign_, [&key](const auto& pair) { return pair.first == key; });
  retired_.push_back(std::move(key));
}

void SettingsFile::Declare(std::string key, bool* value) { Add(std::move(key), value, *value); }

void SettingsFile::Declare(std::string key, int* value) { Add(std::move(key), value, *value); }

void SettingsFile::Declare(std::string key, float* value) { Add(std::move(key), value, *value); }

void SettingsFile::Declare(std::string key, std::uint32_t* value) {
  Add(std::move(key), value, *value);
}

void SettingsFile::Declare(std::string key, std::string* value) {
  Add(std::move(key), value, *value);
}

void SettingsFile::Load() {
  std::error_code missing;
  // ⚠ UN FICHIER ABSENT EST DÉJÀ EN ATTENTE D'ÉCRITURE, et c'est délibéré :
  // sans cela, un joueur qui n'a touché à aucun réglage n'aurait jamais de
  // fichier à ouvrir, et ne saurait donc pas qu'il peut le corriger à la main.
  const bool first_time = !std::filesystem::exists(path_, missing);
  const YAML::Node whole = ReadWhole(path_, &load_failed_);

  // ── CE QUE CE CLIENT NE CONNAÎT PAS, RELEVÉ UNE FOIS ──────────────────────
  //
  // 🔴 À LA LECTURE ET NON À CHAQUE ÉCRITURE. L'écriture relisait le fichier
  // entier pour ne rien perdre — trente-cinq millisecondes de plus à chaque
  // réglage touché, sur le fil qui dessine. Ce qui est étranger ne change pas
  // pendant la partie : le relever au démarrage dit la même chose, une fois.
  //
  // ⚠ GARDÉ EN TEXTE, jamais réinterprété : on ne sait pas ce que c'est, donc on
  // ne sait pas non plus le corriger. `YAML::Dump` rend la forme qu'il avait.
  foreign_.clear();
  bool found_dead = false;
  for (const auto& pair : whole) {
    if (!pair.first.IsScalar()) {
      continue;
    }
    std::string key = pair.first.Scalar();
    const bool ours = std::any_of(entries_.begin(), entries_.end(),
                                  [&key](const Entry& entry) { return entry.key == key; });
    // ⚠ UNE CLÉ RETIRÉE N'EST PAS ÉTRANGÈRE, elle est MORTE : on la connaît
    // assez pour savoir qu'elle ne reviendra pas. Voir `Retire`.
    const bool dead = std::find(retired_.begin(), retired_.end(), key) != retired_.end();
    found_dead = found_dead || dead;
    if (!ours && !dead) {
      foreign_.emplace_back(std::move(key), YAML::Dump(pair.second));
    }
  }

  for (const Entry& entry : entries_) {
    const YAML::Node node = whole[entry.key];
    if (!node || !node.IsScalar()) {
      continue;  // absente : le défaut reste
    }
    // `as<T>(repli)` ne lance pas : une valeur du mauvais type laisse le défaut.
    std::visit(
        [&node](auto* live) {
          using Held = std::remove_pointer_t<decltype(live)>;
          *live = node.as<Held>(*live);
        },
        entry.live);
  }
  // ⚠ UNE CLÉ MORTE TROUVÉE MET LE FICHIER EN ATTENTE D'ÉCRITURE, comme un
  // fichier absent : sinon elle survivrait jusqu'au jour où le joueur touche un
  // réglage, c'est-à-dire indéfiniment pour qui n'ouvre jamais les options.
  if (first_time || found_dead) {
    Touch();
  }
}

void SettingsFile::RestoreEntry(const Entry& entry) {
  std::visit(
      [](auto* live, const auto& fallback) {
        using Held = std::remove_pointer_t<decltype(live)>;
        if constexpr (std::is_same_v<Held, std::decay_t<decltype(fallback)>>) {
          *live = fallback;
        }
      },
      entry.live, entry.fallback);
}

void SettingsFile::RestoreDefaults() {
  for (const Entry& entry : entries_) {
    RestoreEntry(entry);
  }
}

void SettingsFile::RestoreDefaults(int page) {
  for (const Entry& entry : entries_) {
    if (entry.page == page) {
      RestoreEntry(entry);
    }
  }
}

void SettingsFile::RestoreDefault(std::string_view key) {
  for (const Entry& entry : entries_) {
    if (entry.key == key) {
      RestoreEntry(entry);
    }
  }
}

int SettingsFile::PageOf(std::string_view key) const {
  const auto found = std::find_if(entries_.begin(), entries_.end(),
                                  [key](const Entry& entry) { return entry.key == key; });
  return found == entries_.end() ? kNoSettingsPage : found->page;
}

bool SettingsFile::HasPage(int page) const {
  return std::any_of(entries_.begin(), entries_.end(),
                     [page](const Entry& entry) { return entry.page == page; });
}

bool SettingsFile::PageHoldsMachine(int page) const {
  return std::any_of(entries_.begin(), entries_.end(),
                     [page](const Entry& entry) { return entry.page == page && entry.machine; });
}

SettingsValues SettingsFile::Values() const {
  SettingsValues values;
  for (const Entry& entry : entries_) {
    values.emplace(entry.key,
                   Canonical(std::visit([](auto* live) { return AsYaml(*live); }, entry.live)));
  }
  return values;
}

SettingsValues SettingsFile::SharedValues() const {
  SettingsValues values;
  for (const Entry& entry : entries_) {
    if (!entry.machine) {
      values.emplace(entry.key,
                     Canonical(std::visit([](auto* live) { return AsYaml(*live); }, entry.live)));
    }
  }
  return values;
}

SettingsValues SettingsFile::SharedDefaults() const {
  SettingsValues values;
  for (const Entry& entry : entries_) {
    if (!entry.machine) {
      values.emplace(entry.key,
                     Canonical(std::visit([](const auto& fallback) { return AsYaml(fallback); },
                                          entry.fallback)));
    }
  }
  return values;
}

void SettingsFile::AssignShared(const SettingsValues& values) {
  for (const Entry& entry : entries_) {
    if (entry.machine) {
      continue;
    }
    // ⚠ D'ABORD LE DÉFAUT : une clé absente ou illisible le garde.
    std::visit(
        [](auto* live, const auto& fallback) {
          using Held = std::remove_pointer_t<decltype(live)>;
          if constexpr (std::is_same_v<Held, std::decay_t<decltype(fallback)>>) {
            *live = fallback;
          }
        },
        entry.live, entry.fallback);
    const auto found = values.find(entry.key);
    if (found == values.end()) {
      continue;
    }
    YAML::Node node;
    try {
      node = YAML::Load(found->second);
    } catch (const YAML::Exception&) {
      continue;
    }
    if (!node.IsScalar()) {
      continue;
    }
    // `as<T>(repli)` ne lance pas : une valeur du mauvais type laisse le défaut.
    std::visit(
        [&node](auto* live) {
          using Held = std::remove_pointer_t<decltype(live)>;
          *live = node.as<Held>(*live);
        },
        entry.live);
  }
}

void SettingsFile::Touch() noexcept {
  dirty_ = true;
  restart_ = true;
  ++revision_;
}

void SettingsFile::Tick(std::uint32_t now_ms) {
  if (!dirty_) {
    return;
  }
  if (restart_) {
    restart_ = false;
    dirty_since_ms_ = now_ms;
    return;
  }
  if (now_ms - dirty_since_ms_ < kSettingsFlushDelayMs) {
    return;
  }
  Flush();
}

void SettingsFile::Flush() {
  if (!dirty_) {
    return;
  }
  // ⚠ RETENU MÊME SI L'ÉCRITURE ÉCHOUE. Le contraire ferait réessayer soixante
  // fois par seconde sur un dossier en lecture seule, et noierait le journal
  // sous le même avertissement.
  dirty_ = false;
  restart_ = false;
  if (load_failed_) {
    ro::core::LogWarning("réglages : écriture refusée, la dernière lecture de {} avait échoué",
                         ro::core::Utf8FromPath(path_));
    return;
  }
  Write();
}

void SettingsFile::Write() {
  // 🔴 ÉCRIT À LA MAIN, ET C'EST UNE MESURE QUI L'A DÉCIDÉ. Ce fichier est un
  // mapping PLAT — une ligne par clé —, et le construire dans un `YAML::Node`
  // coûtait 314 ms pour huit cents clés : `node[clé] = valeur` cherche la clé
  // par un parcours LINÉAIRE de ce qui est déjà là, donc le remplissage est
  // quadratique. La même sortie écrite directement coûte 1 ms.
  //
  // Le joueur le voyait comme un FIGEMENT du jeu à chaque réglage touché dans un
  // panneau : l'écriture se fait sur le fil qui dessine, et une demi-seconde y
  // est trente images perdues. Voir `tests/app/settings_file_bench.cpp`, qui
  // reprend la mesure.
  //
  // ⚠ CE QU'ON NE CONNAÎT PAS RESTE, comme avant : les clés étrangères relevées
  // à la lecture sont réécrites telles quelles. Un réglage posé par une version
  // plus récente survit donc à une partie jouée avec celle-ci.
  std::string text(kFileHeader);
  // Une ligne fait rarement plus de quarante caractères ; réserver évite une
  // dizaine de réallocations sur un fichier de huit cents clés.
  text.reserve(kFileHeader.size() + (entries_.size() + foreign_.size()) * kRoomPerLine);
  for (const Entry& entry : entries_) {
    if (!entry.heading.empty()) {
      text += '\n';
      text += Intertitle(entry.heading);
    }
    text += entry.key;
    text += ": ";
    std::visit([&text](auto* live) { text += AsYaml(*live); }, entry.live);
    text += '\n';
  }
  if (!foreign_.empty()) {
    text += '\n';
    text += Intertitle(kForeignSection);
  }
  for (const auto& [key, value] : foreign_) {
    text += key;
    text += ": ";
    text += value;
    text += '\n';
  }
  if (const ro::core::Result<void> written = ro::core::WriteFileAtomically(path_, text); !written) {
    // ⚠ UN LIBELLÉ À ELLE : la fenêtre de journal ne garde que le TEXTE, et
    // celui-ci était mot pour mot celui de l'échec de LECTURE.
    ro::core::LogWarning(
        "réglages : écriture impossible ({}) — ce qui vient d'être réglé sera réécrit au "
        "prochain changement",
        ro::core::Describe(written.error()));
  }
}

}  // namespace moonclient
