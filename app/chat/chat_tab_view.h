#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/sim/chat_line.h"
#include "ro/ui/chat_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "chat/chat_history_file.h"
#include "chat/chat_item_link.h"
#include "chat/chat_log_file.h"
#include "chat/chat_rich_decor.h"
#include "chat/chat_tabs.h"
#include "chat/whisper_desk.h"
#include "chat/whisper_lines.h"
#include "items/creator_names.h"

namespace moonclient {

// UNE LIGNE QUI PORTE UN RESPAWN DE MVP se remet en page à chaque image : sa
// marque « · passé » change sa largeur, et le cache du fil garderait sinon la
// hauteur d'avant. Son identité de cache est donc NULLE.
[[nodiscard]] bool CarriesMvpLink(const ro::ui::ChatLineView& view);

// LES ONGLETS DE LA CONVERSATION, VUS DE LA BOUCLE.
//
// ⚠ C'EST ICI QUE LE GENRE D'UNE LIGNE SERT, ET NULLE PART AILLEURS. La couleur
// qu'elle portera, l'onglet qui la montrera : les deux se décident du même
// `ChatKind`, et la fenêtre n'en voit rien — elle reçoit des lignes déjà
// choisies et déjà teintes. Même frontière que pour les raccourcis, et elle
// achète la même chose : la chatbox se vérifie dans un essai sans monde.
//
// ⚠ ET LE TRI SE REFAIT À CHAQUE IMAGE, sur toute la conversation :
// `kChatLinesKeptPerKind` lignes par genre, et jusqu'à `kMostChatLinesKept` au
// réglage. Il le peut parce qu'une ligne sans balise ne se décore pas — voir
// `MayCarryChatTags` —, et une liste gardée par onglet aurait à se refaire au
// moindre changement de filtre.
class ChatTabsView {
 public:
  // LES NOMS DES FORGERONS, confiés une fois la description ouverte.
  void SeeCreators(CreatorNames creators) noexcept { creators_ = creators; }

  explicit ChatTabsView(std::filesystem::path client_root);

  // DE QUOI TRADUIRE LES BALISES D'OBJET DU FIL. ⚠ POSÉES APRÈS COUP : la
  // conversation vit dès l'écran de sélection, la table de l'installation n'est
  // lue qu'à l'entrée en jeu. Sans elles, une balise reste à l'écran telle
  // quelle — laide, mais pas perdue.
  void ReadsItems(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words) {
    items_ = items;
    words_ = words;
  }

  // LES NOMS AFFICHÉS DES CARTES, pour les liens de lieu. Absents, un lieu montre
  // le nom interne de sa carte.
  void ReadsMaps(const ro::vfs::MapDisplayNames* maps) { maps_ = maps; }

  // Refait ce que la fenêtre montre : les languettes, les lignes de l'onglet
  // choisi, et les genres de celui qu'on règle. ⚠ SEULEMENT SI ELLE EST
  // OUVERTE : trier toute la conversation pour une fenêtre fermée serait du
  // temps donné à rien.
  //
  // `now_ms` est l'horloge de la boucle, la MÊME que celle des lignes : c'est
  // l'écart entre les deux qui donne l'heure d'une ligne. ⚠ UNE LIGNE NE PORTE
  // PAS D'HEURE : elle porte une DATE DE MACHINE, et il faut un repère pour la
  // traduire — ce repère, c'est maintenant.
  //
  // `diagnostic` lève les filtres et étiquette chaque ligne de son genre. ⚠ IL
  // NE VIENT PAS DE `state` MAIS DES RÉGLAGES : c'est un réglage du joueur, qui
  // survit à la partie, pas un état de fenêtre.
  // ⚠ IL PREND TOUTES LES FENÊTRES, et il en crée ou en oublie les états : une
  // fenêtre dont plus aucun onglet ne relève n'a plus rien à montrer, et garder
  // son état serait une fuite que rien à l'écran ne signalerait.
  // `look` est l'apparence GÉNÉRALE ; celle d'un onglet qui en a une propre la
  // remplace pour sa fenêtre.
  void Refresh(ro::ui::ChatWindows& windows, std::span<const ro::sim::ChatLine> chat,
               std::uint32_t now_ms, const ro::ui::ChatAppearance& look, bool diagnostic);

  // COMBIEN D'ONGLETS EN TOUT, fenêtres détachées comprises.
  //
  // 🔴 LE PLAFOND DU CLIENT EST GLOBAL, pas par fenêtre : son chargeur
  // refuse au-delà de dix, et le onzième disparaîtrait à la reconnexion
  // suivante sans un mot. Compter par bande en laisserait créer trente.
  [[nodiscard]] int count() const noexcept { return static_cast<int>(tabs_.count()); }

  // Les fenêtres vivantes, la principale d'abord.
  [[nodiscard]] std::span<const std::uint32_t> groups() const noexcept { return living_; }

  // Ce que CETTE fenêtre-là montre. Vide pour un groupe que le tri ne connaît
  // pas : il n'a rien à dessiner.
  [[nodiscard]] std::span<const ro::ui::ChatStripTab> tabs(std::uint32_t group) const;
  [[nodiscard]] std::span<const ro::ui::ChatLineView> lines(std::uint32_t group) const;
  // Le cache de mise en page du fil de CETTE fenêtre, ou nul pour un groupe
  // inconnu — voir `ro::ui::ChatLogLayout`.
  //
  // 🔴 ICI, ET NON DANS L'ÉTAT DE LA FENÊTRE, qui survit à la partie : c'est
  // cette vue qui distribue les identifiants des lignes, et le cache ne vaut que
  // tant qu'ils gardent leur sens. La partie suivante recommence les rangs du
  // monde, et elle recommence avec une vue neuve.
  [[nodiscard]] ro::ui::ChatLogLayout* layout(std::uint32_t group) const;

  // CE QU'UN JETON DE LIEN DÉSIGNE. ⚠ REFAITE À CHAQUE IMAGE, comme les vues :
  // le jeton n'est valable que pour l'image qui l'a rendu, et c'est suffisant —
  // un geste se lit dans la foulée de l'image qui l'a vu.
  [[nodiscard]] const ChatLinkTarget* LinkOf(std::uint32_t token) const;
  [[nodiscard]] std::span<const ro::ui::ChatTypeToggle> types(std::uint32_t group) const;

  // L'apparence que CETTE fenêtre-là doit prendre : la générale, ou celle de
  // son onglet actif quand il en a une.
  [[nodiscard]] const ro::ui::ChatAppearance& look(std::uint32_t group) const;

  // L'apparence de l'onglet en cours de réglage, telle que son menu doit la
  // montrer. ⚠ REMPLIE DEPUIS LA GÉNÉRALE quand l'onglet n'en a pas : les
  // barres doivent partir de ce qu'il a SOUS LES YEUX, pas de zéro.
  [[nodiscard]] const ro::ui::ChatTabLook& tuned_look(std::uint32_t group) const;

  // Fait ce que le joueur a demandé aux onglets, et enregistre si la liste a
  // bougé. Ce qu'il a DIT ne passe pas par ici : c'est le routage du chat.
  //
  // 🔴 APPELÉ APRÈS LE DESSIN DE TOUTES LES FENÊTRES, jamais entre deux. Un
  // retrait, un déplacement ou un détachement réordonne la liste des onglets,
  // et la fenêtre suivante lirait alors des rangs périmés — elle agirait sur
  // l'onglet d'à côté, ou sur celui d'une autre fenêtre.
  void Apply(std::uint32_t group, const ro::ui::ChatWindowAction& asked,
             ro::ui::ChatWindowState& state);

  // Range une languette là où elle vient d'être lâchée.
  //
  // ⚠ LES DEUX RANGS SONT LOCAUX À LEUR BANDE, celui du départ comme celui de
  // l'arrivée, et ce sont deux bandes DIFFÉRENTES. C'est la seule fonction qui
  // traduise deux numérotations à la fois.
  void ApplyDrop(const ro::ui::ChatDrop& drop);

  // ── LE COMPTEUR DE DÉGÂTS RANGÉ COMME UN ONGLET — chat_meter_tab.cpp ──────
  // Est-il rangé dans une conversation ?
  [[nodiscard]] bool meter_docked() const { return tabs_.MeterRank().has_value(); }
  // Cette languette de cette fenêtre est-elle celle du compteur ?
  [[nodiscard]] bool IsMeter(std::uint32_t group, int local) const;
  // Le range dans cette fenêtre, au rang de SA bande — moins un pour tout au
  // bout. Une fenêtre qui n'existe plus se lit comme la principale.
  void DockMeter(std::uint32_t group, int local);
  // Le sort des onglets : il redevient une fenêtre à lui.
  void UndockMeter();
  // Ce que sa languette affiche, refait à chaque image : il porte le rythme.
  void LabelMeter(std::string label) { meter_label_ = std::move(label); }

  // ── LE FIL D'UN SALON — chat_room_lines.cpp ────────────────────────────
  // Ses lignes, décorées comme celles de la conversation et dans le MÊME jeu
  // de jetons de liens : un geste sur l'une se suit comme sur l'autre.
  //
  // ⚠ APRÈS `Refresh`, qui remet les jetons à zéro : appelée avant, elle
  // perdrait les siens.
  void RefreshRoom(std::span<const ro::sim::ChatLine> lines, std::uint32_t room_seq);
  [[nodiscard]] std::span<const ro::ui::ChatLineView> room_lines() const noexcept {
    return room_.lines;
  }
  [[nodiscard]] ro::ui::ChatLogLayout* room_layout() const noexcept { return &room_.layout; }

  // ── LES CHUCHOTEMENTS — chat_whisper_lines.cpp ─────────────────────────
  // Ce qu'une conversation 1:1 a pris ne paraît plus dans le fil. Nul : rien
  // n'est pris.
  void SeesConversations(const WhisperDesk* desk) noexcept { desk_ = desk; }
  // Le nom du personnage joué, en tête de ce qu'on a dit dans une conversation.
  void SeesOwnName(std::string_view own) { own_name_ = own; }
  // CE QUE LE MENU D'UN PSEUDO REFUSE D'AVANCE, demandé une fois par nom et par
  // image : seul l'appelant sait qui est de notre groupe et de notre guilde.
  void JudgesPlayers(std::function<ro::ui::PlayerLinkRefusals(std::string_view)> judge) {
    judge_ = std::move(judge);
  }
  // L'HEURE DU SERVEUR À CETTE IMAGE, qui marque « · passé » un respawn de MVP
  // échu. Posée par la partie avant `Refresh`.
  void ReadsMvpClock(MvpLinkClock clock) noexcept { mvp_clock_ = clock; }
  // CE QUE LE LECTEUR A À CETTE IMAGE, qui décide qu'un lien de réglage se
  // forme. Posé par la partie avant `Refresh`.
  void ReadsSettingReach(SettingReach reach) noexcept { setting_reach_ = reach; }
  // CE QUE LE MENU D'UN `<MVPL>` SAIT D'AVANCE, demandé par lien et par image :
  // seul le carnet de chasse sait ce que le groupe a déjà vu. Sans juge, les
  // faits sont nuls.
  using MvpLinkJudge = std::function<const ro::ui::MvpLinkMenuFacts*(const MvpLink&)>;
  void JudgesMvpLinks(MvpLinkJudge judge) { mvp_judge_ = std::move(judge); }

  // ── LE TEXTE RICHE ET LES REPÈRES DES ONGLETS — chat_tab_marks.cpp ───────
  // Ce que la décoration riche sait de l'image, posé par la partie avant
  // `Refresh`.
  void SeesRich(ChatRichContext context) { rich_ = std::move(context); }
  // Ce qui est posé — les bulles y prennent l'icône d'un objet lié.
  [[nodiscard]] const ChatRichContext& rich() const noexcept { return rich_; }
  // L'adresse de la vignette de cette clé, posée à cette image ; vide sinon.
  [[nodiscard]] std::string_view PictureUrlOf(std::uint32_t key) const;
  // CTRL + MOLETTE : vrai quand l'onglet actif de ce groupe a son apparence à
  // lui, et qu'elle a bougé ; faux, c'est le réglage général qui doit bouger.
  [[nodiscard]] bool ZoomLook(std::uint32_t group, int steps);
  // Le fil de chaque conversation ouverte : toutes les lignes qui nomment son
  // correspondant, dans le tampon du monde. ⚠ APRÈS `Refresh`, comme le salon.
  void RefreshWhispers(std::span<const ro::sim::ChatLine> chat,
                       std::span<const Conversation> conversations);
  [[nodiscard]] std::span<const ro::ui::ChatLineView> whisper_lines(std::string_view peer) const;
  [[nodiscard]] ro::ui::ChatLogLayout* whisper_layout(std::string_view peer) const;

  // ── CE QUI S'EST DIT LA PARTIE D'AVANT ────────────────────────────
  // Appelées par la boucle, PAS par le constructeur : l'option qui les gouverne
  // vit dans les réglages, et les réglages se lisent après.
  //
  // ⚠ DÉCOCHÉE, L'OPTION EFFACE CE QU'ELLE GOUVERNE. Garder sur le disque ce
  // que le joueur vient de refuser serait l'inverse de ce qu'il demande — et ce
  // fichier-là porte ses chuchotements en clair.
  //
  // 🔴 DEUX CASES POUR UN SEUL FICHIER, et c'est ce qui rend l'effacement
  // délicat : la conversation et les lignes tapées répondent à deux questions
  // et se refusent séparément, mais elles vivent au même endroit. Décocher l'une
  // ne supprime donc pas le fichier tant que l'autre y tient encore — ce qu'elle
  // gardait en est RETIRÉ, et le fichier réécrit sur-le-champ.
  void RestorePast(bool keep_chat, bool keep_typed);
  void SavePast(bool keep_chat, int keep, std::span<const std::string> whispered, bool keep_typed,
                std::span<const std::string> typed) const;

  // EFFACE CE QUI EST GARDÉ : le fichier, ET ce qui l'aurait réécrit à la
  // sortie.
  //
  // 🔴 LES DEUX, SINON LE GESTE MENT. Effacer le seul fichier le
  // laisserait revenir intégralement à la fermeture, puisque la sauvegarde
  // recolle le passé relu et ce qu'on a vu passer depuis. Ce qui sera dit
  // APRÈS, lui, sera gardé comme d'habitude — c'est l'option qui en décide,
  // et elle n'a pas changé.
  void ForgetPast();

  // Les destinataires que le fichier portait. Vide si l'option est décochée :
  // `RestorePast` a alors effacé le fichier sans rien en lire.
  [[nodiscard]] std::span<const std::string> restored_whispers() const noexcept {
    return past_.whispered;
  }

  // Les lignes tapées que le fichier portait, de la plus ancienne à la plus
  // récente. Vide si leur case est décochée.
  [[nodiscard]] std::span<const std::string> restored_typed() const noexcept { return past_.typed; }

  // ── LE JOURNAL SUR LE DISQUE (ADR 0033, entrée A4) ────────────────
  // Ouvre ou ferme le journal du jour. Appelée à chaque image : c'est une CASE,
  // pas un évènement, et l'ordre de chargement des réglages n'est pas quelque
  // chose sur quoi il faut parier.
  void KeepLog(bool wanted);

  // `/savechat` : un fichier par onglet, avec ce que l'onglet montre.
  //
  // ⚠ CE QUI EST LU, ET NON CE QUE LE MONDE GARDE : la partie d'avant y est
  // aussi, coiffée de son repère, exactement comme à l'écran.
  [[nodiscard]] ChatLogDump SaveLogs() const;

 private:
  void SaveIfNeeded();
  // Ramasse ce qui vient d'être dit. ⚠ APPELÉE MÊME FENÊTRE FERMÉE : ce qui se
  // dit alors se dit quand même, et le monde efface les plus anciennes lignes
  // de chaque genre.
  void Harvest(std::span<const ro::sim::ChatLine> chat);

  // Ce qu'UNE fenêtre montre, refait à chaque image. Les textes pointent dans
  // `tabs_` et dans la conversation du monde : rien n'est recopié.
  struct GroupViews {
    std::vector<ro::ui::ChatStripTab> tabs;
    // Le rang RÉEL de chaque languette. ⚠ SANS LUI LES DEUX NUMÉROTATIONS SE
    // CONFONDENT : la fenêtre compte ses onglets à partir de zéro, et son
    // deuxième n'est pas le deuxième de la liste.
    std::vector<std::size_t> ranks;
    std::vector<ro::ui::ChatLineView> lines;
    // LES LIGNES DONT UNE BALISE A ÉTÉ TRADUITE, et elles seules.
    //
    // ⚠ UNE FILE, PAS UN VECTEUR : les vues POINTENT dedans, et un vecteur qui
    // grandit les déplacerait sous la main de qui les tient. C'est la même
    // raison qui fait de `shown_` une table ordonnée.
    //
    // ⚠ ET SEULEMENT CELLES-LÀ : la quasi-totalité des lignes n'a pas de balise,
    // et leur vue continue de regarder le texte du monde sans rien recopier.
    std::deque<DecoratedChatLine> decorated;
    std::vector<ro::ui::ChatTypeToggle> types;
    // Ce que cette fenêtre montre de son apparence, et celle que son menu
    // règle. Toutes deux refaites à chaque image.
    ro::ui::ChatAppearance look;
    ro::ui::ChatTabLook tuned;
    // Le rang RÉEL de l'onglet actif, que Ctrl + molette règle ; rien sans onglet.
    std::optional<std::size_t> active;
    // Ce que le fil de cette fenêtre a déjà mis en page. ⚠ `mutable` : c'est un
    // cache, que le dessin tient à jour sans rien changer de ce qu'on montre.
    mutable ro::ui::ChatLogLayout layout;
  };

  // CE QU'UNE LIGNE MONTRE : ses balises traduites, et pour un chuchotement
  // son en-tête et le pseudo cliquable. Les liens prennent leur jeton ici.
  // `sender` est l'expéditeur qu'un `<MVPL>` de la ligne nommera à l'import ;
  // `discord_line`, une ligne relayée de Discord, dont l'auteur se clique.
  // `author`, celui que le serveur a décrit : son nom devient un pseudo
  // cliquable, ou, sur une ligne de Discord, son avatar se pose devant.
  void ShowText(std::string_view raw, ro::ui::ChatLineView& view, GroupViews& views,
                std::string_view sender = {}, bool discord_line = false,
                const ro::sim::ChatAuthor* author = nullptr);
  // UNE LIGNE DONT LE SERVEUR A DÉCRIT L'AUTEUR : sa tête, le nom cliquable,
  // puis sa phrase décorée. Faux, et rien de publié, si l'auteur ne s'accorde
  // pas avec le texte.
  [[nodiscard]] bool ShowAuthored(std::string_view raw, const ro::sim::ChatAuthor& author,
                                  ro::ui::ChatLineView& view, GroupViews& views,
                                  std::string_view sender);
  // CE QU'UNE LIGNE DÉCORÉE PORTE, rendu à la vue : ses liens prennent leur
  // jeton et leurs faits d'adresse, ses vignettes leur clé dans l'image et leur
  // état, ses icônes leur texture. — chat_tab_marks.cpp
  void Publish(DecoratedChatLine line, ro::ui::ChatLineView& view, GroupViews& views);
  // Les tables qui nomment les liens, telles qu'elles sont à cette image.
  [[nodiscard]] ChatDecorTables Tables() const noexcept;
  // L'IDENTITÉ D'UNE LIGNE POUR LE CACHE DU FIL commence par son ASPECT — voir
  // `ro::ui::ChatLineView::id` : ce qui change l'aspect d'une même ligne, posé
  // au-dessus de `kLookShift`. Le basculer change toutes les identités, donc
  // toutes les hauteurs se refont. — chat_tab_marks.cpp
  static constexpr int kLookShift = 56;
  [[nodiscard]] std::uint64_t LookBits(bool diagnostic) const noexcept;
  void ShowLine(const ro::sim::ChatLine& line, WhisperSurface surface, ro::ui::ChatLineView& view,
                GroupViews& views);
  // Ce que le menu de ce pseudo refuse, ou nul sans juge.
  [[nodiscard]] const ro::ui::PlayerLinkRefusals* RefusalsOf(std::string_view name);
  // LES RESPAWNS DE MVP d'une ligne décorée, à partir du morceau `from` : leur
  // expéditeur, et ce que leur menu sait d'avance.
  void NoteMvpLinks(DecoratedChatLine& line, std::size_t from, std::string_view sender);
  // `TabHasUnread` de l'onglet de ce rang, relu seulement quand une ligne est
  // arrivée ou partie, ou que ce qu'il montre a changé. — chat_tab_marks.cpp
  [[nodiscard]] bool UnreadOf(std::size_t rank, std::span<const ro::sim::ChatLine> chat);

  // CE QU'UN ONGLET A RÉPONDU sur ses non-lus, et ce qui l'a décidé. ⚠ COMPARÉ
  // CHAMP À CHAMP : le même onglet aux mêmes lignes répond la même chose.
  struct UnreadMemo {
    // Faux : rien n'a encore été relu à ce rang.
    bool known = false;
    ChatTypeMask accepted = 0;
    ChatTabContent content = ChatTabContent::kLog;
    std::uint64_t read_through = 0;
    std::uint64_t cleared_through = 0;
    std::uint64_t last_seq = 0;
    std::size_t lines = 0;
    bool unread = false;
  };
  // Un mémo par rang d'onglet.
  std::vector<UnreadMemo> unread_memo_;
  // ⚠ À LA PREMIÈRE IMAGE DE LA PARTIE QUI A DES LIGNES, tout est lu : le
  // message d'accueil et ce qui s'est dit avant d'entrer ne marquent aucune
  // languette.
  bool opened_read_ = false;

  std::filesystem::path client_root_;
  ChatTabs tabs_;
  // La languette du compteur. ⚠ ICI ET NON SUR LA PILE : la bande n'en garde
  // qu'une vue, lue pendant le dessin.
  std::string meter_label_;
  const ro::vfs::ItemTable* items_ = nullptr;
  const ro::vfs::ItemWords* words_ = nullptr;
  const ro::vfs::MapDisplayNames* maps_ = nullptr;
  CreatorNames creators_;
  // LES LIENS MONTRÉS CETTE IMAGE-CI, dans l'ordre où les jetons les numérotent.
  // ⚠ TOUTES FENÊTRES CONFONDUES : un jeton désigne un lien, pas une place dans
  // une fenêtre, et deux chatbox détachées montrent parfois la même ligne.
  std::vector<ChatLinkTarget> shown_links_;
  // ⚠ UNE TABLE ORDONNÉE, comme les états de fenêtre : les vues sont rendues
  // par SPAN, et un vecteur qui grandit les déplacerait sous la main de qui les
  // tient.
  std::map<std::uint32_t, GroupViews> shown_;
  // Le fil du salon, et le salon qu'il montre : un salon neuf repart d'un cache
  // neuf, ses rangs de ligne recommençant.
  GroupViews room_;
  std::uint32_t room_seq_ = 0;
  // LES CONVERSATIONS 1:1, par correspondant. ⚠ UNE TABLE ORDONNÉE pour la
  // même raison que `shown_` : les vues sont rendues par span.
  std::map<std::string, GroupViews, std::less<>> whisper_views_;
  const WhisperDesk* desk_ = nullptr;
  std::string own_name_;
  std::function<ro::ui::PlayerLinkRefusals(std::string_view)> judge_;
  MvpLinkClock mvp_clock_;
  SettingReach setting_reach_;
  MvpLinkJudge mvp_judge_;
  ChatRichContext rich_;
  // LES ADRESSES DES VIGNETTES MONTRÉES CETTE IMAGE-CI, rangées par clé. ⚠
  // TOUTES FENÊTRES CONFONDUES, comme les liens.
  std::vector<std::string> shown_pictures_;
  // Les refus de l'image, par nom : les liens POINTENT dedans, d'où la table.
  std::map<std::string, ro::ui::PlayerLinkRefusals, std::less<>> refusals_;
  std::vector<std::uint32_t> living_;
  // Le compte de changements DÉJÀ enregistré. Voir `ChatTabs::revision`.
  std::uint32_t saved_revision_ = 0;
  // La seconde du jour qu'il était quand l'horloge de la boucle valait zéro.
  //
  // ⚠ RELEVÉE UNE FOIS, et pas à chaque ligne : demander l'heure locale
  // soixante fois par image pour afficher soixante horodatages coûterait plus
  // que tout le reste de la fenêtre. Ensuite, ce n'est que de l'arithmétique.
  int day_second_at_boot_ = -1;
  // Le rang de la dernière ligne vue au dernier tri. C'est ce que « vider »
  // retient : tout ce qui précède disparaît de cet onglet-là.
  std::uint64_t last_seq_ = 0;
  // Les lignes de la partie d'avant, et la date du fichier. Elles ne sont PAS
  // dans le monde : voir `chat_history_file.h`.
  PastChat past_;
  // COMBIEN DE FOIS ELLES ONT ÉTÉ RELUES OU OUBLIÉES. Leur identité de ligne
  // porte ce compte : sans lui, le rang d'une ligne relue désignerait pour le
  // cache du fil la ligne qu'il désignait avant.
  std::uint64_t past_epoch_ = 0;
  // Le repère qui les sépare de celles d'aujourd'hui, composé une fois.
  std::string past_mark_;
  // Ce qu'on a vu passer cette partie-ci, pour le réenregistrer. ⚠ LE MONDE NE
  // GARDE QUE SOIXANTE-QUATRE LIGNES : s'en remettre à lui au moment de fermer
  // ne sauverait que la fin de la soirée.
  std::vector<PastChatLine> seen_;
  // Jusqu'où la moisson est allée. Un RANG et non un compte : le tampon du
  // monde perd ses plus vieilles lignes, donc sa taille ne dit rien.
  std::uint64_t harvested_through_ = 0;
  // Le journal du jour, tant que l'option le veut. ⚠ IL S'ÉCRIT DEPUIS LA
  // MOISSON, donc fenêtre fermée aussi : un journal qui ne porte que ce qu'on
  // regardait ne serait pas un journal.
  ChatLogWriter log_;
  // 🔴 CE QUE LA CASE DEMANDE, et non ce que le journal est. Voir `KeepLog` :
  // les deux se confondaient, et un journal qui refuse de s'ouvrir se faisait
  // rouvrir à l'image suivante.
  bool log_wanted_ = false;
};

}  // namespace moonclient
