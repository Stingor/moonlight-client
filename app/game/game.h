#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/frame_tally.h"
#include "ro/net/moonclient_handshake.h"
#include "ro/net/session.h"
#include "ro/net/zone_move.h"
#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/gl_context.h"
#include "ro/render/screen_tint.h"
#include "ro/render/sky_clock.h"
#include "ro/render/sun_shadow_pass.h"
#include "ro/sim/character_ledger.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/pickup_intent.h"
#include "ro/sim/player_config.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/ui/staff_tools.h"
#include "ro/vfs/vfs.h"

#include "afk/afk_screen.h"
#include "capture/zone_recorder.h"
#include "character/character_view.h"
#include "character/preset_view.h"
#include "character/shortcut_view.h"
#include "character/skill_book_view.h"
#include "character/skill_description_view.h"
#include "character/skill_facts_asker.h"
#include "character/skill_view.h"
#include "character/stat_preset_desk.h"
#include "character/status_view.h"
#include "character/style_preview.h"
#include "character/style_view.h"
#include "character/title_view.h"
#include "chat/chat_command_catalogue.h"
#include "chat/chat_line_preview.h"
#include "chat/chat_navi_search_link.h"
#include "chat/chat_tab_view.h"
#include "chat/chat_view.h"
#include "effects/effect_bench_view.h"
#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "game/client_shell.h"
#include "game/database_stage.h"
#include "game/game_foundation.h"
#include "game/game_pieces.h"
#include "game/game_rendering.h"
#include "game/game_settings.h"
#include "game/game_ui_buffers.h"
#include "game/item_gesture.h"
#include "game/ui_stage.h"
#include "guild/guild_emblems.h"
#include "guild/guild_panel.h"
#include "input/camera_aim.h"
#include "input/camera_rig.h"
#include "items/bank_view.h"
#include "items/card_album_view.h"
#include "items/card_insert_view.h"
#include "items/cart_view.h"
#include "items/craft_atlas_view.h"
#include "items/description_view.h"
#include "items/inventory_view.h"
#include "items/storage_view.h"
#include "items/view_equip_view.h"
#include "party/party_panel.h"
#include "world/actor_footprints.h"
#include "world/actor_ghosts.h"
#include "world/actor_sounds.h"
#include "world/actor_styles.h"
#include "world/actor_talk.h"
#include "world/actor_view.h"
#include "world/click_action.h"
#include "world/closed_maps.h"
#include "world/entity_menu.h"
#include "world/graffiti_view.h"
#include "world/ground_unit_view.h"
#include "world/item_drops.h"
#include "world/map_edit_store.h"
#include "world/map_editor_mesh.h"
#include "world/map_editor_session.h"
#include "world/map_music.h"
#include "world/map_stage.h"
#include "world/model_previews.h"
#include "world/monster_sheet.h"
#include "world/name_card_asker.h"
#include "world/native_marker.h"
#include "world/npc_talk_view.h"
#include "world/replace_tally.h"
#include "world/skill_area_view.h"

// ⚠ DÉCLARÉ PLUTÔT QU'INCLUS, comme derrière `ro::render::GlContext` : cet
// en-tête est tiré partout, et SDL n'y sert qu'au compteur d'inactivité.
union SDL_Event;

namespace moonclient {

// LA PARTIE : tout ce que le client tient ouvert, et l'image qu'il en fait.
//
// ⚠ C'EST UNE RACINE DE COMPOSITION, ET C'EST POURQUOI ELLE EST LONGUE. Une
// quarantaine de sous-systèmes s'y déclarent dans un ordre qui compte, et les
// découper « pour la taille » ne ferait que les disperser : chaque morceau
// sorti devrait recevoir douze pointeurs pour retrouver ce qu'il avait sous la
// main. Le seul découpage qui paie est celui des ÉTAPES d'une image, et ce sont
// les méthodes ci-dessous.
//
// ⚠ L'ORDRE DES MEMBRES EST L'ORDRE DE DESTRUCTION, À L'ENVERS, et le VFS vit
// avant tout ce qui en garde une référence — effets, musique, bruits, unités au
// sol, icônes de compétence. ⚠ IL APPARTIENT MAINTENANT À LA COQUILLE, donc il
// survit à la partie : la contrainte est tenue par construction, et c'est
// `ClientShell` qui la porte désormais.
//
// ⚠ ELLE NE SE DÉPLACE NI NE SE COPIE : elle garde l'adresse de ce que la
// coquille contient, et se construit là où elle vivra.
//
// ⚠ TROIS BASES PORTENT UNE PART DE SES CHAMPS, ET C'EST ENCORE L'ORDRE : une
// base se construit avant tout membre et se détruit après. Le socle
// (`GameFoundation`) vient le premier et part le dernier ; les tampons que
// l'interface désigne (`GameUiBuffers`) et les renderers (`GameRendering`)
// survivent à tout ce qui les vise. Leurs champs gardent leur nom — `world_`,
// `sprites_` — et la partie les lit comme les siens.
class Game : public Screen, private GameFoundation, private GameUiBuffers, private GameRendering {
 public:
  // ⚠ LA COQUILLE DOIT ÊTRE OUVERTE : la partie prend l'adresse de ce qu'elle
  // contient, et un `ClientShell` dont `Open` n'a pas réussi n'a ni fenêtre, ni
  // archives, ni réglages à donner.
  Game(ClientShell& shell, ro::net::Connection link, ro::sim::World world,
       const ro::net::ZoneTicket& ticket);

  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;
  Game(Game&&) = delete;
  Game& operator=(Game&&) = delete;
  ~Game();

  // Ouvre ce qu'une PARTIE ajoute à la coquille : la carte, le décor, les
  // accessoires du monde, les vues.
  //
  // ⚠ EN DEUX TEMPS, ET NON DANS LE CONSTRUCTEUR : chaque étape peut échouer
  // pour une raison que le joueur doit lire, et un constructeur n'a que
  // l'exception pour le dire — ce que ce dépôt n'emploie pas.
  [[nodiscard]] ro::core::Result<void> Open(const std::string& map_name);

  // UNE IMAGE DE PARTIE : les événements, le réseau, le monde, l'interface.
  //
  // ⚠ ELLE NE PRÉSENTE PAS, et ne plafonne pas la cadence : la coquille possède
  // la boucle. C'est ce qui permettra à l'écran de connexion de partager
  // exactement la même — deux boucles, c'est deux rythmes et deux façons de quitter.
  [[nodiscard]] ro::core::Result<void> Frame(std::uint32_t now_ms) override;

  // Vrai tant que le joueur n'a ni fermé la fenêtre ni quitté par le menu.
  [[nodiscard]] bool running() const noexcept override { return running_; }

  // LA PARTIE S'EST ARRÊTÉE POUR REVENIR À LA SÉLECTION DE PERSONNAGE : le
  // serveur l'a accordé. Faux quand le joueur a quitté.
  [[nodiscard]] bool back_to_characters() const noexcept { return back_to_characters_; }
  // LA PARTIE S'EST ARRÊTÉE SUR UNE LIAISON PERDUE dont le joueur a pris acte :
  // ce que l'écran de connexion doit lui dire. Vide sinon.
  [[nodiscard]] std::string link_lost_notice() const { return link_loss_.LoginNotice(); }

  // Écrit ce qui attendait : la conversation de cette partie.
  //
  // ⚠ ICI ET NULLE PART AILLEURS : l'écrire à chaque ligne reçue coûterait un
  // fichier par phrase.
  void Close() override;

 private:
  // ── LES ÉTAPES D'UNE IMAGE, dans leur ordre ────────────────────────────────
  // Chacune voit l'état de la partie sans qu'on ait à le lui passer : c'est tout
  // ce que la classe achète, et c'est ce qui manquait quand la boucle tenait
  // dans une fonction avec quarante variables locales.
  void PumpEvents(std::uint32_t now_ms);
  [[nodiscard]] ro::core::Result<void> PumpNetwork(std::uint32_t now_ms);
  // L'HABIT DU GM : sur notre personnage, et dit au serveur. Voir
  // `GmOutfitAnnouncer`.
  void FollowGmOutfit();
  // CE QUI VIT HORS DU MONDE LIT LA TRAME APRÈS LUI, et rend l'issue retenue :
  // voir `game_frame_beyond_world.cpp`.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrameBeyondWorld(const ro::net::Frame& received,
                                                            ro::net::FrameOutcome outcome,
                                                            std::uint32_t now_ms);
  // LE PASSAGE D'UN SERVEUR DE ZONE À UN AUTRE : l'ancienne liaison se ferme,
  // la nouvelle s'ouvre sans bloquer, et prend sa place une fois acceptée.
  void StartZoneMove(const ro::net::CharacterPhase::Handoff& handoff, std::uint32_t now_ms);
  void AdvanceZoneMove(std::uint32_t now_ms);
  // LA LIAISON PERDUE : voir `game_link_loss.cpp`.
  void DeclareLinkLoss(std::string_view cause, bool say_in_chat, std::uint32_t now_ms);
  void FollowLinkLoss();
  void WatchLinkSilence(bool received, std::uint32_t now_ms);
  [[nodiscard]] ro::net::MoonclientHandshake FreshHandshake() const;  // pour un serveur neuf
  // 🔴 TOUT CE QUI SE VOYAIT S'ÉTEINT AU CHANGEMENT DE CARTE, MÊME SI LA CARTE
  // EST LA MÊME. `@refresh` et `@load` passent par le paquet du voyage, et le
  // client natif y détruit son monde entier — effets, nombres, rémanences —
  // avant de le rebâtir de ce que le serveur réannonce. Un effet infini qui
  // survivrait ici jouerait deux fois dès que le serveur le redit.
  void ForgetWhatPlayed();

  // LE MODE CIBLAGE, à chaque image : les gestes de l'image précédente, la
  // cible revalidée, les redemandes au serveur, la vue du HUD. Voir
  // `game_targeting.cpp`.
  void AdvanceTargeting(std::uint32_t now_ms);
  // LE PORTRAIT DE STATUT, à chaque image, avec le HUD de cible : la géométrie
  // déplacée à l'image précédente, puis la vue recomposée.
  void AdvanceStatusPortrait(std::uint32_t now_ms);
  // Les monstres qu'un cyclage peut prendre, relevés dans la scène composée.
  void NoteScreenTargets();
  // LA FLÈCHE DU JEU, à chaque image : ce que l'action du personnage et la
  // souris font de l'engagement, la décision qui en découle — que le mode
  // Ciblage détourne —, en tête d'image ; puis le dessin. Voir
  // `world/native_marker.h`.
  void DecideSelectionMarker(std::uint32_t now_ms);
  void DrawSelectionMarker(std::uint32_t now_ms);
  // Un nom cliqué dans la fenêtre Groupe / Amis : voir `world/name_click.h`.
  void ClickName(std::uint32_t account_id, std::uint32_t now_ms);
  void PlaceSelectionMarker(MarkerCall call, std::uint32_t now_ms);
  [[nodiscard]] EngagementFrame UnderMouse() const;
  // Un ordre d'attaque explicite — double-clic, cadre du HUD — sur `target`.
  void EngageTargetAttack(const ro::sim::Actor& target, bool continuous, std::uint32_t now_ms);
  void AdvanceQuickCast(std::uint32_t now_ms);  // voir `game_quick_cast.cpp`

  // ── LE MENU DU CLIC DROIT SUR UNE ENTITÉ, dans `game_entity_menu.cpp` ─────
  //
  // À chaque image, APRÈS le mode Ciblage : ce que le menu a rendu à l'image
  // précédente se joue, puis les clics droits du monde et du HUD l'ouvrent.
  void AdvanceEntityMenu(std::uint32_t now_ms);
  void OpenEntityMenu(const ro::sim::Actor& actor, EntityFamily family, std::uint32_t now_ms);
  void PlayEntityVerb(EntityVerb verb, std::uint32_t id, int points, std::uint32_t now_ms);
  void ToggleNpcBlock(std::uint32_t id, const std::string& name, std::uint32_t now_ms);
  // LE MENU D'UN MODÈLE DE LA CARTE, Ctrl+Alt+clic droit : game_map_editor.cpp.
  void OpenMapModelMenu(std::size_t model_index, std::uint32_t now_ms);
  void PlayMapModelVerb(int verb);
  // « VOIR L'ÉQUIPEMENT », après le menu qui la demande : `game_view_equip.cpp`.
  void AdvanceViewEquip(std::uint32_t now_ms);
  // LA FICHE D'UN MONSTRE, même aller-retour, dans `game_monster_sheet.cpp`.
  void AdvanceMonsterSheet(std::uint32_t now_ms);
  // LA BASE DE DONNÉES, qui passe juste avant elle et peut l'héberger :
  // `game_database.cpp`. `HostMonsterSheet` rend vrai quand elle l'héberge.
  void AdvanceDatabase(std::uint32_t now_ms);
  [[nodiscard]] bool HostMonsterSheet(MonsterSheet& sheet);
  // Le lien d'un monstre de la base : son rang et son nom, quand elle le sait.
  [[nodiscard]] MonsterLink DatabaseMonsterLink(std::uint32_t monster_id) const;
  [[nodiscard]] DatabaseContext DatabaseContextNow(std::uint32_t now_ms);
  // CE QUE LE MENU D'UN OBJET SAIT D'AVANCE — vote shop, album —, le même pour
  // toutes les surfaces ; puis « Voir dans l'album », d'où que vienne le geste.
  [[nodiscard]] ro::ui::ItemLinkFacts ItemLinkFactsNow();
  void ShowCardInAlbum(std::uint32_t card_id, std::uint32_t now_ms);
  // LA FICHE D'UN OBJET, dans ses deux hôtes : `game_item_sheet.cpp`. Ce que la
  // fiche a demandé se route de même d'où qu'elle vienne ; `sheet` est la vue
  // qui l'a dessinée, `item_id` l'objet qu'elle montrait.
  void RouteItemSheet(const ro::ui::DescriptionWindowAction& asked, std::uint32_t item_id,
                      const DescriptionView& sheet, ro::ui::ItemSheetHost host,
                      std::uint32_t now_ms);
  // Ce que la boutique, l'Atlas et le staff ajoutent à une fiche, APRÈS sa
  // composition, qui l'efface.
  void DecorateItemSheet(DescriptionView& sheet, std::uint32_t item_id);
  // L'effet de chapeau du pantin survolé : boutique, description ou base.
  void AdvanceHoveredDollEffect(std::uint32_t now_ms);
  // « MONTRER MON ÉQUIPEMENT » : la ligne dans la conversation, à chaque annonce.
  void SayEquipmentShown(std::uint32_t now_ms);
  // L'APPEL D'URGENCE, de même, sous son propre genre de ligne.
  void SayCallable(std::uint32_t now_ms);
  // Le monde, en trois dimensions puis en coordonnées d'écran. Rend ce que la
  // souris désigne : la forme du curseur en dépend.
  [[nodiscard]] ro::core::Result<void> DrawWorld(std::uint32_t now_ms);
  // L'OMBRE DU SOLEIL est-elle de l'image : la page « Graphics » la demande, la
  // carte est en plein air — voir `SunShadowsApply` —, et sa carte a pu naître.
  [[nodiscard]] bool SunShadowsAsked(const ro::render::TerrainStages& stages) const;
  // LES ACTEURS DE L'IMAGE, composés dans `scene_actors_` AVANT toute peinture :
  // leur silhouette entre dans l'ombre du soleil, qui précède le sol. Sous le
  // soleil, ils prennent la lumière posée sans l'ombre cuite, et perdent leur
  // ombre ronde. Rien sans garde-robe.
  void ComposeActors(std::uint32_t now_ms, const ro::ui::PickTuning& pick_tuning, bool sun_shadows);
  // L'OMBRE DU SOLEIL de l'image, peinte quand `asked` : le sol, le décor, les
  // acteurs composés, `companions` — charrettes et faucons — et les acteurs en
  // modèle. Sans elle, une vue vide, qui n'ombre rien.
  [[nodiscard]] ro::render::SunShadowView DrawSunShadowIfAsked(
      bool asked, bool models_shown, std::span<const ro::render::DrawnActor> companions,
      const ro::render::SkyLight& sky);
  // LES OMBRES DES LUMIÈRES PONCTUELLES de l'image, peintes par les mêmes porteurs
  // que celle du soleil : `points` avec leurs cartes, ou sans ombre quand le
  // réglage les coupe ou qu'aucune lumière n'éclaire.
  [[nodiscard]] ro::render::PointLightSet DrawPointShadowsIfAsked(
      const ro::render::PointLightSet& points, bool models_shown,
      std::span<const ro::render::DrawnActor> companions);
  // CE QUI PORTE OMBRE dans l'image : le sol, le décor quand il se montre, les
  // acteurs composés, `companions` et les acteurs en modèle.
  [[nodiscard]] ro::render::SunShadowCasters ShadowCastersOfFrame(
      bool models_shown, std::span<const ro::render::DrawnActor> companions);
  // LE CIEL DE L'IMAGE : la lumière de la carte, teintée par la nuit du
  // serveur, et placée à l'heure que l'établi impose quand il en impose une —
  // voir `StaffSkyClock` —, sinon à l'heure du serveur quand le joueur veut le
  // cycle jour/nuit. Fait avancer l'heure de l'établi quand elle tourne.
  [[nodiscard]] ro::render::SkyLight SkyOfFrame(std::uint32_t now_ms);
  // La carte est-elle fermée au ciel — intérieur, ou citée par closed_maps.yml ?
  [[nodiscard]] bool MapClosedToSky() const;
  // Assombrit le vide sous le monde de ce que l'heure retire, puis y peint
  // l'astre à sa vraie place : voir `VoidShade`. Rien sans ciel à l'heure.
  void ShadeVoid(const ro::render::SkyLight& sky);
  // Lève le ciel animé de la carte une fois le joueur là, puis le retire ou le rend selon
  // GreyWorld. Sans effet sur les quatre cartes sur cinq qui n'en ont pas.
  void RaiseMapSky(std::uint32_t now_ms);
  // Les émetteurs et les anneaux de vent de la carte : les monter à la première
  // image, les faire vivre, les peindre.
  void AdvanceMapEffects(std::uint32_t now_ms);
  void DrawMapEffects();
  // Le relief suit GreyWorld, relu sur place : voir `ReloadStageRelief`. Puis
  // le sol s'arrondit ou non selon le réglage : voir `SyncStageGround`.
  [[nodiscard]] ro::core::Result<void> FollowMapRelief();
  [[nodiscard]] ro::core::Result<void> FollowGreyWorldRelief();
  // LE MODE ÉDITEUR DE CARTE : voir game_map_editor.cpp.
  void ServeMapEditor();
  // Tout l'éditeur sauf les lumières, et ce que le serveur en apprend ou en sert (ADR 0124).
  void ServeMapEditorTools();
  void ReplaceMapModels();
  void ServeVertexTool(const ro::render::MapEffectFrame& map_frame, float width, float height);
  [[nodiscard]] bool PublishMapEdit(const MapEditorSession::Outcome& outcome);
  void FollowServedMapEdit(const MapEditSync::Report& report);
  // L'outil Relief, ses points, et le sol qui suit le document : voir game_map_ground.cpp.
  void ServeGroundBrush();
  void FollowMapGround();
  void ServeGroundPoints(float width, float height);
  void DrawInterface(std::uint32_t now_ms);

  // ── L'INTERFACE, PAR FAMILLE DE FENÊTRES ──────────────────────────────────
  //
  // Ce que `DrawInterface` compose avant le dessin et route après, rangé par
  // famille dans son fichier ; ce qui reste dans la boucle est court.
  // ⚠ L'ORDRE DES APPELS COMPTE : le survol est relevé avant le sertissage et
  // la conversation, qui le complètent, et les onglets de la conversation se
  // réordonnent en dernier.
  //
  // La conversation, dans `game_chat.cpp`.
  void StageChat(std::uint32_t now_ms);
  void SendTypedChat(const UiStage::Answers& answers, std::uint32_t now_ms);
  void ApplyChatAnswers(const UiStage::Answers& answers, std::uint32_t now_ms);
  // Les commandes du joueur que le routage a confiées : `game_player_commands.cpp`.
  void RunPlayerAsks(std::uint32_t now_ms);
  // Le sac, la charrette, le coffre, la banque et l'album : `game_containers.cpp`.
  void StageContainers();
  void ApplyContainerAnswers(const UiStage::Answers& answers, std::uint32_t now_ms);
  void ApplyContainerSwitches(const UiStage::Answers& answers, std::uint32_t now_ms);
  void ApplyBankAnswers(const UiStage::Answers& answers, std::uint32_t now_ms);
  // Les bulles d'annonce : ce que le monde a déposé, puis ce que leurs bandeaux
  // peindront, et la place d'une poignée tirée : `game_balloons.cpp`.
  void StageBalloons(std::uint32_t now_ms);
  // La fiche, ses presets, l'éditeur de style et son aperçu : `game_sheet.cpp`.
  void StageCharacterSheet(std::uint32_t now_ms);
  void StageStyle(std::uint32_t now_ms);
  void ApplyStyleAnswers(const UiStage::Answers& answers, std::uint32_t now_ms);
  // Les presets, puis le GIF du pantin : ce que la fiche a demandé après le dessin.
  void ApplySheetAnswers(const UiStage::Answers& answers);
  // Les descriptions et le survol, dans `game_description.cpp`.
  void StageDescriptions(std::uint32_t now_ms);
  void ApplyDescriptionAnswers(const UiStage::Answers& answers, std::uint32_t now_ms);
  // Le lien que Maj + clic pose dans la saisie, dans `game_chat_links.cpp`.
  void PostClickedItemLink(const UiStage::Answers& answers);
  void AdvanceCraftAtlas(std::uint32_t now_ms);  // l'Atlas : `game_craft_atlas.cpp`
  void AdvanceNavigation(std::uint32_t now_ms);  // minimap, navigation : `game_navigation.cpp`
  void AdvanceMvpTracker(std::uint32_t now_ms);  // le carnet MVP : `game_mvp_tracker.cpp`
  void FollowCraftGestures(const CraftGestures& asked, std::uint32_t now_ms);  // `game_craft.cpp`
  void AdvanceQuests(std::uint32_t now_ms);           // le journal et le suivi : `game_quests.cpp`
  void AdvanceMemorialDungeon(std::uint32_t now_ms);  // l'instance : `game_memorial.cpp`
  // UN OBJET TIRÉ D'UN CONTENANT ET LÂCHÉ DANS UN AUTRE.
  //
  // 🔴 LES SIX SENS PASSENT PAR ICI, et c'est ce qui les garde d'accord : une
  // fenêtre voit le lâcher, elle ne sait pas quel paquet il appelle. Le couple
  // (d'où, vers où) décide, et chaque cas arme LA MÊME question de quantité que
  // le menu contextuel du contenant d'origine — sinon deux chemins mèneraient au
  // même transfert avec deux comportements.
  //
  // ⚠ UNE SEULE PAR IMAGE : le joueur n'a qu'une souris (`ReceivedTransfer`).
  void ApplyItemTransfer(const ro::ui::ItemTransferAsk& asked);

  // LE SERTISSAGE : la fenêtre et le sous-menu rapide, composés avant le
  // dessin ; puis ce qu'ils ont demandé, l'accusé à écrire, et la pièce
  // survolée. ⚠ APRÈS LE RELEVÉ DU SURVOL : il le complète, il ne le remplace
  // pas. Voir `items/card_insert_view.h`.
  void StageCardInsert();
  void ApplyCardInsert(const UiStage::Answers& answers, std::uint32_t now_ms);
  // Une carte glissée sur une pièce : voir `items/card_drop.h`.
  void ApplyCardDrop(const UiStage::Answers& answers, std::uint32_t now_ms);

  // SUIVRE UN LIEN D'UNE DESCRIPTION.
  //
  // 🔴 C'EST ICI ET NON DANS `ui`, et c'est la raison d'être de l'aller-retour :
  // une adresse ouvre un PROGRAMME DU SYSTÈME, à partir d'une donnée du jeu. Un
  // geste vers le dehors ne se prend pas au fond d'une couche de dessin, et il
  // ne s'accorde qu'aux schémas qu'on reconnaît.
  void FollowDescriptionLink(const ro::ui::DescriptionWindowAction& asked, std::uint32_t now_ms);
  // ⚠ LA MÊME CHOSE, DEPUIS L'AUTRE FENÊTRE : les deux portent un lien suivi, et
  // ce qu'on en fait ne dépend pas de laquelle il vient.
  void FollowSkillDescriptionLink(const ro::ui::SkillDescriptionAction& asked,
                                  std::uint32_t now_ms);

  // CE QU'UNE OPTION VA TOUCHER, rassemblé à un seul endroit. Deux appelants
  // en ont besoin — le routage du chat et l'étape d'interface — et deux
  // constructions à la main finiraient par ne plus porter les mêmes cibles.
  [[nodiscard]] OptionTargets OptionsTargets();

  // CE QUE LA SESSION AUTORISE, recalculé à chaque image et jamais mémorisé :
  // le niveau de groupe arrive au login et peut changer en cours de session.
  //
  // 🔴 UN SEUL ENDROIT LE CALCULE, PARCE QUE DEUX CHOSES EN DÉPENDENT : la
  // fenêtre qui commande le sol uni, et le sol uni lui-même. Le réglage, lui,
  // SURVIT au lancement ; s'il s'appliquait sans ce droit, un compte redevenu
  // ordinaire ouvrirait sa partie sur un terrain repeint que plus aucune case
  // ne peut éteindre.
  [[nodiscard]] ro::ui::SessionRights Rights() const noexcept;

  // CE QUE LE MENU D'ÉCHAP PROPOSE, relu à chaque image : le joueur peut MOURIR
  // menu ouvert, et un menu qui garderait alors ses boutons de vivant
  // proposerait des gestes que le serveur refuse en silence.
  [[nodiscard]] ro::ui::GameMenuFacts MenuFacts() const noexcept;

  // PEUT-IL SE RELEVER SUR PLACE ? ⚠ LES DEUX VOIES DU SERVEUR, et pas une :
  // un objet du groupe des jetons de Siegfried, OU l'état de régénération.
  // N'en regarder qu'une laisserait le bouton absent là où le geste marcherait.
  [[nodiscard]] bool CanResurrect() const;

  // JOUE CE QUE LE MENU D'ÉCHAP A DEMANDÉ AU SERVEUR.
  //
  // ⚠ ICI ET NON DANS LES RÉGLAGES : ces trois gestes-là demandent la connexion
  // ou le routage de la conversation, que `GameSettings` ne possède pas. Les
  // autres choix — ouvrir une fenêtre, quitter — y restent.
  void ApplyMenuGesture(ro::ui::GameMenuChoice chosen, std::uint32_t now_ms);

  // CE QUE LA FENÊTRE DES MACROS A DEMANDÉ : une ligne à envoyer, l'écran des
  // raccourcis, les valeurs d'usine, ou le fichier à mettre à jour.
  // LE PLACEMENT LIBRE : ce qu'on vient de poser sur une case. ⚠ N'ÉMET RIEN —
  // le serveur ne connaît aucune position d'objet.
  void PlaceDraggedItem(const ro::ui::InventoryWindowAction& asked);
  void ApplyMacroAnswers(const ro::ui::ChatMacroAsk& asked, std::uint32_t now_ms);

  // CE QUE LA SECTION DE CAPTURE A DEMANDÉ : tracer, filmer, prendre une image —
  // et la zone qui vient d'être tracée, qu'il faut écrire dans le fichier.
  //
  // ⚠ ICI ET NON DANS LES RÉGLAGES, comme les trois gestes du menu d'Échap : ces
  // boutons-là parlent à une machine qui tient l'écran et une horloge, et
  // les réglages ne possèdent ni l'une ni l'autre.
  void ApplyCaptureGesture(const UiStage::Answers& answers, std::uint32_t now_ms);

  // ── LA GUILDE, LE GROUPE ET LES AMIS ──────────────────────────────────────
  //
  // ⚠ CHACUN DANS SON FICHIER, `game_guild.cpp` et `game_party.cpp` : ouvrir,
  // rafraîchir avant le dessin, appliquer après — c'est tout ce qu'ils coûtent.
  void OpenGuild();
  void RefreshGuild(std::uint32_t now_ms);
  void ApplyGuildAnswers(const UiStage::Answers& answers, std::uint32_t now_ms);
  void RefreshParty(std::uint32_t now_ms);
  void ApplyPartyAnswers(std::uint32_t now_ms);

  // Ce que la souris désigne au sol, cherché UNE FOIS par image : trois choses
  // en dépendent, et trois recherches pourraient diverger.
  [[nodiscard]] std::optional<ro::sim::CellPosition> HoveredCell() const;
  [[nodiscard]] bool CameraTurning() const;

  // LA FORME DU CURSEUR, décidée une fois par image — sa peinture attend
  // l'interface.
  //
  // 🔴 À PART DU DESSIN DU MONDE, parce qu'il faut l'atteindre même quand le
  // monde n'a rien à dessiner. Cacher le curseur du système est le corollaire
  // d'avoir le nôtre (`LoadCursor`), et ce corollaire est inconditionnel : une
  // installation sans garde-robe sort de `DrawWorld` avant ce calcul, et ne
  // laisserait AUCUN pointeur à l'écran.
  //
  // `designated` est ce que la souris désigne, ou nul quand rien n'a été
  // cherché.
  void ComposeCursor(const ro::sim::Actor* designated,
                     const std::optional<ro::sim::CellPosition>& hovered_cell,
                     std::uint32_t now_ms);

  // REPEINT L'IMAGE DU MONDE À TRAVERS LA TEINTE DE LA VEILLE. ⚠ ENTRE LE MONDE
  // ET L'INTERFACE, et hors du dessin du monde, qui a une sortie anticipée.
  void TintForAfk(std::uint32_t now_ms);

  // PEINT LE CURSEUR, tout en dernier. ⚠ APPELÉ DEPUIS DEUX CHEMINS : l'image
  // ordinaire et celle de la veille, qui n'a plus d'interface mais garde sa
  // flèche tant que le joueur ne l'a pas décochée.
  void PaintCursor();

  // ── L'ÉCRAN DE VEILLE, dans `game_afk.cpp` ─────────────────────────────────
  //
  // ⚠ MÊME CLASSE, AUTRE UNITÉ DE TRADUCTION : la veille est un sujet entier —
  // la décision, la caméra, l'annonce au serveur et deux lignes d'horloge.

  // Rapporte un événement au compteur d'inactivité. Rend vrai quand il doit
  // être AVALÉ, c'est-à-dire ne parvenir ni au jeu ni à l'interface.
  [[nodiscard]] bool NoteAfkInput(const SDL_Event& event, std::uint32_t now_ms);

  // Avance la veille d'une image : la décision, la pose, et ce qu'on dit au
  // serveur. ⚠ APRÈS `AimCamera` ET AVANT LE DESSIN : la pose écrite ici vaut
  // pour l'image en cours.
  void AdvanceAfk(std::uint32_t now_ms);

  // Entre en veille sur-le-champ — le bouton d'essai du panneau.
  void StartAfkNow(std::uint32_t now_ms);

  // Les deux lignes de l'horloge, écrites dans leurs tampons, et la vue qui les
  // désigne.
  [[nodiscard]] ro::ui::AfkClockView AfkClock(std::uint32_t now_ms);

  // ── LA CARTE ───────────────────────────────────────────────────────────────
  // ⚠ FACULTATIVE, PARCE QU'UNE CARTE NE SE CONSTRUIT PAS À VIDE : elle porte
  // un terrain, un atlas et un maillage qui n'ont pas d'état neutre. Elle est
  // posée par l'ouverture, et remplacée à chaque changement de carte.
  std::optional<MapStage> stage_;
  // L'ENREGISTREUR DE ZONE. ⚠ IL VIT DANS LA BOUCLE ET NON DANS L'ÉTAGE
  // D'INTERFACE : sa prise a lieu APRÈS le dessin des fenêtres et AVANT la
  // présentation, c'est-à-dire à un moment que seule la boucle connaît.
  ZoneRecorder recorder_;
  // Le planificateur POINTE dans la carte : il se refait à chaque changement,
  // sous peine de viser celle d'avant.
  ro::sim::WalkPlanner planner_{nullptr};
  std::array<float, 4> background_{};
  std::string attempted_map_;

  // ── LES EFFETS, LE SON, LE SOL ─────────────────────────────────────────────
  std::optional<ro::render::EffectCatalog> effect_catalog_;
  std::optional<EffectNodesInPlay> effect_nodes_;
  std::optional<EffectsInPlay> effects_;
  // La zone que les sorts en cours d'incantation dessinent au sol. Elle tient
  // le VFS pour ses motifs, d'où l'optionnel : elle naît avec la partie.
  std::optional<SkillAreas> skill_areas_;
  // Les messages des Graffitis, couchés au sol. Même montage que la zone.
  std::optional<GraffitiOnGround> graffiti_;
  ActorFootprints footprints_;
  // ⚠ UNE POIGNÉE, PAS UNE POSSESSION : la coquille la tient, parce que
  // l'écran de connexion la veut aussi. Même montage que `audio_`.
  MapMusic& music_;
  std::optional<ActorSounds> actor_sounds_;
  std::optional<GroundUnits> ground_units_;

  // ── LA CAMÉRA, LA SOURIS, LE CURSEUR ───────────────────────────────────────
  ro::render::Camera camera_;
  CameraAim aim_;
  CameraRig camera_rig_;
  // L'image précédente, pour l'heure du ciel qui tourne : voir `SkyOfFrame`.
  std::optional<std::uint32_t> last_sky_ms_;
  // Le fuseau du serveur s'est révélé inconnu : dit une fois au journal.
  bool sky_zone_unknown_reported_ = false;
  // Les cartes où le cycle jour/nuit ne s'applique pas : voir `ClosedMaps`.
  ClosedMaps closed_maps_;
  // LES DOCUMENTS D'ÉDITION DES CARTES, et le mode éditeur : voir docs/adr/0118.
  MapEditStore map_edits_;
  MapEditorSession map_editor_;
  // La bibliothèque de l'éditeur, lue à sa première ouverture.
  std::shared_ptr<const std::vector<std::string>> model_library_;
  // Les vignettes que l'éditeur montre au survol d'un modèle.
  ModelPreviews model_previews_;
  // Ce que l'outil Sommets sait du modèle désigné, et les fichiers qu'il a lus.
  VertexContextCache vertex_context_;
  // Le bilan par seconde du décor que l'éditeur repose.
  ReplaceTally replace_tally_;
  ro::render::DrawableSize size_{};
  float aspect_ratio_ = 1.0F;
  glm::vec2 mouse_{0.0F, 0.0F};
  WorldMouse world_mouse_;
  ro::render::CursorShape cursor_shape_ = ro::render::CursorShape::kArrow;
  std::uint32_t cursor_since_ms_ = 0;
  NameCardAsker name_card_asker_;
  // ⚠ COMPOSÉ DANS LA SCÈNE, PEINT APRÈS L'INTERFACE : sa forme dépend de ce que
  // la souris désigne dans le monde, mais rien ne doit passer devant lui, pas
  // même une fenêtre.
  std::vector<ro::render::ScreenQuad> cursor_quads_;

  // ── L'ÉCRAN DE VEILLE ──────────────────────────────────────────────────────
  //
  // ⚠ LES DEUX VONT ENSEMBLE et ne se séparent pas : le compteur décide QUAND
  // s'endormir et ce qu'il faut avaler, la veille ce que cela fait à l'image.
  AfkIdle afk_idle_;
  AfkScreen afk_screen_;
  // LA TEINTE DE LA VEILLE, sur l'image entière, après le post-traitement.
  ro::render::ScreenTint screen_tint_;
  // « ESSAYER MAINTENANT », armé par la page de réglages. ⚠ CONSOMMÉ À L'IMAGE
  // SUIVANTE : laissé en place, la veille se relancerait à chaque image.
  bool afk_requested_ = false;
  // Les deux lignes de son horloge vivent dans `GameUiBuffers`.

  // ── CE QUE L'IMAGE COMPOSE ─────────────────────────────────────────────────
  ComposedScene scene_actors_;
  // Les rémanences vivent PLUS LONGTEMPS qu'une image : leur propre magasin.
  ActorGhosts ghosts_;
  // D'où chaque butin en vol est parti. Même raison : un arc dure trois
  // quarts de seconde, et son point de départ ne se retrouve plus après.
  ItemDrops drops_;
  ActorTalk talk_;
  std::optional<ro::sim::PickupIntent> pickup_;
  // LA CIBLE QU'ON POURSUIT, tant qu'on n'est pas à portée d'elle. Le serveur
  // ne rapproche pas un joueur : voir `world/attack_intent.h`.
  std::optional<ro::sim::PendingAction> attack_;

  // ── LES PANNEAUX D'ATELIER ─────────────────────────────────────────────────
  SkillBookPanel grimoire_;
  EffectBenchPanel effect_bench_;
  std::optional<ro::sim::CellPosition> cell_cursor_;

  // ── LES RÉGLAGES ET LA FICHE ───────────────────────────────────────────────
  // Les onglets de la conversation : ce qu'ils gardent, et ce qu'ils trient.
  std::optional<ChatTabsView> chat_tabs_;
  // Où part la ligne en cours de frappe : relu de la saisie à chaque image.
  ChatLinePreviewMaker chat_preview_;
  // Une seule liste pour l'autocomplétion du champ ET le menu des macros.
  ChatCommandCatalogue chat_commands_;
  std::optional<SkillSheet> sheet_;
  std::optional<ShortcutBarView> shortcuts_;
  // LES ÉTATS DU JOUEUR, CROISÉS. Elle désigne la table des statuts et leurs
  // icônes, qui vivent dans les accessoires : elle se construit donc après eux.
  std::optional<StatusView> status_view_;
  // LE SAC, CROISÉ. Il désigne la table des objets, les mots et les icônes,
  // qui vivent tous dans les accessoires : il se construit donc APRÈS eux.
  std::optional<InventoryView> bag_;
  // LE SERTISSAGE, croisé avec les mêmes accessoires que le sac.
  std::optional<CardInsertView> card_insert_;
  // LES COULEURS COMPOSÉES, du fil à la garde-robe — voir `world/actor_styles.h`.
  ActorStyles styles_;
  // LA FICHE, CROISÉE. Elle désigne les mêmes accessoires que le sac — la
  // table des objets, les mots, les icônes — et se construit donc après eux.
  std::optional<CharacterView> character_;
  // L'APPARENCE QUE SON PANTIN MONTRE, habillée avant le dessin : c'est elle que
  // le GIF photographie, au clic qui suit.
  ro::render::ActorLook sheet_look_;
  std::optional<PresetView> presets_;           // son onglet des presets, et leur fichier
  std::optional<StatPresetDesk> stat_presets_;  // les préréglages de stats du compte
  // SON ONGLET DES TITRES, croisé avec les deux tables qui les nomment.
  std::optional<TitleView> title_view_;
  // SON ONGLET DE GUILDE, et les emblèmes qu'il montre et qu'il dépose.
  std::optional<GuildPanel> guild_panel_;
  std::optional<GuildEmblems> guild_emblems_;
  std::optional<PartyPanel> party_panel_;  // la fenêtre Groupe / Amis et la grille
  // L'ÉDITEUR DE STYLE, du fil jusqu'au pantin — voir `character/style_view.h`.
  std::optional<StyleView> style_view_;
  // L'APERÇU D'UN STYLE REÇU PAR LE FIL. ⚠ À PART DE L'ÉDITEUR, et il le
  // faut : il s'ouvre depuis la conversation, donc sans que l'éditeur ait
  // forcément servi, et refermer l'un ne doit pas refermer l'autre.
  std::optional<StylePreview> style_preview_;
  // LA CHARRETTE, croisée comme le sac et sur les mêmes accessoires.
  std::optional<CartView> cart_;
  // LE COFFRE, croisé de même. ⚠ SON CONTENU N'EST PAS À NOUS : il n'existe que
  // le temps d'une session que le serveur ouvre et referme.
  std::optional<StorageView> storage_;
  // LA BANQUE. ⚠ ELLE NE DÉSIGNE AUCUN ACCESSOIRE — pas de table d'objets, pas
  // d'icône —, et se construit donc avec la partie plutôt qu'après les
  // accessoires. Elle ne porte que le rang du dernier verdict déjà dit et
  // l'instant de la dernière demande d'ouverture.
  BankView bank_;
  CardAlbumView card_album_;  // l'album de cartes : voir `items/card_album_view.h`
  // LA DESCRIPTION D'UN OBJET, sur les mêmes accessoires. ⚠ ELLE NE DÉPEND
  // D'AUCUN CONTENANT : sa clé est un identifiant, et on décrit aussi bien ce
  // qu'on n'a jamais tenu.
  std::optional<DescriptionView> item_description_;
  // L'APERÇU AU SURVOL. ⚠ SA PROPRE VUE, et non celle de la fenêtre : les deux
  // montrent souvent deux objets différents — on survole l'un en lisant l'autre
  // — et une seule vue les ferait se voler leurs textes à chaque mouvement.
  std::optional<DescriptionView> description_preview_;
  // « VOIR L'ÉQUIPEMENT » : ce qu'un autre joueur a montré, et sa vue. ⚠ LE
  // LIVRE EST HORS DU MONDE, comme les titres, et les deux descriptions le lisent.
  ro::sim::ViewedEquipmentBook inspected_;
  std::optional<ViewedEquipmentView> viewed_equipment_;
  // LA FICHE D'UN MONSTRE : ce que le serveur en a dit, le relevé de Sense, sa vue.
  std::optional<MonsterSheet> monster_sheet_;
  // LA BASE DE DONNÉES : ses tables, lues à la première ouverture, et ses vues.
  DatabaseStage database_;
  std::optional<CraftAtlasView> craft_atlas_;  // l'Atlas des recettes : son livre, sa vue
  // « MONTRER MON ÉQUIPEMENT » ET « VOIR LES COSTUMES » : ce que le serveur dit
  // des nôtres. ⚠ UN ÉTAT DE SESSION : l'équipement ne se réannonce pas au
  // changement de carte, contrairement aux costumes.
  ro::sim::PlayerConfig own_config_;
  ro::sim::CharacterLedger ledger_;  // quêtes, réputation… : ce qu'aucune fenêtre ne montre
  // LA DESCRIPTION D'UNE COMPÉTENCE. ⚠ SA PROPRE VUE, comme l'aperçu : on
  // compare volontiers une compétence à l'objet qui la donne, et les deux
  // fenêtres restent ouvertes ensemble.
  std::optional<SkillDescriptionView> skill_description_;
  // LE DIALOGUE D'UN PNJ, croisé : le monde en porte le texte, l'acteur le nom,
  // et les accessoires les icônes que ce texte appelle.
  std::optional<NpcTalkView> dialogue_;
  // ── CE QU'ON A DÉJÀ DEMANDÉ AU SERVEUR, ET QUAND ──────────────────────────
  // Les chiffres des onglets techniques d'une compétence, sans harceler.
  SkillFactsAsker skill_facts_;
  // ── LES LIENS DE LA SAISIE : les liens en attente et leurs libellés vivent
  // dans `GameUiBuffers`, parce que la rangée de saisie pointe dedans.
  // Pose le lien de cette place dans la saisie ; sans effet si l'objet n'y est plus.
  void PostItemLink(const ro::sim::Inventory& holder, std::uint16_t client_slot);
  // Pose une balise DÉJÀ COMPOSÉE : c'est ce que fait le Maj+clic sur un lien
  // qu'on relaie, et l'objet n'a pas à être en sac pour cela.
  void PostChatLink(const ro::vfs::ItemLink& link);
  void PostChatLink(const MonsterLink& link);  // `<MOBL>` : `chat_monster_link.h`
  void PostChatLink(const RecipeLink& link);   // `<CRAF>` : `chat_recipe_link.h`
  void PostChatLink(const PlaceLink& link);    // `<NAVIL>` : `chat_place_link.h`
  void PostChatLink(const MvpLink& link);
  void PostChatLink(const NaviSearchLink& link);  // `<NAVS>` : `chat_navi_search_link.h`
  // POSE LE LIEN D'UN STYLE DANS LA SAISIE : `<STYL>étiquette:code</STYL>`.
  //
  // 🔴 LA RECETTE ENTIÈRE VOYAGE, et pas une désignation : un style n'existe
  // nulle part hors de son porteur. ⚠ ÉTIQUETTE VIDE = NOTRE PSEUDO, résolu
  // ici : le nom du personnage appartient au monde, pas à l'éditeur de style.
  void PostStyleLink(const std::string& code, const std::string& label);
  // LE DÉPÔT LUI-MÊME, commun aux deux espèces : élagage, plafond, séparation
  // du mot d'avant, et la barre qu'on déplie. Faux quand la place manque.
  //
  // ⚠ UN SEUL ENDROIT QUI ÉCRIT DANS LA SAISIE : deux copies laisseraient un
  // jour l'une oublier de déplier la barre, et le lien partirait dans un champ
  // que personne ne voit.
  [[nodiscard]] bool PlaceInChatInput(PendingChatLink link);
  // Joue ce que le joueur vient de faire d'un lien, d'où qu'il vienne ; d'un item
  // de CATALOGUE, sans jeton — un butin, un matériau ; puis d'un objet de COMMERCE.
  void FollowChatLink(const ro::ui::ChatLinkAsk& asked, std::uint32_t now_ms);
  void FollowMvpLink(const ro::ui::ChatLinkAsk& asked, const MvpLink& link, std::string_view sender,
                     std::uint32_t now_ms);
  void FollowCatalogItem(const ro::ui::ChatLinkAsk& asked, std::uint32_t now_ms);
  void FollowItemGesture(ro::ui::ChatLinkGesture gesture, const ItemGestureTarget& item,
                         std::uint32_t now_ms);
  void FollowTradeItem(const TradeItemFocus& focus, std::uint32_t now_ms);
  // UN NOM DE MONSTRE d'où qu'il vienne, puis une recherche `<NAVS>` et son ouverture.
  void FollowMonsterGesture(ro::ui::ChatLinkGesture gesture, const MonsterLink& monster,
                            bool by_view, std::uint32_t now_ms);
  void FollowNaviSearchLink(const ro::ui::ChatLinkAsk& asked, const NaviSearchLink& link,
                            std::uint32_t now_ms);
  void OpenNaviSearch(const NaviSearchLink& link);
  // La branche des liens de STYLE. ⚠ AUCUN DE SES GESTES NE TOUCHE AU
  // PERSONNAGE : un code vient d'un autre joueur.
  void FollowStyleLink(const ChatLinkTarget& aimed, ro::ui::ChatLinkGesture gesture,
                       std::uint32_t now_ms);
  // LE MÊME GESTE, DEPUIS UN DIALOGUE DE PNJ, et les liens de son balisage.
  void FollowNpcDialogLink(const ro::ui::NpcDialogAnswer& answer, std::uint32_t now_ms);
  // L'objet survolé à l'image PRÉCÉDENTE : une infobulle attend que la souris s'arrête.
  std::uint32_t hovered_item_ = 0;
  // L'ÉTAT qu'un lien de dialogue survole. ⚠ RELEVÉ À L'IMAGE PRÉCÉDENTE : la
  // fenêtre le dit dans sa réponse, que l'on lit APRÈS l'avoir dessinée.
  ro::sim::StatusEffectId hovered_status_ = 0;
  // ⚠ SA PLACE ET SON CONTENANT : c'est ce qui décore le nom de l'aperçu.
  std::uint16_t hovered_slot_ = 0;
  ro::ui::ItemHolder hovered_holder_ = ro::ui::ItemHolder::kInventory;

  // ── CE QUI NE SE DIT QU'UNE FOIS ───────────────────────────────────────────
  SceneCensus reported_census_;
  std::set<std::uint16_t> reported_undecodable_;
  std::set<std::uint16_t> reported_unhandled_;
  std::size_t reported_missing_ = 0;
  ro::net::FrameTally frame_tally_;
  // 🔴 GARDE LA SESSION. ⚠ Déclarée APRÈS `client_root_`, que `FreshHandshake` lit.
  ro::net::MoonclientHandshake handshake_{FreshHandshake()};
  ro::net::MoonclientHandshakeState reported_handshake_ =
      ro::net::MoonclientHandshakeState::kUnsent;

  // ── LES COMPTEURS DE LA BOUCLE ─────────────────────────────────────────────
  std::uint32_t acknowledged_entries_ = 0;
  std::uint32_t last_tick_ms_ = 0;
  std::uint64_t start_ticks_ = 0;
  bool running_ = true;
  // ── LE RETOUR À LA SÉLECTION DE PERSONNAGE ─────────────────────────────────
  // La demande est partie, et quand : le serveur répond d'un accord ou d'un
  // refus, et une absence de réponse se dit au bout d'un délai.
  std::optional<std::uint32_t> character_select_asked_ms_;
  bool back_to_characters_ = false;
  // Lit la réponse du serveur dans cette trame. Rend vrai quand la main passe :
  // ce qui suit sur la liaison de carte ne se lit plus.
  bool FollowCharacterSelect(const ro::net::Frame& received, std::uint32_t now_ms);
  // Une demande restée sans réponse se dit, et se rend.
  void ExpireCharacterSelect(std::uint32_t now_ms);
  // LE CIEL DE LA CARTE A-T-IL ÉTÉ LEVÉ ? Une seule fois par carte : ses nœuds
  // vivent aussi longtemps qu'elle, et le resemer à chaque image empilerait
  // quatre-vingts nœuds par image. Remis à faux avec le reste, au changement de
  // carte — voir `ForgetWhatPlayed`.
  bool sky_raised_ = false;
  // Ce que GreyWorld et les bascules d'effet en ont fait, et de la météo :
  // sans sens tant qu'il n'est pas levé. Voir `RaiseMapSky`.
  bool sky_hidden_ = false;
  bool weather_hidden_ = false;
  // Les effets de la carte ont-ils été montés sur CETTE scène ? Remis à faux au
  // seul changement de carte : un téléport sur place ne les recrée pas.
  bool map_effects_raised_ = false;
  // Les objets d'effet levés, et la nuit de l'image précédente qui décide de
  // ceux « nuit seulement » : voir `AdvanceMapEffects`.
  std::vector<ro::formats::RswEffect> raised_effects_;
  float effect_night_ = 0.0F;
  // L'heure du ciel de l'image précédente, pour les plages d'heures.
  std::optional<float> effect_hour_;
  // CE QUE LE ROUTAGE DU CHAT CONFIE À LA PARTIE, vidé à chaque image.
  std::vector<PlayerCommand> player_asks_;

  // ⚠ EN DERNIER, ET IL LE FAUT : il garde l'adresse de presque tout ce qui
  // précède. Détruit le premier, il ne pend sur rien.
  std::optional<ChatDispatch> chat_dispatch_;
};

}  // namespace moonclient
