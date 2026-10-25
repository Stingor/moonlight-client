#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/survivors_meta.h"
#include "ro/sim/survivors_records.h"

#include "survivors/survivors_pending.h"

namespace moonclient {

// ── LE CARNET DU MODE SURVIVORS : `savedata\survivors.yml` ──────────────────
//
// ⚠ UN SEUL POUR TOUTE L'INSTALLATION, comme `minigames.yml` : c'est le joueur
// devant l'écran qui joue, pas un compte ni un personnage. Et un seul ÉCRIVAIN :
// tout ce qui touche au fichier passe par `SurvivorsSaveFile`.
//
// Sections, toutes facultatives, aux clés anglaises stables :
// - `options` : les réglages du mode ;
// - `meta` : le solde de Zeny, leur cumul, les rangs et les dépenses de
//   l'Église et de la Kafra, la dernière Cursed Water choisie ;
// - `album` : les cartes trouvées, les exploits atteints, les compteurs ;
// - `records` : les dix meilleures parties par (classe, arène, tableau, et
//   semaine pour le défi) ;
// - `pending_results` : les parties classées dont le serveur n'a pas encore
//   rendu le verdict, à lui renvoyer.
//
// Les cartes, classes, arènes, skills et Bénédictions se rangent par leur CLÉ
// du catalogue, jamais par un rang. ⚠ UNE CLÉ OU UNE SECTION INCONNUE (celle
// d'un client plus récent) SE GARDE ET SE RÉÉCRIT : revenir à une version
// antérieure ne fait rien perdre. Il en va de même d'une section connue dont la
// FORME a changé (une table là où ce client attend une liste…), tant que ce
// client n'y écrit rien, et de la valeur d'un rang, d'une carte ou d'un exploit
// qu'il ne sait pas lire. Un champ inconnu à l'intérieur d'une section connue
// ne se garde pas.
//
// Lecture tolérante, champ par champ : un champ absent, d'un mauvais type ou
// hors bornes garde son défaut ; un rang au-delà du maximum se ramène au
// maximum ; un solde négatif se lit zéro.
inline constexpr std::string_view kSurvivorsSaveFileName = "survivors.yml";

// Les réglages du mode : « Effets réduits », « Afficher les mesures »,
// « Ombres du soleil de la horde » et le dernier premier bolt choisi. Ils
// restent hors des réglages du client.
struct SurvivorsOptions {
  bool reduced_effects = false;
  bool show_measures = false;
  bool horde_sun_shadows = false;
  std::uint16_t first_skill_id = 0;  // 0 : aucun choix encore, Fire Bolt se présélectionne
  [[nodiscard]] bool operator==(const SurvivorsOptions&) const = default;
};

// « AFFICHER LES MESURES » EST UN OUTIL DE DÉVELOPPEMENT : la case ne paraît,
// et les mesures ne se montrent, qu'au client de développement
// (`ro::core::kDevTools`). Ailleurs, une valeur déjà rangée dans le fichier ne
// montre rien, et elle y reste telle quelle.
[[nodiscard]] constexpr bool MeasuresShown(const SurvivorsOptions& options,
                                           bool dev_tools) noexcept {
  return dev_tools && options.show_measures;
}

// Ce que ce client ne lit pas — une section, une clé de rang inconnue, une
// carte ou un exploit d'une autre forme —, recopié tel quel.
struct SurvivorsForeignEntry {
  std::string key;
  std::string yaml;  // le texte YAML de sa valeur
  [[nodiscard]] bool operator==(const SurvivorsForeignEntry&) const = default;
};

struct SurvivorsSaveMeta {
  std::uint64_t zeny = 0;         // le solde
  std::uint64_t zeny_earned = 0;  // le cumul de tout ce que les parties ont rapporté
  ro::sim::SurvivorsMetaRanks ranks;
  std::vector<SurvivorsForeignEntry> foreign_church_ranks;
  std::vector<SurvivorsForeignEntry> foreign_kafra_ranks;
  std::uint8_t cursed_water = 0;  // le dernier rang choisi, 0 à kMostCursedWater
  [[nodiscard]] bool operator==(const SurvivorsSaveMeta&) const = default;
};

struct SurvivorsAlbumCard {
  std::string key;  // la clé de la carte (`hornet_card`…)
  std::uint32_t found = 0;
  std::int64_t first_found_day = 0;  // jours depuis le 1970-01-01 UTC
  [[nodiscard]] bool operator==(const SurvivorsAlbumCard&) const = default;
};

// Les compteurs au cumul, que les exploits lisent.
struct SurvivorsAlbumCounters {
  std::uint64_t chills = 0;      // monstres ralentis par un glaçage
  std::uint64_t wind_kills = 0;  // monstres Wind vaincus
  std::uint16_t best_level = 0;
  std::uint32_t best_survived_ms = 0;
  std::uint32_t runs = 0;
  [[nodiscard]] bool operator==(const SurvivorsAlbumCounters&) const = default;
};

struct SurvivorsAlbum {
  std::vector<SurvivorsAlbumCard> cards;  // dans l'ordre du fichier
  std::vector<std::string> feats;         // les clés des exploits atteints
  // Une carte ou un exploit d'une autre forme : réécrit après les siens, sauf
  // quand ce client a rangé depuis une entrée de même clé.
  std::vector<SurvivorsForeignEntry> foreign_cards;
  std::vector<SurvivorsForeignEntry> foreign_feats;
  SurvivorsAlbumCounters counters;
  [[nodiscard]] bool operator==(const SurvivorsAlbum&) const = default;
};

// Les tableaux d'une arène. La partie classée a aussi le sien au classement
// mondial, sous la même clé ; le défi de même.
inline constexpr std::string_view kFreeBoardKey = "free";
inline constexpr std::string_view kChallengeBoardKey = "challenge";
inline constexpr std::string_view kRankedBoardKey = "ranked";

// LE DÉFI A UN TABLEAU PAR SEMAINE (Q11) : celui de la semaine en cours garde
// ses dix meilleures parties, celui d'une semaine passée sa meilleure seule.
struct SurvivorsBoard {
  std::string class_key;  // `mage`
  std::string arena_key;  // `prt_fild08`
  std::string board_key;  // kFreeBoardKey, kChallengeBoardKey ou kRankedBoardKey
  ro::sim::IsoWeek week;  // la semaine du défi ; {0, 0} en partie libre
  std::vector<ro::sim::SurvivorsRecord> runs;  // rangés, kRecordsPerBoard au plus
  [[nodiscard]] bool operator==(const SurvivorsBoard&) const = default;
};

struct SurvivorsSave {
  SurvivorsOptions options;
  SurvivorsSaveMeta meta;
  SurvivorsAlbum album;
  std::vector<SurvivorsBoard> records;
  // Dans l'ordre du fichier, puis des ajouts.
  std::vector<SurvivorsPendingResult> pending_results;
  // Le texte YAML d'une entrée que ce client ne sait pas lire, ou qui ne peut
  // pas partir : réécrite après les siennes, jamais envoyée.
  std::vector<std::string> foreign_pending;
  // Les sections inconnues, et les sections connues d'une autre forme : une de
  // ces dernières se réécrit à la place de la sienne tant que celle-ci est
  // restée à son défaut.
  std::vector<SurvivorsForeignEntry> foreign_sections;
  [[nodiscard]] bool operator==(const SurvivorsSave&) const = default;
};

[[nodiscard]] std::filesystem::path SurvivorsSavePath(const std::filesystem::path& client_root);

// LE TEXTE DU FICHIER, dans les deux sens, sans rien lire ni écrire. Un texte
// vide est un carnet vierge ; kCorruptData quand le YAML le refuse ou que sa
// racine n'est pas une table : le fichier est ABÎMÉ.
[[nodiscard]] ro::core::Result<SurvivorsSave> ReadSurvivorsSave(std::string_view text);
[[nodiscard]] std::string WriteSurvivorsSave(const SurvivorsSave& save);

// UNE MUTATION : elle s'applique au carnet RELU, et rend vrai quand elle a
// changé quelque chose à écrire. Un achat s'y juge donc sur le solde du
// fichier, pas sur celui de l'ouverture.
using SurvivorsSaveDelta = std::function<bool(SurvivorsSave&)>;

// Le carnet d'un texte, un delta appliqué, et le texte à écrire : rien quand
// le delta n'a rien changé.
struct SurvivorsSaveApplied {
  SurvivorsSave save;
  std::optional<std::string> text;
};

// Le cœur de `SurvivorsSaveFile::Mutate` sur un fichier sain ; kCorruptData
// quand le texte est abîmé.
[[nodiscard]] ro::core::Result<SurvivorsSaveApplied> ApplySurvivorsSaveDelta(
    std::string_view text, const SurvivorsSaveDelta& delta);

// LES DEUX MUTATIONS DES RÉSULTATS EN ATTENTE, à passer à `Mutate` : chacune
// s'applique au carnet relu, et ne touche que son entrée. Ajouter remplace
// l'entrée de même (serveur, personnage, jeton) et ne change rien pour une
// entrée qui ne peut pas partir ; retirer ne touche jamais une entrée
// étrangère.
[[nodiscard]] SurvivorsSaveDelta AddPendingResult(SurvivorsPendingResult entry);
[[nodiscard]] SurvivorsSaveDelta RemovePendingResult(std::string server_key,
                                                     std::uint32_t character_id,
                                                     std::uint32_t ticket);

// Les instants du carnet, `AAAA-MM-JJTHH:MM:SSZ` en UTC, dans les deux sens ;
// rien quand le texte n'en est pas un.
[[nodiscard]] std::string SurvivorsInstantText(std::int64_t utc_s);
[[nodiscard]] std::optional<std::int64_t> SurvivorsInstantOfText(std::string_view text);

// Le nom de la copie d'un fichier abîmé : `survivors.yml.damaged-AAAAMMJJ-HHMMSS`,
// à l'heure UTC.
[[nodiscard]] std::string DamagedCopyName(std::chrono::system_clock::time_point now);

// Ce que le fichier a donné.
struct SurvivorsSaveRead {
  SurvivorsSave save;
  // ⚠ LE FICHIER EXISTE SANS SE LAISSER OUVRIR (tenu par un autre programme) :
  // `save` est vierge, et rien n'a été écrit.
  bool file_unread = false;
  // LE FICHIER ÉTAIT ABÎMÉ : copié là, puis remplacé par un carnet vierge. Le
  // joueur l'apprend une fois.
  std::optional<std::filesystem::path> damaged_copy;
};

// ÉCRIRE ET LIRE LE CARNET : relire, appliquer, écrire. Rien ne se garde entre
// deux appels : deux clients ouverts en même temps ne s'effacent pas leurs
// Zeny, chacun relisant ce que l'autre a écrit avant d'y ajouter son delta.
// ⚠ À CONDITION QUE LEURS MUTATIONS NE SE CHEVAUCHENT PAS : aucun verrou ne
// tient le fichier entre la relecture et l'écriture, qui ne durent que le
// temps d'un petit fichier, et le temporaire de `WriteFileAtomically` porte un
// nom fixe. Deux mutations dans la même fenêtre peuvent perdre la première.
//
// Un fichier ABÎMÉ (le YAML le refuse, ou sa racine n'est pas une table) est
// copié à côté, puis remplacé par un carnet vierge ; si la copie échoue, rien
// ne s'écrit. Un fichier qui NE S'OUVRE PAS ne s'écrase jamais.
class SurvivorsSaveFile {
 public:
  using Clock = std::function<std::chrono::system_clock::time_point()>;
  // `now` date la copie d'un fichier abîmé ; vide, l'horloge du système.
  explicit SurvivorsSaveFile(Clock now = {});

  // Vierge, sans un mot, quand le fichier n'existe pas : c'est la première
  // partie.
  [[nodiscard]] SurvivorsSaveRead Load(const std::filesystem::path& client_root) const;
  // Le carnet après le delta. kIoFailure quand le fichier ne s'ouvre pas ou
  // qu'un abîmé ne se copie pas : rien n'a été écrit.
  [[nodiscard]] ro::core::Result<SurvivorsSaveRead> Mutate(const std::filesystem::path& client_root,
                                                           const SurvivorsSaveDelta& delta) const;

 private:
  // Copie un fichier abîmé à côté de lui ; rien quand la copie échoue.
  [[nodiscard]] std::optional<std::filesystem::path> CopyDamaged(
      const std::filesystem::path& path) const;

  Clock now_;
};

// LES RÉGLAGES SEULS, pour le mode : lus du carnet, et écrits comme un delta
// qui ne pose que les champs où `after` diffère de `before`. Un réglage que le
// mode n'a pas touché garde donc la valeur du fichier relu, même quand le mode
// l'avait lu autrement (fichier tenu à l'ouverture, autre client). L'écriture
// rend le carnet relu, et la copie d'un fichier abîmé qu'elle a remplacé.
[[nodiscard]] SurvivorsOptions LoadSurvivorsOptions(const std::filesystem::path& client_root);
[[nodiscard]] ro::core::Result<SurvivorsSaveRead> SaveSurvivorsOptions(
    const std::filesystem::path& client_root, const SurvivorsOptions& before,
    const SurvivorsOptions& after);

}  // namespace moonclient
