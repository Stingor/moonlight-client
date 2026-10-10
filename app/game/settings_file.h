#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "game/save_folder.h"

namespace moonclient {

// LES RÉGLAGES DU JOUEUR, SUR LE DISQUE.
//
// ⚠ UN RÉGLAGE SE DÉCRIT UNE FOIS : sa clé, et l'adresse de sa valeur. La
// lecture et l'écriture parcourent la MÊME table, donc elles ne peuvent pas
// diverger. C'est la panne que ce dispositif existe pour rendre impossible :
// deux miroirs tenus à la main finissent par différer d'une clé, et rien ne le
// dit — ni le compilateur, ni un essai, ni le jeu, qui se contente de perdre le
// réglage en silence.
//
// ⚠ LA VALEUR AU MOMENT DE LA DÉCLARATION EST LE DÉFAUT. On déclare avant de
// lire, donc le champ porte encore l'initialisateur écrit dans son propre
// en-tête : ce dernier reste la SEULE source du défaut. Un défaut recopié ici
// serait un second endroit à corriger, et le jour où les deux divergent, c'est
// la moitié des joueurs — ceux dont le fichier n'a pas la clé — qui obtient
// l'autre valeur.
//
// ⚠ LES CLÉS INCONNUES SONT CONSERVÉES. Le fichier est relu avant d'être
// réécrit, et l'on n'y remplace que ce qu'on a déclaré : un fichier écrit par
// une version plus récente ne perd pas ses réglages parce qu'une plus ancienne
// l'a ouvert une fois.
//
// ⚠ ET APRÈS UNE LECTURE QUI A ÉCHOUÉ, ON N'ÉCRIT PLUS. L'état en mémoire ne
// représente alors qu'une partie du fichier, le reste étant aux défauts :
// réécrire depuis là rendrait la perte définitive. On refuse, et on le dit.
// LES VALEURS QU'UN PROFIL DE RÉGLAGES EMPORTE : par clé, le scalaire YAML tel
// que ce fichier l'écrit.
using SettingsValues = std::map<std::string, std::string>;

// Un réglage qu'aucune page du panneau ne montre : voir `SettingsFile::PageScope`.
inline constexpr int kNoSettingsPage = -1;

class SettingsFile {
 public:
  explicit SettingsFile(std::filesystem::path path);

  // LES RÉGLAGES DE LA MACHINE, déclarés tant que cette portée vit : écran,
  // son, langue, échelle. Ils restent au fichier et ne suivent aucun profil —
  // charger un profil ne doit ni faire clignoter l'écran ni changer le son.
  class MachineScope {
   public:
    explicit MachineScope(SettingsFile& file) : file_(file) { ++file_.machine_depth_; }
    MachineScope(const MachineScope&) = delete;
    MachineScope& operator=(const MachineScope&) = delete;
    ~MachineScope() { --file_.machine_depth_; }

   private:
    SettingsFile& file_;
  };

  // LA PAGE DU PANNEAU QUI MONTRE les réglages déclarés tant que cette portée
  // vit : c'est elle que « Rétablir cette page » remet à l'origine. Un nombre
  // et non une section nommée : ce fichier ne connaît pas le panneau.
  //
  // ⚠ UN RÉGLAGE HORS DE TOUTE PAGE ne revient à l'origine qu'avec tous les
  // autres : c'est le cas de ce qu'aucune ligne ne montre — la place d'une
  // fenêtre, le compte retenu.
  class PageScope {
   public:
    PageScope(SettingsFile& file, int page) : file_(file), outer_(file.page_) {
      file_.page_ = page;
    }
    PageScope(const PageScope&) = delete;
    PageScope& operator=(const PageScope&) = delete;
    ~PageScope() { file_.page_ = outer_; }

   private:
    SettingsFile& file_;
    int outer_;
  };

  // La même chose pour TOUTES les déclarations qui suivent, comme `Section` :
  // pour une longue suite de blocs qui changent de page sans s'imbriquer.
  void Page(int page) noexcept { page_ = page; }

  // OUVRE UN GROUPE DANS LE FICHIER : un intertitre écrit avant les réglages qui
  // suivent.
  //
  // 🔴 CE FICHIER SE LIT À LA MAIN, c'est ce qui a fait choisir YAML — et six
  // cents clés d'affilée ne se lisent pas. Le nom d'une clé dit déjà sa famille
  // (`entity_monsters_…`), mais il faut la chercher ; un intertitre la donne du
  // premier coup d'œil.
  //
  // ⚠ UN COMMENTAIRE, DONC AUCUNE STRUCTURE NOUVELLE. Imbriquer vraiment les
  // clés changerait leur chemin et ferait perdre à tous les joueurs ce qu'ils
  // ont réglé ; ici le document reste le même mapping plat, en plus lisible.
  //
  // ⚠ IL VAUT POUR LA PROCHAINE DÉCLARATION, et ne s'écrit que si elle vient :
  // un groupe vide ne laisse pas de titre orphelin.
  void Section(std::string title);

  // Déclare un réglage. ⚠ L'ORDRE DE DÉCLARATION EST L'ORDRE DU FICHIER : les
  // réglages d'un même sujet se déclarent ensemble pour se lire ensemble.
  //
  // ⚠ LA CLÉ EST EN ANGLAIS (ADR 0093), comme toute clé d'un fichier de
  // `savedata\` : elle s'écrit telle qu'on la déclare.
  void Declare(std::string key, bool* value);
  void Declare(std::string key, int* value);
  void Declare(std::string key, float* value);
  void Declare(std::string key, std::string* value);
  // ⚠ UNE COULEUR EST UN ENTIER NON SIGNÉ, en ABGR, transparence comprise —
  // la même forme que le fichier des onglets. La ranger dans un `int` la
  // ferait ressortir négative du fichier dès que le canal alpha est plein.
  void Declare(std::string key, std::uint32_t* value);

  // UNE CLÉ QUI A EXISTÉ ET QUI N'EXISTE PLUS : elle sort du fichier à la
  // prochaine écriture.
  //
  // 🔴 SANS CELA, UN RÉGLAGE RETIRÉ EST IMMORTEL. Ce qu'on ne connaît pas est
  // conservé — c'est ce qui protège les réglages d'une version plus récente —,
  // et une clé qu'on vient de supprimer est indiscernable de celles-là : elle
  // irait donc s'installer sous « écrit par une autre version », ce qui est un
  // mensonge, et le joueur qui la lirait chercherait le réglage correspondant.
  //
  // ⚠ SE DÉCLARE COMME LES AUTRES, AVANT `Load`, et la ligne peut disparaître
  // une fois que plus personne n'a de fichier assez vieux pour la porter.
  void Retire(std::string key);

  // Lit le fichier et pose les valeurs. Un fichier ABSENT n'est pas une erreur —
  // c'est la première partie, et les défauts font l'affaire. Une clé absente,
  // illisible ou d'un autre type laisse le défaut en place.
  void Load();

  // Remet tous les réglages déclarés à la valeur qu'ils avaient à la
  // déclaration. N'écrit rien : c'est à l'appelant de demander l'écriture, une
  // fois qu'il a reposé ces valeurs dans le moteur.
  void RestoreDefaults();
  // La même chose pour les seuls réglages de cette page : voir `PageScope`.
  void RestoreDefaults(int page);
  // La même chose pour une seule clé : un réglage montré HORS de sa page, comme
  // l'échelle de l'interface dans les réglages du login. Une clé inconnue ne
  // rend rien.
  void RestoreDefault(std::string_view key);

  // La page d'une clé déclarée, ou `kNoSettingsPage`.
  [[nodiscard]] int PageOf(std::string_view key) const;
  // Cette page porte-t-elle au moins un réglage ? Et un réglage de la machine ?
  [[nodiscard]] bool HasPage(int page) const;
  [[nodiscard]] bool PageHoldsMachine(int page) const;
  // TOUTES les valeurs présentes, celles de la machine comprises.
  [[nodiscard]] SettingsValues Values() const;

  // ── CE QU'UN PROFIL DE RÉGLAGES EMPORTE : tout, sauf la machine ──────────
  // Les valeurs présentes.
  [[nodiscard]] SettingsValues SharedValues() const;
  // Les valeurs de déclaration : un profil remis à zéro.
  [[nodiscard]] SettingsValues SharedDefaults() const;
  // Pose ces valeurs. Une clé absente — un réglage plus récent que le profil —
  // ou illisible prend sa valeur de déclaration. N'écrit rien, comme
  // `RestoreDefaults`.
  void AssignShared(const SettingsValues& values);
  // Grandit à chaque `Touch` : qui suit les réglages compare ce nombre.
  [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

  // Un réglage a bougé. ⚠ RELANCE L'ATTENTE : c'est tout l'objet de
  // l'anti-rebond. Un curseur qu'on fait glisser rend « changé » à CHAQUE
  // image ; écrire à chacune serait une sérialisation complète et un cycle
  // disque par image, sur le fil qui dessine.
  void Touch() noexcept;

  // À appeler une fois par image. Écrit quand le joueur s'est arrêté.
  void Tick(std::uint32_t now_ms);

  // Écrit tout de suite s'il reste quelque chose en attente. À employer avant ce
  // qui pourrait faire perdre le délai — la fermeture du jeu.
  void Flush();

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
  [[nodiscard]] bool waiting() const noexcept { return dirty_; }

 private:
  using Live = std::variant<bool*, int*, float*, std::uint32_t*, std::string*>;
  using Value = std::variant<bool, int, float, std::uint32_t, std::string>;

  struct Entry {
    // La clé écrite.
    std::string key;
    Live live;
    Value fallback;
    // L'intertitre écrit AVANT cette clé, ou vide. ⚠ PORTÉ PAR SON PREMIER
    // MEMBRE, comme le titre de famille d'une section du panneau : un groupe
    // sans réglage n'a alors aucun moyen de laisser un titre derrière lui.
    std::string heading;
    // Un réglage de la machine : voir `MachineScope`.
    bool machine = false;
    // Voir `PageScope`.
    int page = kNoSettingsPage;
  };

  void Add(std::string key, Live live, Value fallback);
  static void RestoreEntry(const Entry& entry);
  void Write();

  std::filesystem::path path_;
  std::vector<Entry> entries_;

  // L'intertitre que la prochaine déclaration emportera. Voir `Section`.
  std::string pending_section_;

  // LES CLÉS RETIRÉES. Voir `Retire` : lues, jamais conservées.
  std::vector<std::string> retired_;

  // CE QUE CE CLIENT NE CONNAÎT PAS, relevé à la lecture et réécrit tel quel.
  //
  // ⚠ EN TEXTE, ET RELEVÉ UNE SEULE FOIS : voir `Load`. L'écriture relisait le
  // fichier pour ne rien perdre, ce qui coûtait une relecture complète à chaque
  // réglage touché — sur le fil qui dessine.
  std::vector<std::pair<std::string, std::string>> foreign_;

  // L'attente : `dirty_` dit qu'il reste à écrire, `restart_` qu'un changement
  // vient d'arriver et que le décompte repart de zéro.
  bool dirty_ = false;
  bool restart_ = false;
  std::uint32_t dirty_since_ms_ = 0;

  // La dernière lecture a-t-elle échoué ? Voir l'en-tête : on n'écrit plus.
  bool load_failed_ = false;

  // Voir `MachineScope` : plus que zéro, la déclaration est de la machine.
  int machine_depth_ = 0;
  // Voir `PageScope`.
  int page_ = kNoSettingsPage;
  std::uint64_t revision_ = 0;
};

// Le délai d'attente avant écriture. Assez long pour couvrir un glissement de
// curseur, assez court pour qu'un réglage posé puis le jeu fermé d'un coup soit
// déjà sur le disque.
inline constexpr std::uint32_t kSettingsFlushDelayMs = 700;

// Le fichier, dans `savedata\` du client — avec les touches et la barre.
inline constexpr std::string_view kSettingsFileName = "settings.yml";

}  // namespace moonclient
