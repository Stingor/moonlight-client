#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/sim/shortcut_bar.h"

namespace moonclient {

// LES RACCOURCIS DU JOUEUR : ce que chaque touche déclenche.
//
// ⚠ UNE TABLE, ET NON UNE SUITE DE `else if`. Écrites en dur dans le traitement
// du clavier, aucune touche ne pourrait se changer, aucune ne pourrait se
// MONTRER, et rien ne dirait qu'une touche en cache une autre. Une table rend
// les trois possibles d'un coup.
//
// ⚠ LA TABLE EST LA SEULE SOURCE. Le traitement du clavier l'interroge, la
// fenêtre de configuration l'écrit, et les réglages la relisent : il n'y a pas de
// second endroit où une touche serait décidée.

// Ce qu'une touche peut déclencher, quand ce n'est pas une case de barre.
//
// ⚠ L'ORDRE EST CELUI DE LA FENÊTRE DE CONFIGURATION, et c'est la seule raison
// d'un ordre : le joueur y cherche par famille, pas par ordre d'ajout.
//
// 🔴 LES CASES DE BARRE N'Y SONT PAS, et elles sont pourtant dans la table :
// elles occupent les rangs qui suivent ceux-ci — voir `ActionRef`. Les
// énumérer demanderait quatre-vingt-seize `case` dans trois `switch`
// différents, donc trois occasions d'en oublier une.
enum class GameAction : std::uint8_t {
  kOpenChat,
  kGameMenu,
  // 🔴 UNE TOUCHE PAR ONGLET, ET NON UNE POUR LA FENÊTRE. Une seule touche ne
  // peut ouvrir qu'un onglet : le DERNIER vu — c'est-à-dire celui dont on ne
  // sait pas lequel c'est avant d'avoir appuyé. Le jeu, lui, en donne une par
  // fenêtre, parce que ce sont trois fenêtres ; ici ce sont trois onglets, et
  // la touche doit suivre l'onglet, pas le cadre.
  //
  // ⚠ ALT+S EST CELLE DU GRIMOIRE, pas celle de la fiche : c'est la répartition
  // du jeu, où Alt+S ouvre l'arbre des compétences, Alt+Q le mannequin et Alt+G
  // la guilde. Le costume n'a pas de touche d'usine — le jeu n'a pas cette
  // fenêtre-là, et une touche prise à tout le monde pour un onglet qu'on ouvre
  // rarement retirerait du clavier une place qui vaut mieux.
  kOpenCharacterEquipment,
  kOpenCharacterCostume,
  kOpenCharacterGuild,
  kOpenCharacterGrimoire,
  // LES ONGLETS DE L'HOMONCULE ET DU MERCENAIRE. ⚠ ALT+R ET CTRL+R, les touches
  // du jeu, et elles ne font rien tant que le compagnon n'existe pas : le jeu
  // refuse d'ouvrir sa fenêtre sans lui.
  kOpenCharacterHomunculus,
  kOpenCharacterMercenary,
  // LA PETITE FENÊTRE DE STATUT, à côté de la fiche. ⚠ ALT+A, la touche du jeu
  // pour cette fenêtre-là ; A seul reste à la troisième barre.
  kOpenStatus,
  // LA FICHE DU PET. ⚠ SANS TOUCHE D'USINE : le jeu n'en a pas — on n'y
  // accède que par le menu du pet —, et toute touche libre est déjà prise.
  kOpenPetWindow,
  kOpenInventory,
  kOpenCart,
  // LA FENÊTRE AMIS / GROUPE, une touche par onglet comme la fiche. ⚠ ALT+Z ET
  // ALT+H, les touches du jeu : celle de la fenêtre de groupe et celle de la
  // liste d'amis.
  kOpenParty,
  kOpenFriends,
  // LA BANQUE DE ZENY. ⚠ ELLE N'OUVRE RIEN TOUTE SEULE : la touche DEMANDE
  // au serveur, dont la réponse ouvre — ou referme. Voir `ro/net/bank.h`.
  kOpenBank,
  // L'ALBUM DE CARTES. ⚠ SANS TOUCHE D'USINE : aucune fenêtre du jeu n'existe
  // pour lui, donc aucune touche native à reprendre, et toute touche libre est
  // déjà prise par quelqu'un. Le joueur choisit la sienne.
  kOpenCardAlbum,
  kOpenSkillBook,
  kOpenChatMacros,
  // L'ÉDITEUR DE STYLE. ⚠ ALT+P : les joueurs de ce serveur l'ont déjà dans
  // les doigts, et le jeu n'a pas cette fenêtre-là — il n'y a donc aucune
  // touche native à respecter. Sans elle, la fenêtre ne s'ouvrirait que par le
  // menu du jeu ou par un PNJ styliste.
  kOpenStyleEditor,
  // L'ATLAS DES RECETTES. ⚠ SANS TOUCHE D'USINE : c'est un outil qu'on ouvre en
  // préparant une fabrication, et une touche prise à tout le monde pour lui
  // retirerait du clavier une place qui vaut mieux. La commande `/atlas` et le
  // bouton de sa page de réglages l'ouvrent aussi.
  kOpenCraftAtlas,
  // LA NAVIGATION. ⚠ SANS TOUCHE D'USINE : l'icône du menu et la minimap
  // l'ouvrent déjà.
  kOpenNavigation,
  // LA CARTE DU MONDE. ⚠ SANS TOUCHE D'USINE : le menu de la minimap l'ouvre
  // déjà.
  kOpenWorldMap,
  // LE CARNET DE CHASSE MVP. ⚠ SANS TOUCHE D'USINE : toute touche libre est
  // déjà prise ; l'icône du menu et le bouton de sa page l'ouvrent.
  kOpenMvpTracker,
  // LA MINIMAP, montrée ou masquée. ⚠ CTRL+TAB, la touche du jeu.
  kToggleMinimap,
  // LA BOUTIQUE DE VOTE. ⚠ ELLE N'OUVRE RIEN TOUTE SEULE, comme la banque : la
  // touche DEMANDE au serveur, dont la réponse ouvre — ou referme. ⚠ SANS
  // TOUCHE D'USINE, comme dans le jeu, dont la table des raccourcis ne lui en
  // donne pas.
  kOpenCashShop,
  // LE COURRIER. ⚠ SANS TOUCHE D'USINE : l'icône du menu l'ouvre déjà. Ouvrir
  // demande la liste au serveur ; refermer n'émet rien.
  kOpenMail,
  // LES SUCCÈS. ⚠ SANS TOUCHE D'USINE : l'icône du menu l'ouvre déjà. Rien ne
  // part vers le serveur.
  kOpenAchievements,
  // LE JOURNAL DES QUÊTES : Alt+U, la touche du jeu. Rien ne part vers le
  // serveur : le journal est déjà là.
  kOpenQuests,
  // LE SALON DE DISCUSSION : Alt+C, la touche du jeu. Elle ouvre et referme le
  // formulaire ; dans un salon, elle montre la salle.
  kOpenChatRoom,
  // L'INSTANCE : Alt+B, la touche du jeu. Rien ne part vers le serveur.
  kOpenMemorialDungeon,
  // LE MARCHÉ. ⚠ SANS TOUCHE D'USINE : l'icône du menu et `/marche` l'ouvrent
  // déjà. Joué à l'image suivante par son étage ; c'est le serveur qui juge la
  // ville.
  kOpenMarket,
  // LE RAPPORT DE BUG, sans contexte : carte et position viennent du serveur.
  // ⚠ CTRL+ALT+B : les joueurs de ce serveur l'ont déjà dans les doigts. C'est aussi le recours
  // quand la fenêtre en cause est trop étroite pour garder son bouton, ou quand aucune n'est
  // ouverte.
  kReportBug,
  kFrameWholeMap,
  kSitOrStand,
  kEffectBench,
  // L'ENREGISTREUR DE ZONE. ⚠ TROIS TOUCHES ET NON UNE, parce qu'elles servent
  // dans l'autre sens : celle qui filme n'ouvre pas le tracé, et une fois la
  // zone connue il n'existerait plus aucun moyen de la RECADRER sans rouvrir le
  // panneau — c'est-à-dire sans recouvrir l'écran qu'on veut justement cadrer.
  // Et un tutoriel alterne le film et l'arrêt sur image : obliger à choisir dans
  // un panneau à chaque fois reviendrait à l'ouvrir devant ce qu'on capture.
  kCaptureRecord,
  kCapturePicture,
  kCaptureZone,
  // L'ÉCRAN DE VEILLE, À LA DEMANDE. ⚠ SANS TOUCHE D'USINE : le geste est celui
  // de quelqu'un qui se lève de sa chaise, et prendre une touche du clavier de
  // tout le monde pour cela reviendrait à la retirer à ceux qui ne s'en servent
  // jamais.
  kAfkScreen,
  // LE CIBLAGE AU CLAVIER, sans touche d'usine : celle qui vient à l'esprit,
  // Tab, sert au chat, et Échap a son propre réglage dans le mode Ciblage.
  kTargetNearest,
  kTargetPrevious,
  kTargetNext,
  kTargetClear,
  // « EN ATTENTE » DE L'HOMONCULE ET DU MERCENAIRE, une bascule. ⚠ ALT+T ET
  // CTRL+T, les touches du jeu. Sans compagnon, elles ne font rien.
  kHomunculusStandby,
  kMercenaryStandby,
  // L'ORDRE DIRECT À UN COMPAGNON : aller à la case cliquée, ou attaquer le
  // monstre cliqué. ⚠ UN CLIC SUR LE MONDE, jamais une touche — voir
  // `ActionInput::kWorldClick`. Alt + clic droit et Alt + clic gauche, les
  // gestes du jeu.
  kHomunculusOrder,
  kMercenaryOrder,
  // En dernier : leur intertitre coifferait les lignes suivantes, et la remise
  // doit se lire après `kFrameWholeMap` pour reprendre sa touche.
  kCameraReset,
  kCameraResetYaw,
  kCameraResetTilt,
  kCameraResetZoom,
  kCameraResetTiltZoom,
  kCameraOrbit,
  kCameraTilt,
  kCameraOrbitZoom,
  kCameraTiltZoom,
  kCameraZoomWheel,
  kCameraTiltWheel,
};
inline constexpr std::size_t kNamedActionCount =
    static_cast<std::size_t>(GameAction::kCameraTiltWheel) + 1;

// LES DIX MACROS DE CHAT, et ce n'est pas un chiffre rond choisi au hasard :
// c'est la taille du vecteur du client, celle que ses dix comportements de
// raccourci indexent.
//
// ⚠ ELLE EST DÉCLARÉE DEUX FOIS, ICI ET DANS `ro/ui/chat_macros.h`, et ce n'est
// pas un oubli : `moonclient_hotkeys` ne lie PAS `ro_ui` — cette
// bibliothèque-ci ne connaît que SDL et `sim`, et c'est une propriété qu'on
// tient. Les deux sont recoupées par un `static_assert` dans `key_action.cpp`,
// le seul endroit qui voit les deux : leur divergence est une erreur de
// compilation, pas une remarque en revue.
inline constexpr std::size_t kChatMacroSlots = 10;

// Le premier rang des macros dans la table. ⚠ APRÈS LES CASES DE BARRE : le
// fichier range par NOM, mais la table vit dans un tableau indexé par rang.
inline constexpr std::size_t kMacroFirstRank = kNamedActionCount + ro::sim::kShortcutSlotCount;

// LES NEUF DRAPEAUX, Ctrl+1 à Ctrl+9 au client du joueur : chacun envoie
// l'émoticône d'un drapeau. Une quatrième famille, après les macros.
inline constexpr std::size_t kFlagEmoteSlots = 9;
inline constexpr std::size_t kFlagFirstRank = kMacroFirstRank + kChatMacroSlots;

// TOUT CE QU'UNE TOUCHE PEUT DÉCLENCHER : une action nommée, ou une case de
// barre.
//
// 🔴 UN SEUL TYPE POUR LES DEUX, ET C'EST CE QUI GARDE UNE SEULE TABLE. Une
// seconde table pour les cases rouvrirait ce que la première ferme : deux
// endroits où une touche se décide, et l'un qui cache l'autre. Le contrôle de
// collision, lui, ne verrait qu'un monde sur deux.
//
// ⚠ IL SE CONVERTIT DEPUIS `GameAction` SANS LE DIRE : les appelants qui ne
// connaissent que les actions nommées — et ils sont nombreux — s'écrivent comme
// avant.
class ActionRef {
 public:
  constexpr ActionRef() = default;
  constexpr ActionRef(GameAction named)  // NOLINT(google-explicit-constructor)
      : rank_(static_cast<std::size_t>(named)) {}

  // La case d'index donné, au sens de `ro::sim::ShortcutIndex`.
  [[nodiscard]] static constexpr ActionRef OfShortcut(std::size_t index) {
    return ActionRef(kNamedActionCount + index);
  }

  // LA MACRO DE CHAT DE CE RANG-LÀ.
  //
  // 🔴 UNE TROISIÈME FAMILLE PLUTÔT QUE DIX ACTIONS NOMMÉES, et c'est le même
  // raisonnement que pour les cases : dix entrées dans l'énumération feraient
  // dix `case` dans trois `switch`, donc trois occasions d'en oublier une. Ce
  // qui se ressemble se range ensemble.
  //
  [[nodiscard]] static constexpr ActionRef OfMacro(std::size_t slot) {
    return ActionRef(kMacroFirstRank + slot);
  }

  // LE DRAPEAU DE CE RANG-LÀ, zéro pour Ctrl+1.
  [[nodiscard]] static constexpr ActionRef OfFlag(std::size_t slot) {
    return ActionRef(kFlagFirstRank + slot);
  }

  // Le rang dans la table. ⚠ C'EST AUSSI CE QUE LA FENÊTRE RENVOIE quand le
  // joueur clique une ligne : la liste est écrite dans cet ordre.
  [[nodiscard]] constexpr std::size_t rank() const noexcept { return rank_; }

  // 🔴 LES DEUX FAMILLES SE DISTINGUENT, ET C'EST VITAL : un `is_shortcut()` qui
  // vaudrait « tout ce qui n'est pas nommé » ferait tomber une macro dans le
  // lancement d'une case de barre — avec un index hors compte, donc un
  // raccourci muet que rien n'expliquerait.
  [[nodiscard]] constexpr bool is_shortcut() const noexcept {
    return rank_ >= kNamedActionCount && rank_ < kMacroFirstRank;
  }
  [[nodiscard]] constexpr bool is_macro() const noexcept {
    return rank_ >= kMacroFirstRank && rank_ < kFlagFirstRank;
  }
  [[nodiscard]] constexpr bool is_flag() const noexcept { return rank_ >= kFlagFirstRank; }

  // L'action nommée, ou rien quand c'est une case ou une macro.
  [[nodiscard]] constexpr std::optional<GameAction> named() const noexcept {
    if (rank_ >= kNamedActionCount) {
      return std::nullopt;
    }
    return static_cast<GameAction>(rank_);
  }

  // L'index de la case. ⚠ N'A DE SENS QUE SI `is_shortcut()` : sinon il rend un
  // nombre énorme, que `ShortcutBars` refusera comme toute case hors compte.
  [[nodiscard]] constexpr std::size_t shortcut_index() const noexcept {
    return rank_ - kNamedActionCount;
  }

  // Le rang de la macro. ⚠ N'A DE SENS QUE SI `is_macro()`.
  [[nodiscard]] constexpr std::size_t macro_slot() const noexcept {
    return rank_ - kMacroFirstRank;
  }

  // Le rang du drapeau. ⚠ N'A DE SENS QUE SI `is_flag()`.
  [[nodiscard]] constexpr std::size_t flag_slot() const noexcept { return rank_ - kFlagFirstRank; }

  [[nodiscard]] constexpr bool operator==(const ActionRef&) const = default;

 private:
  explicit constexpr ActionRef(std::size_t rank) : rank_(rank) {}

  std::size_t rank_ = 0;
};

inline constexpr std::size_t kBindableCount = kFlagFirstRank + kFlagEmoteSlots;

// Une touche, avec ce qu'il faut tenir en même temps.
//
// La touche est un code de la bibliothèque de fenêtrage — un entier opaque ici :
// cette table ne sait pas les nommer, et c'est la boucle qui traduit.
struct Binding {
  std::uint32_t key = 0;
  bool ctrl = false;
  bool shift = false;
  bool alt = false;

  [[nodiscard]] bool bound() const noexcept { return key != 0; }
  [[nodiscard]] bool operator==(const Binding&) const = default;
};

// Un geste de souris est une « touche » marquée d'un bit que SDL n'emploie pas :
// il se lie, se vole et s'enregistre comme une touche.
enum class MouseGesture : std::uint8_t {
  kRightDrag = 1,
  kRightDoubleClick,
  kMiddleDrag,
  kMiddleDoubleClick,
  kWheel,
  // UN CLIC SIMPLE SUR LE MONDE, pour les ordres aux compagnons. ⚠ EN DERNIER :
  // le rang d'un geste est gravé dans les fichiers.
  kLeftClick,
  kRightClick,
};
inline constexpr std::uint32_t kMouseGestureMark = 1U << 28;

[[nodiscard]] constexpr std::uint32_t KeyOfGesture(MouseGesture gesture) noexcept {
  return kMouseGestureMark | static_cast<std::uint32_t>(gesture);
}

[[nodiscard]] std::optional<MouseGesture> GestureOfKey(std::uint32_t key) noexcept;

enum class ActionInput : std::uint8_t {
  // Une touche ou un double-clic.
  kPress,
  kDrag,
  kWheel,
  // UN CLIC SUR LE MONDE, gauche ou droit, AVEC Alt ou Ctrl. 🔴 NI MAJ, NI
  // CLIC NU, NI CTRL + CLIC GAUCHE SEUL : Maj range l'ordre dans la file, le
  // clic nu marche ou ouvre le menu d'entité, et Ctrl + clic gauche est
  // l'attaque continue. Les prendre retirerait au joueur un geste du jeu.
  kWorldClick,
};

[[nodiscard]] ActionInput InputOfAction(ActionRef action);

[[nodiscard]] bool BindingFits(ActionRef action, Binding binding);

// LA CONSIGNE QUE LA FENÊTRE MONTRE pendant qu'elle attend le geste de cette
// action, déjà traduite ; nul pour la consigne ordinaire (« appuyez sur une
// touche »). ⚠ UN ORDRE AUX COMPAGNONS ATTEND UN CLIC : la consigne ordinaire
// ferait chercher une touche que la liaison refuserait.
[[nodiscard]] const char* CaptureHintOfAction(ActionRef action);

[[nodiscard]] std::string FileNameOfKey(std::uint32_t key);
// Zéro quand le nom n'est ni une touche ni un geste connus.
[[nodiscard]] std::uint32_t KeyOfFileName(const std::string& name);

// Le nom d'une action, pour la fenêtre de configuration.
//
// ⚠ EN FRANÇAIS NU, traduit à la lecture : une table de constantes s'initialise
// avant le catalogue, et un texte traduit à sa définition y resterait figé.
//
// ⚠ UNE CHAÎNE COMPOSÉE POUR UNE CASE — « Barre 3, case 7 » —, d'où la copie :
// quatre-vingt-seize littéraux n'auraient été ni traduisibles ni tenables.
[[nodiscard]] std::string NameOfAction(ActionRef action);

// LE NOM DU GROUPE où la fenêtre range la ligne : vide pour une action nommée,
// « Barre 3 » pour une case.
//
// ⚠ C'EST DE L'AFFICHAGE, et rien d'autre n'en dépend. Sans lui, la fenêtre
// aligne quatre-vingt-seize lignes qui se ressemblent toutes.
[[nodiscard]] std::string GroupOfAction(ActionRef action);

// Les onglets de la fenêtre de configuration, dans leur ordre d'affichage.
enum class HotkeyTab : std::uint8_t {
  kBars,
  kInterface,
  kMacros,
  kPlay,
  kCamera,
  kTools,
};
inline constexpr std::size_t kHotkeyTabCount = static_cast<std::size_t>(HotkeyTab::kTools) + 1;

[[nodiscard]] HotkeyTab TabOfAction(ActionRef action);
[[nodiscard]] const char* NameOfTab(HotkeyTab tab);

// LA CLÉ D'UNE ACTION DANS LE FICHIER DE SAUVEGARDE.
//
// ⚠ ELLE NE CHANGE JAMAIS UNE FOIS LIVRÉE. C'est elle qui relie un raccourci
// gardé à l'action qu'il déclenche ; la renommer, c'est rendre à tous les
// joueurs les touches d'origine sans que rien ne le dise. Elle est donc
// DISTINCTE du nom affiché, qui, lui, se traduit et se reformule librement.
// Elle est en anglais (ADR 0093).
//
// ⚠ COMPOSÉE POUR UNE CASE — « bar3_slot7 », les deux nombres à partir de UN.
[[nodiscard]] std::string SavedKeyOfAction(ActionRef action);

// Toutes les actions liables, dans l'ordre de la fenêtre : les nommées, puis les
// cases de barre.
[[nodiscard]] std::span<const ActionRef> AllBindableActions();

// LES OUTILS DE MISE AU POINT QUI ONT UNE TOUCHE : le banc d'effets, et le
// panneau « Lancer un skill », liste brute des compétences apprises — le
// joueur, lui, lance par le grimoire de la fiche et par la barre.
inline constexpr std::array<GameAction, 2> kDevelopmentActions = {GameAction::kEffectBench,
                                                                  GameAction::kOpenSkillBook};

// L'ACTION EXISTE-T-ELLE DANS CETTE CONSTRUCTION ? Celles de
// `kDevelopmentActions` n'existent qu'en Debug (`ro::core::kDevTools`).
// Ailleurs, leur ligne ne se montre pas et leur touche, même retenue d'un
// fichier de raccourcis, ne déclenche rien.
[[nodiscard]] bool ActionInThisBuild(ActionRef action);

// LE LIBELLÉ D'UN RACCOURCI — « Ctrl+Alt+F1 », ou vide s'il n'y en a pas.
//
// ⚠ L'ORDRE DES MODIFICATEURS EST FIXE — Ctrl, Alt, Maj —, et c'est celui du
// client : deux écrans qui les rangeraient autrement se liraient comme deux
// raccourcis différents.
//
// 🔴 ICI PLUTÔT QUE DANS CHAQUE ÉCRAN, ET TOUJOURS AVEC LES MODIFICATEURS, même
// sur une case petite : sept barres sur huit n'ont de touches qu'avec un
// modificateur, et une case qui affiche « F1 » pour Ctrl+F1 MENT — la case
// d'à côté mentirait de la même façon.
[[nodiscard]] std::string LabelOfBinding(Binding binding);

class Hotkeys {
 public:
  // Les raccourcis d'origine, ceux du client. Un joueur qui n'a jamais rien
  // changé doit retrouver exactement ce qu'il avait.
  Hotkeys();

  [[nodiscard]] Binding Of(ActionRef action) const {
    return action.rank() < bindings_.size() ? bindings_[action.rank()] : Binding{};
  }

  // L'action que cette touche déclenche, s'il y en a une.
  //
  // ⚠ LES MODIFICATEURS COMPTENT EXACTEMENT. Alt+S et S sont deux raccourcis
  // distincts — l'un ouvre la fiche, l'autre le grimoire de lancement — et une
  // comparaison qui les confondrait en rendrait un inatteignable.
  //
  // Entrée du pavé numérique vaut Entrée, sauf si une liaison la nomme elle-même.
  [[nodiscard]] std::optional<ActionRef> ActionFor(std::uint32_t key, bool ctrl, bool shift,
                                                   bool alt) const;

  // Faute d'une liaison qui nomme Ctrl, cherche la même sans lui : Ctrl +
  // molette zoome comme la molette seule.
  [[nodiscard]] std::optional<ActionRef> ActionForGesture(MouseGesture gesture, bool ctrl,
                                                          bool shift, bool alt) const;

  // Pose un raccourci, et rend CELUI À QUI LA TOUCHE A ÉTÉ PRISE.
  //
  // ⚠ LA TOUCHE SE LIBÈRE D'ABORD : deux actions sur la même touche en
  // laisseraient une morte, et le joueur ne saurait pas laquelle. Le client fait
  // de même.
  //
  // 🔴 ET ON REND LA VICTIME, PARCE QUE LA TABLE COMPTE PLUS DE CENT LIGNES :
  // l'action dépossédée est ailleurs dans la liste, et le joueur ne verrait
  // jamais ce qu'il vient de perdre. Rien de repris = rien rendu.
  std::optional<ActionRef> Bind(ActionRef action, Binding binding);

  // Rétablit les raccourcis d'origine, tous à la fois.
  void RestoreDefaults();

  // COMBIEN DE FOIS LA TABLE A CHANGÉ.
  //
  // ⚠ C'EST CE QUI REMPLACE UN DRAPEAU « À ENREGISTRER » QU'IL FAUDRAIT PENSER À
  // LEVER. La table se modifie depuis deux endroits — la fenêtre de
  // configuration et la capture au clavier —, et le troisième qui viendra
  // oublierait le drapeau. Qui enregistre compare ce compte au sien.
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

 private:
  std::array<Binding, kBindableCount> bindings_;
  std::uint32_t revision_ = 0;
};

}  // namespace moonclient
