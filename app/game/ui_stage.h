#pragma once

// ⚠ CET EN-TÊTE NE TIRE PAS LES FENÊTRES, et il ne doit pas se remettre à le
// faire : plus d'une source sur trois de l'exécutable l'inclut. Ce que les
// fenêtres gardent (`Windows`), ce qu'elles rendent (`Answers`) et ce que les
// dessins reçoivent d'un tenant vivent dans `game/ui_windows.h`,
// `game/ui_answers.h` et `game/ui_stage_views.h` ; ici, ils ne sont que nommés.
// Un type qu'une signature reçoit par référence ou par `std::span` se déclare
// d'avance, et son en-tête s'inclut là où l'on s'en sert.

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/ui/escape_stack.h"
#include "ro/ui/hud_frame.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/stat_bars.h"
#include "ro/ui/ui_context.h"
#include "ro/ui/zone_capture.h"

#include "game/over_world_paint.h"
#include "game/ui_stage_fwd.h"
#include "ui_backend_gl.h"

struct ImFont;
struct SDL_Window;
union SDL_Event;

namespace ro::net {
class FrameTally;
}  // namespace ro::net

namespace ro::render {
class SkinTextures;
}  // namespace ro::render

namespace ro::ui {
struct AfkClockView;
struct AlignGrid;
struct BankWindowView;
struct CartTally;
struct CastBarView;
struct CastBarsLook;
struct CharacterCreateState;
struct CharacterDeleteState;
struct CharacterSelectState;
struct ChatBubbleLook;
struct ChatBubbleView;
struct ChatEmojiGroup;
struct ChatEmotion;
struct ChatInputLink;
struct ChatLinePreview;
struct ChatMacroCommandGroup;
struct ChatWindowState;
struct DamageTexts;
struct DescriptionView;
struct EntityBarView;
struct EntityBarsLook;
struct GameMenuFacts;
struct HotkeyWindowView;
struct InventoryCellView;
struct InventoryTally;
struct ItemLinkFacts;
struct ItemObtainAnchorGrip;
struct ItemObtainToastEntry;
struct ItemObtainToastLook;
struct LoadingScreenView;
struct LoginWindowAction;
struct LoginWindowState;
struct LoginWindowView;
struct MapSignView;
struct OptionSectionView;
struct OptionsWindowAction;
struct OptionsWindowState;
struct PaletteEditorView;
struct PatchScreenAnswers;
struct PatchScreenView;
struct PvpHudLook;
struct SessionRights;
struct ShortcutBarsLayout;
struct ShortcutSlotView;
struct SkillDescriptionView;
struct SkillGroupView;
struct SkillPointTally;
struct StatusBarLayout;
struct StatusCellView;
struct StorageTally;
struct StylePreviewView;
}  // namespace ro::ui

namespace moonclient {

class ChatTabsView;
struct AchievementWindows;
struct BattlegroundWindows;
struct ChatRoomWindows;
struct FairPlayBox;
struct MailWindows;
struct MvpTrackerWindows;
struct NavigationWindows;
struct PartyBookingWindows;
struct QuestWindows;
struct RankingBoardWindows;
struct SurvivorsAsk;
struct SurvivorsFrameView;
struct TrackerWindows;
struct TradeWindows;
struct WhisperWindows;

// L'INTERFACE DU JEU : son contexte, son dos, et la trame qu'ils composent.
//
// ⚠ C'est ici, et nulle part ailleurs, que `ui` rencontre le reste du jeu. La
// couche ne connaît ni `net` ni `sim` — le DAG l'interdit —, donc c'est cet
// étage qui RECOPIE ce qu'une fenêtre montre en données pures. C'est la même
// discipline que la `FrameScene` du renderer, et elle achète la même chose :
// une fenêtre se fabrique et se vérifie dans un test, sans serveur.
//
// ⚠ IL NE POSSÈDE AUCUN ÉTAT DE FENÊTRE. En mode immédiat, une fenêtre se
// redessine entière à chaque image : ce qui doit durer vit chez l'appelant,
// exactement comme l'interrupteur de l'inspecteur de trames. C'est aussi ce qui permet
// à une commande de chat de l'ouvrir sans rien connaître de cet étage.
class UiStage {
 public:
  // Voir `game/ui_stage_views.h`, `game/ui_windows.h` et `game/ui_answers.h`.
  using NpcTalk = UiNpcTalk;
  using CharacterSheetView = UiCharacterSheetView;
  using ChatWindowAsk = UiChatWindowAsk;
  using Windows = UiWindows;
  using Answers = UiAnswers;
  using LobbySeatsAnswers = UiLobbySeatsAnswers;
  using LobbySeatsFrame = UiLobbySeatsFrame;

  // Échoue si l'un des deux dos d'ImGui refuse de s'ouvrir. Le jeu se passe
  // alors d'interface plutôt que de ne pas démarrer.
  //
  // ⚠ `windows` EST RETENU D'UNE PARTIE À L'AUTRE — place, taille et ouverture.
  // Le contexte en garde l'ADRESSE pour les écrire dans ses réglages : il doit
  // donc survivre à cet étage, ce que la boucle assure en le déclarant AVANT.
  // `font_family` est le rang de la police choisie, moins de zéro pour celle
  // d'ImGui. Il vient des réglages, déjà lus. `loose_font_folder` est le
  // dossier où le joueur dépose ses propres polices — voir `FindLooseFonts`.
  // `settings_file` est le fichier où l'interface retient la place et
  // l'ouverture de ses fenêtres ; vide, elle s'en passe plutôt que d'écrire au
  // hasard.
  [[nodiscard]] static ro::core::Result<UiStage> Open(
      SDL_Window* window, Windows& windows, int font_family,
      const std::filesystem::path& loose_font_folder, const std::filesystem::path& settings_file);

  // Les polices du dossier du jeu que l'atlas a su lire, par leur nom.
  [[nodiscard]] std::vector<std::string> LooseFontNames() const { return ui_->LooseFontNames(); }

  // L'HABILLAGE, recopié en identifiants et tailles.
  //
  // ⚠ POSÉ APRÈS L'OUVERTURE, et il le faut : l'interface s'ouvre avec le
  // contexte graphique, les pièces se lisent plus tard avec le reste des
  // accessoires. Tant qu'il n'est pas posé, les fenêtres gardent leur aspect
  // ordinaire — ce qui est exactement ce qu'on veut d'une installation sans art.
  //
  // C'est ici, et nulle part ailleurs, que la texture devient un identifiant :
  // `ui` ne connaît pas OpenGL.
  void UseSkin(const ro::render::SkinTextures& textures);
  // L'habillage que le joueur règle : posé à chaque image, il change en jeu.
  void SetSkinLook(const ro::ui::SkinLook& look) { skin_.SetLook(look); }

  // L'échelle de l'interface. Voir `UiContext::SetInterfaceScale` : c'est un
  // réglage du joueur, retenu d'une partie à l'autre.
  void SetInterfaceScale(float scale) { ui_->SetInterfaceScale(scale); }
  [[nodiscard]] float interface_scale() const { return ui_->interface_scale(); }

  // LA DISPOSITION DES FENÊTRES, lue et posée à chaud. Voir
  // `UiContext::CurrentLayout` et `ui::WindowPlacer`.
  [[nodiscard]] ro::ui::LayoutProfile CurrentLayout() const { return ui_->CurrentLayout(); }
  [[nodiscard]] std::map<std::string, bool> LayoutToggles() const { return ui_->LayoutToggles(); }
  void UseLayout(const ro::ui::LayoutProfile& layout) { ui_->UseLayout(layout); }
  [[nodiscard]] std::uint64_t layout_revision() const { return ui_->layout_revision(); }

  // Change la police de toute l'interface, TOUT DE SUITE.
  //
  // ⚠ AUCUN ATLAS N'EST REBÂTI : toutes les familles y sont déjà, bakées à
  // l'ouverture, et la bascule ne coûte qu'un changement de pointeur. C'est
  // précisément pourquoi elles sont toutes chargées d'avance.
  // `family` est un rang de `SystemFontFamilies()` — celui du réglage — et non de
  // l'atlas : voir `UiContext::ChooseFont`.
  void ChooseFont(int family);

  // La police des dégâts écrits en police : celle de ce nom, ou celle de
  // l'interface quand le nom est vide ou inconnu.
  [[nodiscard]] ImFont* DamageFont(std::string_view name, bool bold) const;

  // Écrit les réglages s'ils ont bougé. À appeler une fois par image : sans
  // cela, une fenêtre déplacée puis le jeu fermé perdrait sa place.
  void SaveSettings() { ui_->SaveSettingsIfDirty(); }

  // Donne l'événement à l'interface. À appeler sur CHAQUE événement, avant toute
  // autre décision : ImGui ne sait ce qu'il y a sous le curseur que s'il l'a vu bouger.
  void TakeEvent(const SDL_Event& event) const;
  // Un relâchement que la veille a avalé, qui ne doit rien cliquer.
  void TakeSwallowedRelease(const SDL_Event& event) const;

  // Qui reçoit le geste. Se lit APRÈS `TakeEvent`, et porte le verdict de l'image
  // précédente : c'est ainsi que marche une interface en mode immédiat.
  [[nodiscard]] ro::ui::InputGate gate() const { return ui_->gate(); }

  // ÉCHAP A-T-IL UNE FENÊTRE À FERMER ? Tant que oui, la touche ne doit pas
  // quitter le jeu : elle ferme, une par une, la plus au-dessus d'abord.
  [[nodiscard]] bool escape_has_work() const { return escape_.has_work(); }

  // Ouvre la trame, dessine ce qui est ouvert, la ferme et la peint.
  //
  // ⚠ EN DERNIER DANS L'IMAGE : rien de ce que l'interface dessine ne doit être
  // masqué par le monde.
  //
  // `dialog` est le dialogue en cours, ou nul. Ce qu'il rend est ce que le
  // joueur a DEMANDÉ : la boucle le route vers le serveur, cet étage ne touche à
  // rien lui-même.
  //
  // UNE IMAGE D'INTERFACE DE L'ÉCRAN DE CONNEXION.
  //
  // ⚠ À LA PLACE DE `Draw`, jamais avec : les deux ouvrent et ferment la même
  // image d'ImGui, et un formulaire n'a rien à dire aux `Answers` du JEU.
  // `map_sign` : le bandeau de la ville du décor, ou nul. `settings` : les
  // réglages du login, sur `pages`, qui répondent dans `settings_asked`.
  [[nodiscard]] ro::ui::LoginWindowAction DrawLobby(
      ro::ui::LoginWindowState& login, const ro::ui::LoginWindowView& view,
      const ro::ui::MapSignView* map_sign, ro::ui::OptionsWindowState& settings,
      std::span<const ro::ui::OptionSectionView> pages,
      ro::ui::OptionsWindowAction* settings_asked);

  // UNE IMAGE D'INTERFACE DE LA SCÈNE DE SÉLECTION.
  //
  // ⚠ SA JUMELLE, ET JAMAIS EN MÊME TEMPS : les deux écrans du démarrage se
  // succèdent, et chacune de ces fonctions ouvre et ferme la même image
  // d'ImGui. Deux fonctions plutôt qu'un aiguillage à drapeau : ce que l'une
  // rend n'a rien à voir avec ce que rend l'autre, et les fondre demanderait
  // de rendre les deux réponses à chaque image.
  [[nodiscard]] LobbySeatsAnswers DrawCharacters(ro::ui::CharacterSelectState& seats,
                                                 ro::ui::CharacterCreateState& create,
                                                 ro::ui::CharacterDeleteState& remove,
                                                 const LobbySeatsFrame& frame);

  // UNE IMAGE OÙ L'INTERFACE NE SE DESSINE PAS : l'écran de veille.
  //
  // 🔴 ON SAUTE LE DESSIN, PAS L'ÉTAT, et c'est toute la différence avec le
  // « cacher l'interface » du client d'origine — lequel FERME les fenêtres pour
  // les rouvrir ensuite. Acceptable pour un geste volontaire, inacceptable pour
  // un basculement automatique : partir se faire un café ne doit pas ranger ce
  // qu'on avait sorti. Ici rien n'est touché ; l'image suivante redessine tout.
  //
  // ⚠ ELLE OUVRE ET FERME LA MÊME IMAGE D'IMGUI que ses deux jumelles, et
  // jamais en même temps qu'elles.
  void DrawAfkOnly(const ro::ui::AfkClockView& clock, const OverWorldPaint& over_world = {});

  // UNE IMAGE D'ÉCRAN DE CHARGEMENT, et rien d'autre : voir `ro/ui/loading_screen.h`.
  // Même règle que ses jumelles — jamais en même temps qu'elles.
  void DrawLoadingOnly(const ro::ui::LoadingScreenView& view);

  // UNE IMAGE D'ÉCRAN DE MISE À JOUR : voir `ro/ui/patch_screen.h`. Même règle
  // que ses jumelles.
  [[nodiscard]] ro::ui::PatchScreenAnswers DrawPatchOnly(const ro::ui::PatchScreenView& view);
  // UNE IMAGE DE PARTIE DE SURVIVORS : voir `game/ui_stage_survivors.h`.
  [[nodiscard]] SurvivorsAsk DrawSurvivorsOnly(const SurvivorsFrameView& view);

  // `skills` porte les onglets du grimoire, DÉJÀ composés : `ui` ne connaît ni
  // l'arbre du client ni le livre du serveur.
  [[nodiscard]] Answers Draw(
      const ro::net::FrameTally& tally, Windows& windows, const NpcTalk& talk,
      const ro::ui::ChatLinePreview& chat_preview, const ChatTabsView& chat_tabs,
      // LES LIENS DÉJÀ POSÉS DANS LA SAISIE, à peindre en pastilles. ⚠ SEULE LA
      // FENÊTRE PRINCIPALE EN A : elle seule porte une saisie.
      std::span<const ro::ui::ChatInputLink> chat_links,
      std::span<const ro::ui::ChatEmotion> emotions, std::span<const ro::ui::ChatEmojiGroup> emojis,
      std::span<const ro::ui::SkillGroupView> skills, ro::ui::SkillPointTally points,
      const CharacterSheetView& sheet,
      // CE QUE L'ÉDITEUR DE STYLE MONTRE : les pièces du corps porté, leurs
      // pastilles, et le pantin que `app` a composé.
      const ro::ui::PaletteEditorView& style,
      // ET CE QUE MONTRE L'APERÇU D'UN STYLE REÇU PAR LE FIL. ⚠ UNE AUTRE
      // FENÊTRE, sur un autre pantin : les deux peuvent être ouvertes en même
      // temps sur deux recettes différentes.
      const ro::ui::StylePreviewView& style_preview, std::span<const ro::ui::InventoryCellView> bag,
      ro::ui::InventoryTally bag_tally, std::span<const ro::ui::InventoryCellView> cart,
      ro::ui::CartTally cart_tally, std::span<const ro::ui::InventoryCellView> storage,
      ro::ui::StorageTally storage_tally,
      // LES DEUX SOLDES DE LA BANQUE, déjà recopiés du monde.
      const ro::ui::BankWindowView& bank, const ro::ui::HotkeyWindowView& hotkeys,
      std::span<const ro::ui::ShortcutSlotView> shortcuts,
      const ro::ui::ShortcutBarsLayout& shortcut_bars,
      // LES ÉTATS DU JOUEUR, DÉJÀ CROISÉS : `ui` ne connaît ni les EFST, ni les
      // tables de l'installation, ni les textures.
      std::span<const ro::ui::StatusCellView> statuses, const ro::ui::StatusBarLayout& status_bar,
      // LES SIX JAUGES LIBRES. ⚠ `look` N'EST PAS CONST : le joueur les
      // déplace à la souris, et c'est le dessin qui écrit leur géométrie.
      // `Answers::stat_bars_moved` dit quand il faut persister.
      const ro::ui::StatBarValues& stat_bar_values,
      const ro::ui::StatBarWarnings& stat_bar_warnings, ro::ui::StatBarsLook& stat_bars,
      // LES DEUX CADRES DU CLASSEMENT PVP. ⚠ PAS CONST NON PLUS, et pour la même
      // raison que les jauges : le joueur les pose à la souris.
      ro::ui::PvpHudLook& pvp_hud, std::span<const ro::ui::OptionSectionView> options,
      const ro::ui::DescriptionView& described, const ro::ui::DescriptionView& previewed,
      const ro::ui::SkillDescriptionView& skill_described, const ro::ui::AlignGrid& grid,
      std::span<const ro::ui::ChatBubbleView> bubbles, const ro::ui::ChatBubbleLook& bubble_look,
      // LES BARRES DE VIE ET DE MAGIE DES ENTITÉS, déjà projetées en pixels par
      // `app` : `ui` ne connaît ni caméra ni monde. Le FILTRE — l'espèce, le
      // survol, la cible, le seuil d'entame — se joue au dessin, contre le
      // réglage, et non à la récolte.
      std::span<const ro::ui::EntityBarView> entity_bars,
      const ro::ui::EntityBarsLook& entity_bar_look,
      // LES BARRES D'INCANTATION, projetées de même. Le filtre se joue au dessin.
      std::span<const ro::ui::CastBarView> cast_bars, const ro::ui::CastBarsLook& cast_bar_look,
      // LES NOMBRES DE DÉGÂTS QUE LE RÉGLAGE ÉCRIT EN POLICE, projetés de même,
      // et la police qu'il leur choisit.
      const ro::ui::DamageTexts& damage_texts,
      // CE QUE LE SERVEUR AUTORISE. ⚠ Relu à chaque image, jamais mémorisé : un
      // droit se perd en cours de session comme il s'accorde.
      ro::ui::SessionRights rights,
      // CE QUE LE MENU D'ÉCHAP PROPOSE : la disposition dépend de l'état du
      // personnage, et se relit à chaque image — on peut mourir menu ouvert.
      const ro::ui::GameMenuFacts& menu_facts,
      // Ce que le menu du clic droit des macros propose, DÉJÀ TRADUIT : `ui` ne
      // connaît aucune commande de serveur.
      std::span<const ro::ui::ChatMacroCommandGroup> macro_commands,
      // LE BANDEAU D'OBTENTION, déjà composé : `ui` ne connaît ni sac, ni table
      // d'objets. ⚠ APRÈS LES FENÊTRES DANS LE DESSIN — une notification ne se
      // range pas derrière celle qu'on vient d'ouvrir — et sa poignée n'est pas
      // const : le joueur la déplace, et `Answers::obtained` dit quand
      // persister.
      std::span<const ro::ui::ItemObtainToastEntry> obtained,
      const ro::ui::ItemObtainToastLook& obtained_look, ro::ui::ItemObtainAnchorGrip& obtained_grip,
      // CE QUE L'ENREGISTREUR DE ZONE MONTRE : le tracé, le décompte, le témoin.
      // ⚠ TOUT EN DERNIER DANS LE DESSIN, et par-dessus le reste : le tracé
      // recouvre l'écran, et le témoin doit rester visible au-dessus des
      // fenêtres qu'on est justement en train de filmer.
      const ro::ui::ZoneCaptureView& capture, std::int32_t round_trip_ms,
      const OverWorldPaint& over_world = {});

 private:
  // Glisse le rappel dans la liste de fond, puis rend à l'interface l'état
  // qu'il a changé.
  static void PaintOverWorld(const OverWorldPaint& over_world);
  // LES COMMERCES : leur dessin, ce que le menu d'un item sait d'avance, et
  // leur place dans la pile d'Échap. Voir `ui_stage_trade.cpp`.
  void DrawTradeWindows(TradeWindows& trade, ro::ui::ItemLinkFacts link_facts);
  void RegisterTradeEscapes(TradeWindows& trade);
  // LE COURRIER, de même : voir `ui_stage_mail.cpp`.
  void DrawMailWindows(MailWindows& mail, ro::ui::ItemLinkFacts link_facts);
  void RegisterMailEscapes(MailWindows& mail);
  // LES SALONS, de même : voir `ui_stage_chat_room.cpp`. La salle porte la barre
  // de la conversation : ce qu'elle rend s'ajoute à celui des conversations.
  void DrawChatRoomWindows(ChatRoomWindows& chat_room, ro::ui::ChatWindowState& chat,
                           const ro::ui::ChatLinePreview& preview, const ChatTabsView& tabs,
                           std::span<const ro::ui::ChatInputLink> links,
                           ro::ui::ItemLinkFacts link_facts, Answers& answers);
  void RegisterChatRoomEscapes(ChatRoomWindows& chat_room);
  void DrawWhisperWindows(WhisperWindows& whispers, const ChatTabsView& tabs,
                          std::span<const ro::ui::ChatEmotion> emotions,
                          std::span<const ro::ui::ChatEmojiGroup> emojis,
                          ro::ui::ItemLinkFacts link_facts, Answers& answers);
  void RegisterWhisperEscapes(WhisperWindows& whispers);  // 1:1 : `ui_stage_whisper.cpp`
  // LA CONVERSATION, ses salons et ses 1:1 : voir `ui_stage_chat.cpp`.
  void DrawConversation(Windows& windows, const ro::ui::ChatLinePreview& preview,
                        const ChatTabsView& tabs, std::span<const ro::ui::ChatInputLink> links,
                        std::span<const ro::ui::ChatEmotion> emotions,
                        std::span<const ro::ui::ChatEmojiGroup> emojis, Answers& answers);
  void DrawAchievementWindows(AchievementWindows& achievements);  // `ui_stage_achievements.cpp`
  void RegisterAchievementEscapes(AchievementWindows& achievements);
  void DrawQuestWindows(QuestWindows& quests, TrackerWindows& tracker);  // `ui_stage_quests.cpp`
  void RegisterQuestEscapes(QuestWindows& quests);
  void DrawMvpTrackerWindows(MvpTrackerWindows& mvp);  // `ui_stage_mvp_tracker.cpp`
  void RegisterMvpTrackerEscapes(MvpTrackerWindows& mvp);
  void DrawBattlegroundWindows(BattlegroundWindows& battleground);  // `ui_stage_battleground.cpp`
  void RegisterBattlegroundEscapes(BattlegroundWindows& battleground);
  void DrawRankingWindows(RankingBoardWindows& ranking);  // `ui_stage_ranking.cpp`
  void DrawFairPlayNotice(FairPlayBox& box);              // `ui_stage_fair_play.cpp`
  void RegisterRankingEscapes(RankingBoardWindows& ranking);
  void DrawPartyBookingWindows(PartyBookingWindows& booking);  // `ui_stage_party_booking.cpp`
  void RegisterPartyBookingEscapes(PartyBookingWindows& booking);
  // LA NAVIGATION ET SES DEUX INFOBULLES : voir `ui_stage_navigation.cpp`.
  void DrawNavigationWindows(NavigationWindows& navigation);
  void RegisterNavigationEscapes(NavigationWindows& navigation);
  UiStage(std::unique_ptr<ro::ui::UiContext> ui, ro::ui::UiBackendGl backend);

  // 🔴 UN SEUL ÉTAT DE GESTE POUR TOUS LES CADRES DE HUD, parce qu'il n'y a
  // qu'une souris. Un par surface autoriserait « deux cadres se croient
  // attrapés », que rien ne signalerait.
  ro::ui::HudGrip hud_grip_;

  // ⚠ PAR POINTEUR, parce que `UiContext` ne se déplace pas : il se désigne
  // lui-même auprès d'ImGui. Cet étage, lui, doit rester déplaçable — `Open`
  // le rend par valeur —, et c'est le pointeur qui bouge alors, pas l'objet.
  std::unique_ptr<ro::ui::UiContext> ui_;
  // ⚠ APRÈS le contexte, et c'est une contrainte de destruction : ImGui range
  // l'état du dos DANS le contexte, si bien que détruire celui-ci d'abord
  // ferait écrire la fermeture du dos dans de la mémoire libérée. L'ordre des
  // membres est l'ordre de construction, et l'inverse à la destruction.
  ro::ui::UiBackendGl backend_;
  ro::ui::SkinArt skin_;
  ro::ui::EscapeStack escape_;
  // Le rectangle de capture en cours de tracé. Voir `ro::ui::ZoneTraceState`.
  ro::ui::ZoneTraceState zone_trace_;
};

}  // namespace moonclient
