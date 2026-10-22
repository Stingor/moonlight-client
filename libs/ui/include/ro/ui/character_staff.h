#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

// LE VOLET DU STAFF : le troisième volet du mannequin.
//
// 🔴 IL PILOTE CE QUE LE VOLET DES NOMBRES NE FAIT QU'AFFICHER. Monter une
// caractéristique, changer de classe, régler la vitesse de marche : chaque
// commande y est posée EN FACE de la valeur qu'elle change, et c'est ce qui la
// rend lisible sans libellé.
//
// 🔴 TOUT Y PASSE PAR DES COMMANDES DU SERVEUR. Aucune n'a de paquet : le
// serveur ne les expose que par la parole, exactement comme l'ouverture d'un
// coffre. La fenêtre émet donc une INTENTION — `app` sait que « remettre les
// caractéristiques à zéro » s'écrit « @resetstat ».
//
// ⚠ ET RIEN N'Y EST PROTÉGÉ PAR CE FICHIER. Le droit qui montre le volet est un
// gate d'AFFICHAGE : le serveur revalide chaque commande selon le groupe du
// compte, et son refus arrive dans la conversation comme d'habitude.
namespace ro::ui {

// ── LA VITESSE DE MARCHE ────────────────────────────────────────────────────
//
// ⚠ PLUS BAS = PLUS RAPIDE : c'est un DÉLAI entre deux cases, pas une vitesse.
// Cent cinquante est la marche normale, vingt le plancher du serveur.
inline constexpr int kWalkSpeedFastest = 20;
inline constexpr int kWalkSpeedNormal = 150;
inline constexpr int kWalkSpeedSlowest = 500;

// ── LES BORNES DES CHAMPS ───────────────────────────────────────────────────
//
// ⚠ TROIS PLAFONDS ET NON UN SEUL : une caractéristique plafonne à quelques
// centaines là où le zeny se compte en millions. Un plafond unique aurait
// silencieusement rogné les montants de l'un, ou laissé l'autre écrire n'importe
// quoi.
inline constexpr int kStatStepMost = 9999;
inline constexpr int kLevelStepMost = 999;
inline constexpr int kZenyStepMost = 1000000000;
inline constexpr int kStatPointsMost = 30000;
inline constexpr int kRefineStepMost = 10;

// ── LES EMPLACEMENTS QUE « @refine » ACCEPTE ────────────────────────────────
//
// Un masque `EQP_*` du serveur (`common/mmo.hpp`), et zéro pour « toutes les
// pièces portées » — la valeur que la commande traite comme « pas de filtre ».
//
// 🔴 LES LIBELLÉS RESTENT LES TERMES DU JEU, comme les abréviations des cases du
// mannequin : le staff doit pouvoir apparier d'un coup d'œil la ligne du menu et
// la case. Seule la première entrée est une phrase, donc la seule à traduire.
struct RefinePlace {
  std::uint32_t mask;
  const char* label;
};
inline constexpr std::array<RefinePlace, 11> kRefinePlaces = {{
    {0, "Toutes les pièces"},
    {2, "Weapon"},
    {32, "Shield"},
    {16, "Armor"},
    {4, "Garment"},
    {64, "Shoes"},
    {256, "Head top"},
    {512, "Head mid"},
    {1, "Head bot"},
    {8, "Acc. R"},
    {128, "Acc. L"},
}};

// ── L'ÉTAT VISUEL QUE « @option » POSE ──────────────────────────────────────
//
// Les drapeaux `OPTION_*` du TROISIÈME paramètre. Les deux premiers portent les
// vrais statuts — pétrifié, gelé, maudit — et la modale les met à zéro : le
// staff qui pose un état visuel veut être propre, pas gelé.
//
// ⚠ LE NIVEAU DE CART N'EST PAS ICI : ses cinq valeurs s'excluent, c'est un menu
// et non une case. Voir `kCartOptionBits`.
struct OptionFlag {
  std::uint32_t bit;
  const char* label;
};
inline constexpr std::array<OptionFlag, 10> kOptionFlags = {{
    {32, "Riding"},
    {16, "Falcon"},
    {1, "Sight"},
    {2, "Hiding"},
    {4, "Cloaking"},
    {64, "Invisible"},
    {2048, "Orc Head"},
    {4096, "Wedding"},
    {8192, "Ruwach"},
    {16384, "Chasewalk"},
}};

// Index = niveau de cart, zéro pour aucun ; valeur = bit `OPTION_CART*`.
inline constexpr std::array<std::uint32_t, 6> kCartOptionBits = {0, 8, 128, 256, 512, 1024};
inline constexpr int kCartLevelMost = 5;

// LA FAMILLE D'UNE CLASSE : l'en-tête sous lequel le menu la range.
//
// 🔴 UNE ÉNUMÉRATION ET NON UNE CHAÎNE, et c'est `Tr` qui l'impose : il veut
// voir son littéral à l'appel. Une chaîne prise dans la table des classes ne
// serait dans aucun catalogue — elle resterait française dans toutes les
// langues, et rien ne le signalerait. Voir `StaffJobFamilyLabel`.
enum class StaffJobFamily : std::uint8_t {
  kNovice,        // Novice / 1re classe
  kSecond,        // 2e classe
  kTransFirst,    // Trans. 1re classe
  kTransSecond,   // Trans. 2e classe
  kThird,         // 3e classe
  kThirdTrans,    // 3e classe (trans.)
  kFourth,        // 4e classe
  kExpanded,      // Classes étendues
  kBabyFirst,     // Baby / 1re classe
  kBabySecond,    // Baby 2e classe
  kBabyThird,     // Baby 3e classe
  kBabyExpanded,  // Baby étendues
};

// Le libellé d'une famille, traduit à la LECTURE.
[[nodiscard]] const char* StaffJobFamilyLabel(StaffJobFamily family);

// Une classe que « @job » accepte, telle que la fenêtre la montre :
// « 4011  White Smith (Whitesmith) », comme la complétion de la discussion.
struct StaffJobView {
  std::uint32_t id = 0;
  // ⚠ TERMINÉ PAR UN NUL : l'aperçu du menu le passe tel quel à ImGui.
  std::string_view label;
  // Le nom que le serveur lui donne quand il diffère du libellé ; vide sinon.
  std::string_view detail;
  StaffJobFamily family = StaffJobFamily::kNovice;
};

// ── CE QUE LE VOLET VIENT DE DEMANDER ───────────────────────────────────────
enum class StaffOrder : std::uint8_t {
  kNone,
  // Ajuste une caractéristique de `amount`, au rang de `stat`.
  kAdjustStat,
  kResetStats,
  kMaxStats,
  // Change de classe pour `job_id`.
  kChangeJob,
  // Pose la vitesse de marche à `amount`.
  kSetSpeed,
  // Rend la vitesse par défaut. ⚠ CE N'EST PAS `kSetSpeed` À CENT CINQUANTE : le
  // serveur pose un verrou dès que la vitesse s'écarte du défaut, et seule une
  // valeur NÉGATIVE le relâche. Écrire 150 rendrait la bonne vitesse et
  // garderait le verrou.
  kNormalSpeed,
  kAdjustBaseLevel,
  kAdjustJobLevel,
  kAdjustZeny,
  // Donne `amount` points de caractéristique.
  kGiveStatPoints,
  // Ajuste le raffinage de `amount` sur les emplacements de `mask`.
  kRefine,
  kHeal,
  kRepairAll,
  // Bascule l'invisibilité. ⚠ LA CASE MONTRE L'OPTION QUE LE SERVEUR ANNONCE, et
  // non un souvenir local : il inverse ce qu'il a, et c'est lui qui la garde
  // d'une connexion à l'autre.
  kToggleHide,
  // Basculent `@killer` (attaquer les joueurs hors PvP), `@killable` (être
  // attaquable hors PvP), `@playertest` (plafonds GM ignorés) et `@nodelay` (aucune
  // limite de cadence). ⚠ MÊME RÈGLE que l'invisibilité : les cases montrent
  // ce que le serveur annonce, et c'est lui qui les garde.
  kToggleKiller,
  kToggleKillable,
  kTogglePlayerTest,
  kToggleNoDelay,
  // Veut être ignoré des monstres (`amount` non nul) ou ne plus l'être. ⚠ UN
  // SOUHAIT, PAS UNE COMMANDE : `app` le retient et bascule le serveur jusqu'à
  // ce qu'il y corresponde — à la connexion suivante aussi, puisque le serveur,
  // lui, l'oublie.
  kSetBattleIgnore,
  // Remplace l'état visuel par `mask`.
  kSetOption,
  kWipeInventory,
};

struct StaffAction {
  StaffOrder order = StaffOrder::kNone;
  // LE MONTANT, ET IL EST SIGNÉ. ⚠ LE SIGNE VIENT DU BOUTON, JAMAIS DE LA
  // SAISIE : un « -5 » tapé dans le champ puis un clic sur « + » enverrait
  // « @str +-5 », que le serveur lirait comme il pourrait.
  int amount = 0;
  // Le rang de la caractéristique visée, de zéro à cinq, pour `kAdjustStat`.
  int stat = 0;
  // Le masque d'emplacement de `kRefine`, ou l'état visuel de `kSetOption`.
  std::uint32_t mask = 0;
  std::uint32_t job_id = 0;
};

// ── CE QUE LE VOLET SAIT DU MONDE, posé par `app` à chaque image ────────────
//
// ⚠ IL NE L'ÉCRIT JAMAIS, et c'est pour cela qu'il vit hors de son état : `ui`
// ne possède aucun état de jeu.
struct StaffPaneView {
  // ⚠ DES VUES, et elles ne valent que pour l'image.
  std::span<const StaffJobView> jobs;
  std::uint32_t own_job = 0;
  // L'état visuel connu du client, pour amorcer la modale.
  bool riding = false;
  bool falcon = false;
  int cart_level = 0;
  // L'invisibilité, lue dans l'option que le serveur annonce.
  bool hidden = false;
  // `@killer`, `@killable`, `@playertest` et `@nodelay`, lus dans les réglages
  // que le serveur annonce.
  bool killer = false;
  bool killable = false;
  bool player_test = false;
  bool no_delay = false;
  // « Ignoré des monstres » tel que le joueur le VEUT : le serveur n'annonce pas
  // le sien. Voir `StaffOrder::kSetBattleIgnore`.
  bool battle_ignored = false;
  // La vitesse que le serveur a annoncée.
  int server_speed = kWalkSpeedNormal;
};

// ── CE QUE LE VOLET GARDE D'UNE IMAGE À L'AUTRE ─────────────────────────────
struct StaffPaneState {
  // Le pas de chaque ajusteur. ⚠ CE SONT DES AJUSTEMENTS, PAS DES CIBLES : les
  // commandes du serveur prennent toutes un delta signé.
  std::array<int, 6> stat_step = {1, 1, 1, 1, 1, 1};
  int base_level_step = 10;
  int job_level_step = 10;
  int zeny_step = 10000;
  int stat_points = 100;
  int refine_step = 1;
  int refine_place = 0;

  // LA VITESSE MONTRÉE PAR LE CURSEUR.
  //
  // 🔴 LE SERVEUR EN EST LA SOURCE, ET ON LE SUIT PAR FRONT. Après notre propre
  // envoi, il renvoie la valeur qu'on affiche déjà : s'aligner à chaque image
  // ferait sauter le curseur en arrière le temps de l'aller-retour.
  int speed = kWalkSpeedNormal;
  int speed_sent = kWalkSpeedNormal;
  int speed_seen = kWalkSpeedNormal;

  // Le filtre du menu des classes. ⚠ REMIS À ZÉRO À CHAQUE OUVERTURE : gardé
  // d'une fois sur l'autre, il rouvrirait la liste presque vide sans que rien ne
  // dise pourquoi.
  std::array<char, 32> job_filter = {};

  // L'ÉTAT VISUEL EN COURS D'ÉDITION, amorcé sur ce que le client sait à
  // l'ouverture de la modale.
  std::array<bool, kOptionFlags.size()> option_flags = {};
  int option_cart = 0;

  // 🔴 VIDER L'INVENTAIRE SE FAIT EN DEUX TEMPS. La commande SUPPRIME tout et le
  // serveur ne demande rien : le clic est irréversible, donc la confirmation est
  // ici. Même règle que la dissolution d'une guilde.
  bool wipe_asked = false;
};

// ── OÙ LES RANGÉES DU VOLET DES NOMBRES SE POSENT ───────────────────────────
//
// 🔴 LE VOLET DU STAFF S'ALIGNE DESSUS, ET IL NE PEUT PAS LES DEVINER. Ses
// commandes se posent EN FACE de la valeur qu'elles changent, et une rangée de
// nombres n'a pas la hauteur d'une rangée de boutons. Relevé plutôt que calculé,
// parce que la police et l'échelle décident des deux.
//
// ⚠ RELATIF AU HAUT DU CONTENU, et non à l'écran : les deux volets sont des
// enfants distincts, et celui des nombres peut être défilé.
struct CharacterStatRows {
  std::array<float, 6> attribute = {};
  float status_point = 0.0F;
  // La première dérivée — l'ATK —, sur laquelle le menu des classes s'aligne.
  float first_derived = 0.0F;
  // ⚠ FAUX TANT QUE LE VOLET N'A PAS ÉTÉ DESSINÉ : sans relevé, le volet du
  // staff suit son flux naturel plutôt que de s'aligner sur des zéros.
  bool measured = false;
};

}  // namespace ro::ui
