#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/chat_line.h"

namespace moonclient {

// Ce que les conversations 1:1 ont pris au fil : voir `chat/whisper_desk.h`.
class WhisperDesk;

// LES ONGLETS DE LA CONVERSATION : ce que chacun accepte de montrer.
//
// ⚠ CONFIGURABLES, COMME DANS LE CLIENT. Il ne s'agit pas de trois onglets
// figés : le joueur les crée, les nomme, et coche pour chacun les types de
// message qu'il veut y voir. C'est ce que le client garde dans son
// `ChatWndInfo_U.lua`, et c'est ce qui fait qu'un joueur peut se tailler un
// onglet « commerce » que personne n'avait prévu.
//
// ⚠ LE FILTRE EST UN MASQUE DE TYPES — les vingt-huit cases de
// `ro::sim::ChatType`, dans l'ordre du client d'origine —, et le type est la
// seule chose qui décide. Le genre d'une ligne règle son allure, pas sa case.
//
// ⚠ ET LE TRI SE FAIT ICI, PAS DANS LA FENÊTRE. `ui` ne connaît pas `sim`, donc
// pas le type d'une ligne : elle reçoit les lignes DÉJÀ choisies pour l'onglet
// courant. C'est ce qui évite de faire traverser une énumération de `sim`
// jusqu'à une couche qui n'en a que faire.

// Le masque d'un onglet : un bit par type.
using ChatTypeMask = std::uint32_t;
static_assert(ro::sim::kChatTypeCount <= sizeof(ChatTypeMask) * 8,
              "un bit par type : le masque doit les tenir tous");

[[nodiscard]] constexpr ChatTypeMask MaskOf(ro::sim::ChatType type) {
  return ChatTypeMask{1} << static_cast<std::uint8_t>(type);
}

// Tous les types à la fois — ce que coche « Tout cocher ».
[[nodiscard]] constexpr ChatTypeMask EveryChatType() {
  return (ChatTypeMask{1} << ro::sim::kChatTypeCount) - 1;
}

// CE QU'UN ONGLET NEUF ACCEPTE : tout, sauf « Mes skills (visée et lancer) ».
//
// ⚠ CETTE CASE-LÀ EST DÉCOCHÉE D'OFFICE : chaque sort armé ou lancé y écrit une
// ligne, et un combat noierait le fil. Le joueur qui veut ce journal la coche.
// Un fichier d'onglets écrit avant elle la relit décochée, pour la même raison
// — voir `chat/chat_tab_file.cpp`.
inline constexpr ChatTypeMask kDefaultChatTypes =
    EveryChatType() & ~MaskOf(ro::sim::ChatType::kSkillUse);

// LES TYPES QU'UN MESSAGE ALIMENTE AUJOURD'HUI.
//
// ⚠ LA SEULE LISTE QUI LE DIT : brancher un type de plus, c'est ajouter sa
// ligne ici. Les autres restent des cases EN ATTENTE — cochables comme les
// autres, mais grisées dans le menu avec une infobulle qui dit pourquoi : un
// joueur qui coche « Altérations d'état » et ne voit rien venir doit savoir
// que ce n'est pas son filtre qui est en cause.
inline constexpr ChatTypeMask kFedChatTypes =
    MaskOf(ro::sim::ChatType::kPublic) | MaskOf(ro::sim::ChatType::kPublicChat) |
    MaskOf(ro::sim::ChatType::kWhisper) | MaskOf(ro::sim::ChatType::kPartyChat) |
    MaskOf(ro::sim::ChatType::kGuildChat) | MaskOf(ro::sim::ChatType::kAllianceChat) |
    MaskOf(ro::sim::ChatType::kItemGetDrop) | MaskOf(ro::sim::ChatType::kEquipmentOnOff) |
    MaskOf(ro::sim::ChatType::kPartyItem) | MaskOf(ro::sim::ChatType::kDamagedEquipment) |
    MaskOf(ro::sim::ChatType::kSkillFailure) | MaskOf(ro::sim::ChatType::kPartySetup) |
    MaskOf(ro::sim::ChatType::kBattle) | MaskOf(ro::sim::ChatType::kPartyBattle) |
    MaskOf(ro::sim::ChatType::kExperience) | MaskOf(ro::sim::ChatType::kClanChat) |
    MaskOf(ro::sim::ChatType::kCall) | MaskOf(ro::sim::ChatType::kBroadcast) |
    MaskOf(ro::sim::ChatType::kBattlefield) | MaskOf(ro::sim::ChatType::kDiscord) |
    MaskOf(ro::sim::ChatType::kSkillUse);

[[nodiscard]] constexpr bool IsFedChatType(ro::sim::ChatType type) {
  return (kFedChatTypes & MaskOf(type)) != 0;
}

// Le libellé MONTRÉ d'un type, traduit.
[[nodiscard]] const char* ChatTypeLabel(ro::sim::ChatType type);

// L'étiquette du diagnostic, « t07 » : le numéro du type, qui relie une ligne
// à la case qui la laisse passer.
[[nodiscard]] std::string_view ChatTypeTag(ro::sim::ChatType type);

// Le type de ce numéro, ou rien s'il n'en désigne aucun — un fichier relu peut
// porter n'importe quoi.
[[nodiscard]] std::optional<ro::sim::ChatType> ChatTypeOf(int number);

// L'APPARENCE PROPRE À UN ONGLET, telle que le FICHIER la porte.
//
// 🔴 ELLE NE CONNAÎT NI IMGUI NI `ui`, et ce n'est pas une précaution de
// style : cette liste d'onglets vit dans une bibliothèque qui ne lie QUE
// `core`, `sim` et yaml-cpp — c'est l'éditeur de liens qui le vérifie, et c'est
// ce qui la rend essayable sans fenêtre. `ChatTabsView` traduit vers `ui`, comme
// il le fait déjà pour les types.
//
// ⚠ UN DRAPEAU, ET NON UNE VALEUR SENTINELLE PAR CHAMP. Le joueur veut « comme
// les autres » ou « à moi » ; dans le premier cas ses onglets doivent SUIVRE
// un changement du réglage général, pas en garder la copie du jour où ils l'ont
// recopié.
//
// ⚠ ZÉRO EST UNE COULEUR VALIDE — le noir transparent — et ne peut donc pas
// dire « rien ». C'est `own` qui dit si `body` a un sens ; sans lui, la vue
// remplit ce champ depuis le réglage général avant de le montrer.
struct TabLook {
  bool own = false;
  int font_percent = 100;
  int padding_pixels = 0;
  int line_gap_pixels = 0;
  std::uint32_t body = 0;
};

// CE QU'UN ONGLET MONTRE : le fil des messages, ou le compteur de dégâts rangé
// parmi eux.
//
// ⚠ AU PLUS UN ONGLET COMPTEUR, et il n'a ni types ni vidage : il ne montre
// aucune ligne. Seul dans une flottante, il la quitte pour redevenir la fenêtre
// du compteur — voir `ChatTabs::lone_meter`.
enum class ChatTabContent : std::uint8_t { kLog, kDamageMeter };

// Un onglet : son nom, et ce qu'il laisse passer.
struct ChatTab {
  std::string name;
  ChatTypeMask accepted = 0;
  ChatTabContent content = ChatTabContent::kLog;
  // ── « VIDER CET ONGLET » ──────────────────────────────────────
  // Le rang de la dernière ligne au moment du geste : cet onglet ne montre
  // plus que ce qui est arrivé APRÈS. Zéro = rien n'a été vidé.
  //
  // ⚠ UN MASQUE, PAS UNE SUPPRESSION. La conversation est un tampon PARTAGÉ par
  // tous les onglets : en retirer les lignes d'un onglet les arracherait à tous
  // ceux qui les acceptent aussi. Vider « Guilde » ne doit rien coûter à
  // l'onglet qui montre tout. C'est aussi ce qui rend le geste réversible.
  //
  // ⚠ NON PERSISTÉ, et il ne peut pas l'être : les rangs sont réattribués à
  // chaque session, donc un rang relu d'un fichier ne désignerait plus rien. Le
  // vidage vaut pour la session en cours.
  std::uint64_t cleared_through = 0;
  // ── LA FENÊTRE QUI LE MONTRE ─────────────────────────
  // ZÉRO = la principale, toute autre valeur = une flottante.
  //
  // ⚠ UNE FENÊTRE EST UN GROUPE, PAS UN ONGLET. Le client d'origine ne sait
  // faire que deux choses d'un onglet : le laisser dans la principale, ou lui
  // donner une fenêtre à lui. Ici PLUSIEURS onglets peuvent partager une même
  // flottante, comme les onglets d'un navigateur, et le joueur de Moonlight le
  // connaît.
  //
  // 🔴 L'IDENTIFIANT NE DESCEND PAS DE L'ONGLET FONDATEUR. C'est lui qui
  // nomme la fenêtre, donc ce qui lui garde sa position et sa taille ; or le
  // fondateur peut la quitter, ou disparaître, alors qu'elle reste. Un
  // compteur à elle, donc, JAMAIS réemployé — un numéro rendu ferait hériter
  // la fenêtre neuve de la géométrie de l'ancienne.
  std::uint32_t group = 0;
  // LA GÉOMÉTRIE DE SA FENÊTRE EST-ELLE FIGÉE ?
  //
  // 🔴 IL DÉCRIT UNE FENÊTRE ET VIT SUR UN ONGLET, et c'est un choix.
  // Il y a un verrou PAR FENÊTRE, et un panneau de réglages ne sait pas de
  // laquelle on parle : une case unique y figerait les trois d'un coup, y
  // compris la flottante qu'on vient d'arracher. Le menu d'un onglet, lui,
  // s'ouvre TOUJOURS depuis une fenêtre précise.
  //
  // ⚠ ÉCRIT SUR TOUS LES ONGLETS DU GROUPE, et relu sur celui qu'on montre.
  // Une copie par onglet plutôt qu'une table de groupes à ranger, à relire et
  // à purger : c'est un booléen, la redondance ne coûte rien, et rien ne peut
  // diverger tant que les deux écritures passent par `SetLocked`.
  //
  // ⚠ UNE FLOTTANTE NAÎT LIBRE : on vient de la placer, la figer aussitôt
  // serait la figer où le hasard du lâcher l'a mise.
  bool locked = false;
  // ── SON APPARENCE À LUI ──────────────────────────────
  // Tant que `own` est faux, l'onglet SUIT les réglages généraux — et leurs
  // changements à venir.
  TabLook look;
  // ── CE QUI EST LU ────────────────────────────────────
  // Le rang de la dernière ligne vue quand l'onglet était montré : ce qui le
  // suit et qu'il accepte marque sa languette. ⚠ NON PERSISTÉ, comme le vidage :
  // les rangs repartent à chaque session.
  std::uint64_t read_through = 0;

  [[nodiscard]] bool Accepts(ro::sim::ChatType type) const {
    return (accepted & MaskOf(type)) != 0;
  }
};

// LE NOM D'UN ONGLET NEUF : « NewTab_2 », puis le premier numéro libre.
//
// ⚠ LE NOM DU CLIENT D'ORIGINE, numéroté à partir de deux comme lui : les
// joueurs le reconnaissent. Les homonymes restent permis — on peut renommer —,
// mais en proposer un d'office serait désagréable.
[[nodiscard]] std::string NewTabName(std::span<const ChatTab> tabs);

// UNE LIGNE QUE CET ONGLET MONTRERAIT EST-ELLE ARRIVÉE DEPUIS QU'ON L'A LU ?
// `desk`, facultatif, retire ce qu'une conversation 1:1 a pris au fil.
[[nodiscard]] bool TabHasUnread(const ChatTab& tab, std::span<const ro::sim::ChatLine> chat,
                                const WhisperDesk* desk);

// LA LISTE DES ONGLETS, et ce qu'on en fait.
//
// ⚠ ELLE N'EST JAMAIS VIDE. Un joueur qui retire son dernier onglet n'aurait
// plus rien où lire, et rien où cliquer pour en refaire un : le dernier ne se
// retire pas.
class ChatTabs {
 public:
  // Part avec l'onglet du client : un seul, qui accepte `kDefaultChatTypes`.
  ChatTabs();

  [[nodiscard]] std::span<const ChatTab> tabs() const noexcept { return tabs_; }
  [[nodiscard]] std::size_t count() const noexcept { return tabs_.size(); }

  // Ajoute un onglet qui accepte `kDefaultChatTypes`, et rend son rang.
  std::size_t Add(std::string name);
  void Rename(std::size_t rank, std::string name);
  // Sans effet sur le dernier onglet DE FIL : voir l'en-tête — le compteur ne
  // donne rien à lire.
  void Remove(std::size_t rank);
  // Déplace un onglet AVANT le rang `before`.
  //
  // ⚠ `before` EST COMPTÉ DANS LA BANDE TELLE QU'ELLE EST DESSINÉE, celui
  // qu'on déplace COMPRIS — c'est ce que le joueur voit, donc la seule
  // numérotation qui puisse correspondre à son geste. La conversion vers le
  // rang d'arrivée réel se fait ici, et nulle part ailleurs.
  void Move(std::size_t rank, std::size_t before);
  void SetAccepted(std::size_t rank, ChatTypeMask accepted);
  void SetLook(std::size_t rank, const TabLook& look);
  // Sort cet onglet dans une fenêtre à LUI, et rend le numéro de celle-ci.
  // Sans effet s'il y est déjà seul : le geste n'aurait rien à faire, et il
  // consommerait un numéro.
  std::uint32_t Detach(std::size_t rank);
  // Le pose dans cette fenêtre-là ; zéro le rend à la principale.
  void Attach(std::size_t rank, std::uint32_t group);

  // ── LE COMPTEUR DE DÉGÂTS, RANGÉ COMME UN ONGLET ──────────────────────────
  // Le rang de l'onglet compteur, ou rien s'il n'est rangé nulle part.
  [[nodiscard]] std::optional<std::size_t> MeterRank() const;
  // Le range dans cette fenêtre, AVANT le rang réel `before` — créé s'il
  // manque, déplacé sinon.
  void DockMeter(std::string name, std::uint32_t group, std::size_t before);
  // Le retire des onglets : le compteur redevient une fenêtre à lui.
  void UndockMeter();
  // Vrai quand il est SEUL dans une flottante : une fenêtre de conversation
  // qui ne porte que lui n'a plus de raison d'être.
  [[nodiscard]] bool lone_meter() const;

  // Fige ou libère la géométrie d'une FENÊTRE : tous ses onglets reçoivent le
  // même verrou, puisque c'est la fenêtre qu'il décrit.
  void SetGroupLocked(std::uint32_t group, bool locked);
  // Les fenêtres VIVANTES, la principale d'abord, puis les flottantes dans
  // l'ordre où leurs onglets paraissent.
  //
  // ⚠ LA PRINCIPALE EST TOUJOURS DE LA LISTE, même vide : elle porte la
  // saisie, et une conversation où l'on ne peut plus rien écrire parce qu'on a
  // sorti tous ses onglets serait un piège sans retour.
  [[nodiscard]] std::vector<std::uint32_t> Groups() const;

  // Masque tout ce qui précède ce rang, ou lève le masque avec zéro.
  //
  // ⚠ NE COMPTE PAS COMME UN CHANGEMENT : ce champ ne s'écrit pas dans le
  // fichier, et lever le compte ferait réécrire celui-ci pour rien.
  void ClearThrough(std::size_t rank, std::uint64_t seq);
  // Cet onglet est lu jusqu'à ce rang. ⚠ PAS UN CHANGEMENT NON PLUS, pour la
  // même raison que le vidage.
  void MarkRead(std::size_t rank, std::uint64_t seq);

  // Remplace tout — ce que fait la lecture du fichier. Une liste sans onglet
  // de fil est refusée, pour la même raison que le retrait du dernier ; un
  // second onglet compteur est oublié.
  void Replace(std::vector<ChatTab> tabs);

  // ⚠ UN COMPTE DE CHANGEMENTS, PAS UN DRAPEAU. La liste se modifie depuis la
  // fenêtre et depuis la lecture du fichier ; un drapeau qu'il faudrait penser à
  // lever au troisième endroit serait oublié.
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

 private:
  // Cette fenêtre porte-t-elle encore au moins un onglet ? Un seul ?
  [[nodiscard]] bool Holds(std::uint32_t group) const;
  [[nodiscard]] bool AloneIn(std::uint32_t group) const;

  std::vector<ChatTab> tabs_;
  std::uint32_t revision_ = 0;
  // Le prochain numéro de fenêtre. ⚠ RELEVÉ À LA LECTURE DU FICHIER, au-dessus
  // du plus grand qu'il porte : repartir de un rendrait à une fenêtre neuve le
  // numéro d'une fenêtre encore ouverte, donc sa géométrie.
  std::uint32_t next_group_ = 1;
};

}  // namespace moonclient
