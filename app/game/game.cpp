// LA BOUCLE D'UNE PARTIE : les événements, le réseau, puis l'interface.
//
// Le dessin du monde vit à côté, dans `game_draw.cpp` : c'est la plus grosse des
// étapes, et la seule dont l'ordre des couches soit un sujet à part entière.

#include "game/game.h"

#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#include "ro/core/log.h"
#include "ro/net/movement.h"
#include "ro/net/player_config.h"
#include "ro/net/skill_facts.h"
#include "ro/net/world_update.h"
#include "ro/render/skin_pieces.h"
#include "ro/sim/damage_meter.h"
#include "ro/sim/movement.h"
#include "ro/vfs/cash_shop_words.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_name.h"

#include "character/staff_view.h"
#include "character/stat_bar_values.h"
#include "chat/chat_setting_link.h"
#include "chat/emoji_catalog.h"
#include "game/battleground_stage.h"
#include "game/bug_report_context.h"
#include "game/connected_session.h"
#include "game/debug_console_route.h"
#include "game/hud_lock.h"
#include "game/interface_sound.h"
#include "game/menu_icon_route.h"
#include "game/setting_destination_open.h"
#include "game/warp_list_stage.h"
#include "game/window_events.h"
#include "items/inventory_view.h"
#include "options/damage_page_names.h"
#include "options/entity_page_tabs.h"
#include "world/cast_bar_view.h"
#include "world/click_action.h"
#include "world/damage_path_sample.h"
#include "world/hit_popup_rules.h"
#include "world/npc_talk.h"
#include "world/sound_zone_view.h"

namespace moonclient {

ro::core::Result<void> Game::Frame(std::uint32_t now_ms) {
  PumpEvents(now_ms);
  if (auto ok = PumpNetwork(now_ms); !ok) {
    return ok;
  }
  // LES QUÊTES SUIVENT LA LANGUE, si le joueur vient d'en changer.
  FollowQuestLanguage(*vfs_, pieces_);
  // ET L'INTERFACE SUIT LE SKIN, s'il vient d'en choisir un autre — les
  // effets aussi, dont certaines images sont des icônes d'objet.
  if (shell_.FollowSkinChoice()) {
    effect_renderer_.ForgetTextures();
  }
  // LE TITRE, après le réseau qui a pu changer la carte. ⚠ À CHAQUE IMAGE, et
  // sans coût : la fenêtre ne change que si le texte a changé.
  gl_->SetTitle(WindowTitle(settings_->options().pages().window_title.names_shown(), world_));
  // 🔴 LE CARNET MVP AVANCE À CHAQUE IMAGE, LIAISON FERMÉE COMPRISE : sa
  // fenêtre et ses lignes détachées se dessinent hors ligne, leurs gestes sont
  // locaux, et les faits des liens prêtés au chat ne se vident qu'ici. Après le
  // réseau, donc après le guidage et l'annonce des surfaces de l'image.
  AdvanceMvpTracker(now_ms);
  // LE JOURNAL DES QUÊTES ET LE SUIVI, après le réseau qui a avancé les succès.
  AdvanceQuests(now_ms);
  // 🔴 LE CHAMP DE BATAILLE AUSSI, LIAISON FERMÉE COMPRISE : sa file survit aux
  // cartes, ses fenêtres suivent la file, et ce qu'elles demandent part d'ici.
  pieces_.battleground.Advance(
      windows_.battleground,
      BattlegroundContext{
          .link = link_,
          .world = world_,
          .party = party_,
          .guild = guild_book_,
          .vfs = vfs_,
          .image_at = [this](std::string_view path) -> ro::ui::SkinImage {
            if (!pieces_.game_images.has_value()) {
              return {};
            }
            const ro::render::IconTexture& icon =
                pieces_.game_images->GetOurs(std::string(ro::render::kSkinRoot) += path);
            if (!icon.usable()) {
              return {};
            }
            return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                                     .width = static_cast<float>(icon.width_px),
                                     .height = static_cast<float>(icon.height_px)};
          },
          .item_at = [this](std::uint32_t item_id) -> BattlegroundItemFace {
            const TradeItems& items = pieces_.trade.items();
            const bool known = items.table() != nullptr && items.table()->Find(item_id) != nullptr;
            return BattlegroundItemFace{.icon = items.IconOf(item_id),
                                        .name = known ? items.NameOf(item_id) : std::string()};
          },
          .link_ready = link_.is_open(),
          .own_char_id = ticket_.character_id,
          .now_ms = now_ms});
  // SES AVIS DANS LE CHAT, sur le canal du champ de bataille ; ses sons, comme
  // ceux de l'interface.
  for (std::string& notice : pieces_.battleground.TakeNotices()) {
    world_.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kBattlefield, 0,
                      std::move(notice), now_ms);
  }
  if (std::optional<BugReportContext> report = pieces_.battleground.TakeBugReport();
      report.has_value()) {
    AskBugReport(windows_.bug_report, std::move(*report));
  }
  // UNE RÉCOMPENSE se décrit, se relaie et s'interroge comme un butin.
  FollowCatalogItem(pieces_.battleground.TakeItemGesture(), now_ms);
  // LES SONS D'INTERFACE de l'image : ceux du champ de bataille, puis l'arrivée
  // d'un membre au salon.
  const auto play_sounds = [this](std::vector<InterfaceSoundFile> sounds, std::string_view from) {
    for (const InterfaceSoundFile& sound : sounds) {
      if (vfs_ == nullptr) {
        return;
      }
      if (const ro::core::Result<void> played =
              PlayInterfaceSound(audio_, *vfs_, client_root_, sound);
          !played) {
        ro::core::LogWarning("son {} : {}", from, ro::core::Describe(played.error()));
      }
    }
  };
  play_sounds(pieces_.battleground.TakeSounds(), "du champ de bataille");
  play_sounds(pieces_.chat_rooms.TakeSounds(), "du salon");
  // LES CLASSEMENTS attendent leurs noms ; les avis d'étoile ont leur son.
  pieces_.rank_notices.Advance(world_, now_ms);
  // Le tableau montre le dernier classement, et demande celui d'un onglet.
  pieces_.ranking_board.Advance(windows_.ranking_board, pieces_.rank_notices, link_, now_ms);
  std::vector<InterfaceSoundFile> star_sounds;
  for (std::string& sound : pieces_.rank_notices.TakeSounds()) {
    star_sounds.push_back(
        InterfaceSoundFile{.place = InterfaceSoundPlace::kSoundArchive, .path = std::move(sound)});
  }
  play_sounds(std::move(star_sounds), "d'un avis d'étoile");
  // ⚠ APRÈS LE RÉSEAU, DONC APRÈS `AimCamera`, ET AVANT LE DESSIN : la veille
  // écrit la pose de la caméra, et elle doit écraser le cadrage du personnage,
  // pas être écrasée par lui.
  AdvanceAfk(now_ms);
  // LE RELIEF D'ABORD : la caméra, le voile et le dessin le lisent tous.
  if (auto followed = FollowGreyWorldRelief(); !followed) {
    return followed;
  }
  camera_rig_.SetBounded(!(windows_.staff_tools.look.free_camera && Rights().staff_tools));
  camera_rig_.SetExpandedZoom(settings_->options().expanded_zoom());
  // Après le suivi, la secousse et la veille : l'œil de cette image.
  camera_rig_.FloorEye(camera_, *stage_);
  // LE DÉCOR SE VOILE SUR CET ŒIL-LÀ, avant d'être peint : voir `DecorVeilView`.
  // ⚠ ÉTEINT QUAND GREYWORLD ÔTE LES DÉCORS : il les repeindrait en translucide.
  pieces_.decor_veil.Advance(DecorVeilFrame{
      .enabled = settings_->options().graphics().decor_veil() &&
                 !ro::render::GreyWorldHidesModels(settings_->options().pages().grey_world.look()),
      .world = &world_,
      .planner = &planner_,
      .stage = &*stage_,
      .camera = &camera_,
      .aspect_ratio = aspect_ratio_,
      .width = static_cast<std::int32_t>(shell_.width()),
      .height = static_cast<std::int32_t>(shell_.height()),
      .now_ms = now_ms});
  pieces_.night_light.Advance(*stage_, world_, now_ms);
  // Pas pendant la veille ni une secousse : la caméra ne porte pas alors la vue
  // du joueur.
  if (link_.is_open() && !afk_screen_.active() && !aim_.shake.shaking()) {
    if (const std::optional<CameraMemory> now = camera_rig_.MemoryNow(camera_); now.has_value()) {
      settings_->NoteCameraMemory(*now);
    }
  }
  if (auto ok = DrawWorld(now_ms); !ok) {
    return ok;
  }
  // ⚠ ENTRE LES DEUX, ET C'EST TOUT CE QUI LA DÉFINIT : elle teinte ce que le
  // monde vient de peindre, et l'interface se pose par-dessus, intacte. Voir
  // `TintForAfk`, et la sortie anticipée qui l'a fait sortir du dessin.
  TintForAfk(now_ms);
  DrawInterface(now_ms);
  // 🔴 LA PRISE ICI, ENTRE LE DESSIN ET LA PRÉSENTATION. Plus tôt, elle
  // rendrait un écran sans ses fenêtres — or ce sont elles que l'on montre.
  // Plus tard, il n'y a plus rien à lire : l'image a été présentée.
  //
  // ⚠ ELLE RESTE DANS LA PARTIE bien que la présentation soit à la coquille :
  // l'enregistreur est à la partie, et ce qu'il prend est ce que la partie
  // vient de dessiner. Le seul point commun avec la coquille est l'instant.
  const ro::render::DrawableSize drawable = gl_->drawable_size();
  const ro::render::Resolution points = gl_->size();
  recorder_.GrabIfDue(FrameSurface{.drawable_width = drawable.width,
                                   .drawable_height = drawable.height,
                                   .point_width = points.width,
                                   .point_height = points.height},
                      now_ms);
  return {};
}

Game::~Game() {
  // Le rappel des profils de réglages désigne cette partie.
  settings_->profiles().AfterPose({});
}

void Game::Close() {
  // La conversation, pour la partie suivante. ⚠ LES RÉGLAGES NE SONT PAS ICI :
  // ils appartiennent à la coquille, qui les écrit après nous. Celle-ci ne fait
  // que les LIRE, pour savoir ce que le joueur veut garder.
  if (chat_tabs_.has_value()) {
    const ro::ui::ChatAppearance& look = settings_->options().chat_look();
    chat_tabs_->SavePast(look.keep_history, look.kept_lines, chat_preview_.whispered(),
                         look.keep_typed, windows_.chat.main.input.history.lines);
  }
  // Le livre encore ouvert garde sa page, comme s'il venait d'être refermé.
  if (item_description_.has_value()) {
    item_description_->books().NoteLeaving(windows_.book);
  }
  // L'IA DES COMPAGNONS : ses profils en attente, et les délais pour la
  // session suivante.
  pieces_.companion_ai.Leave(static_cast<std::uint32_t>(SDL_GetTicks()));
  // La minimap quitte le panneau d'options : ses réglages vivent avec la partie.
  settings_->options().pages().minimap_panel.Entrust(nullptr);
  // Le pseudo et la carte quittent le titre avec la partie.
  gl_->SetTitle(WindowTitle());
}

void Game::PumpEvents(std::uint32_t now_ms) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // 🔴 LE COMPTEUR D'INACTIVITÉ VOIT L'ÉVÉNEMENT LE PREMIER, ET PEUT L'AVALER. Il
    // faut décider d'avaler AU MOMENT MÊME où l'on constate le geste : un drapeau
    // « on s'est réveillé » consommé plus tard laisserait passer le clic de l'image
    // en cours — celui-là même qui déplacerait le personnage dans une direction que
    // le joueur n'a pas choisie, puisqu'il visait un décor vu sous un autre angle.
    if (NoteAfkInput(event, now_ms)) {
      continue;
    }
    // L'INTERFACE VOIT TOUT, ET AVANT TOUT LE MONDE : elle ne sait ce qu'il y a
    // sous le curseur que si elle a vu la souris bouger. Ce qu'elle en dit
    // ensuite décide à qui revient le geste — voir `window_events.h`.
    interface_->TakeEvent(event);
    RouteEvent({.gl = &*gl_,
                .interface = &*interface_,
                .link = &link_,
                .world = &world_,
                .stage = &*stage_,
                .planner = &planner_,
                .effects = &*effects_,
                .effect_nodes = &*effect_nodes_,
                .camera = &camera_,
                .rig = &camera_rig_,
                .chat = &windows_.chat.main,
                .grimoire = &grimoire_,
                .skill_tree = pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
                .sheet = &windows_.character,
                .grimoire_keys = &pieces_.grimoire_keys,
                .homunculus_present = pieces_.homunculus.view().present,
                .mercenary_present = pieces_.mercenary.view().present,
                .companion_ai = &pieces_.companion_ai,
                .pet = &windows_.pet,
                .inventory = &windows_.inventory,
                .cart = &windows_.cart,
                .party = &windows_.party.window,
                .bank = &bank_,
                .card_album = &card_album_,
                .card_album_window = &windows_.card_album,
                .cash_shop = &pieces_.cash_shop,
                .mail = &pieces_.mail,
                .achievements = &pieces_.achievements,
                .quests = &pieces_.quests,
                .trade = &pieces_.trade,
                .chat_rooms = &pieces_.chat_rooms,
                .chat_room_form_open = windows_.chat_room.make.open,
                .menu = &windows_.menu,
                .macros = &windows_.macros,
                .style = &windows_.style,
                .craft_atlas = &windows_.craft_atlas.state,
                .navigation = &windows_.navigation.state,
                .world_map = &windows_.navigation.world_map,
                .minimap_shown = &navi_.prefs.minimap_shown,
                .mvp_tracker = &pieces_.mvp_tracker,
                .bug_report = &windows_.bug_report,
                .recorder = &recorder_,
                .hotkeys = &windows_.hotkeys,
                .bindings = &settings_->bindings(),
                .presets = &*presets_,
                .targeting = &targeting_,
                .quick_cast = &quick_cast_,
                .entity_menu = &menu_gestures_,
                .afk_now = &afk_requested_,
                .shortcuts = &*shortcuts_,
                .escape_closes_a_window = interface_->escape_has_work(),
                .enter_claimed =
                    CraftStage::ClaimsEnter(windows_.make_item, windows_.weapon_refine,
                                            settings_->options().pages().make_item.settings(),
                                            settings_->options().pages().weapon_refine.settings()),
                .no_shift = settings_->options().no_shift(),
                .no_ctrl = settings_->options().no_ctrl(),
                .shop_one_click = settings_->options().shops().one_click(),
                .bench = &effect_bench_,
                .chat_dispatch = &*chat_dispatch_,
                .scene_actors = &scene_actors_,
                .mouse = &mouse_,
                .world_mouse = &world_mouse_,
                .running = &running_,
                .size = &size_,
                .aspect_ratio = &aspect_ratio_,
                .pickup = &pickup_,
                .attack = &attack_,
                .now_ms = now_ms},
               event);
  }
}

void Game::DrawInterface(std::uint32_t now_ms) {
  // L'INTERFACE APRÈS LE MONDE : rien de ce qu'elle dessine ne doit être masqué
  // par lui, ni par la couche d'écran.
  const ro::ui::SkillPointTally skill_points =
      sheet_->Refresh(windows_.character.open, world_, now_ms);
  // Le droit qui borne la cadence d'item de Quick cast, AVANT que la page ne se
  // compose : il arrive au login et peut changer en cours de session.
  settings_->options().quick_cast().NoteStaff(Rights().staff_tools);
  // L'appel d'urgence, tel que le serveur l'a dit, de même.
  settings_->options().pages().call.NoteCallable(own_config_.callable());
  settings_->Refresh(windows_, now_ms);
  if (windows_.staff_tools.look.camera_hud) {
    windows_.camera_hud =
        camera_rig_.HudOf(camera_, stage_->name, (SDL_GetModState() & SDL_KMOD_CTRL) != 0);
  }
  // LES ZONES DE SON, composées seulement quand l'établi les montre à qui en a
  // le droit : une carte porte des centaines de sources.
  windows_.sound_zones.clear();
  windows_.staff_tools.sound_audible = audio_.sound_audible();
  if (ro::ui::SoundZonesShown(windows_.staff_tools) && Rights().staff_tools &&
      actor_sounds_.has_value()) {
    windows_.sound_zones = SoundZoneViews(*actor_sounds_, *stage_, camera_, aspect_ratio_,
                                          static_cast<float>(shell_.width()),
                                          static_cast<float>(shell_.height()), now_ms);
  }
  // L'ENREGISTREUR RELIT SES RÉGLAGES ET AVANCE SON HORLOGE. ⚠ AVANT LE DESSIN :
  // c'est ce passage qui récolte l'encodage fini et pose le GIF dans le
  // presse-papier, et le témoin doit dire « copié » à l'image où ça arrive.
  recorder_.Follow(settings_->options().capture());
  recorder_.Tick(now_ms);
  // LES FAMILLES DE FENÊTRES COMPOSENT LEURS VUES — voir `game.h`.
  StageChat(now_ms);
  StageDescriptions(now_ms);
  StageContainers();
  pieces_.craft.Stage(world_, link_, windows_.make_item, windows_.weapon_refine,
                      settings_->options().pages().make_item.settings(),
                      settings_->options().pages().weapon_refine.settings(), now_ms);
  windows_.make_item_view = pieces_.craft.make_view();
  windows_.weapon_refine_view = pieces_.craft.refine_view();
  StageCharacterSheet(now_ms);
  StageStyle(now_ms);
  // La barre suit le personnage, par son NOM ; celle d'un compte staff suit
  // aussi son métier, qui change en jeu.
  shortcuts_->FollowCharacter(world_);
  shortcuts_->UseCompanions(&pieces_.homunculus, &pieces_.mercenary);
  pieces_.grimoire_keys.Advance(windows_.character, *shortcuts_, settings_->bindings(), now_ms);
  shortcuts_->Highlight(GrimoireKeys::Highlighted(windows_.character));
  shortcuts_->Refresh(world_, settings_->bindings(), now_ms);
  // LES ÉTATS : ce que le fil a annoncé, et ce que les bits d'altération
  // portent. ⚠ APRÈS LE BALAYAGE DES ÉCHUS, qui tourne en tête d'image : un état
  // fini ne doit pas se recomposer.
  status_view_->Refresh(world_, now_ms, settings_->options().status_preview());
  // LE COMPTEUR DE DÉGÂTS AVANCE, même fenêtre fermée ou écran de veille levé :
  // les cumuls de zone gagnent la conversation à leur heure.
  pieces_.damage_meter.Compose(
      world_, party_, pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
      settings_->options().pages().damage_meter, interface_->DamageFont({}, true), now_ms,
      &windows_.damage_meter_view);
  windows_.damage_meter_view.docked = chat_tabs_->meter_docked();
  // CE QUE L'INTERFACE REND À LA BOUCLE : la réponse au PNJ, ce que le joueur a
  // dit, et ce qu'il a demandé aux réglages. Tout part d'ici, et de nulle part
  // ailleurs.
  // LE DIALOGUE, CROISÉ AVANT LE DESSIN : son texte vient du monde, le nom de
  // son PNJ de l'acteur, et ses icônes des accessoires.
  const UiStage::NpcTalk talk{
      .view = dialogue_->Refresh(world_, now_ms, settings_->options().npc_menu_search()),
      .art = dialogue_->art(),
      // L'APERÇU DE L'ÉTAT SURVOLÉ. ⚠ COMPOSÉ ICI parce qu'il croise la table de
      // l'installation et les icônes, que `ui` ne voit ni l'une ni l'autre.
      .status_preview = status_view_->Preview(hovered_status_),
      .illustration = pieces_.npc_illustration.Stage(
          pieces_.game_images.has_value() ? &*pieces_.game_images : nullptr)};
  // LE CATALOGUE DU VOTE SHOP, pour le menu d'un item de la conversation et du
  // dialogue. ⚠ APRÈS `Refresh`, qui remet la vue du dialogue à zéro.
  const std::span<const ro::ui::VoteShopOffer> vote_shop = pieces_.cash_shop.offers();
  dialogue_->NoteVoteShop(vote_shop);
  dialogue_->SeesSettingReach(SettingReachOf(pieces_));
  windows_.chat.vote_shop = vote_shop;
  // ⚠ LA PILE SE PREND ICI, ET NON PLUS TÔT : c'est le même appel qui purge les
  // lignes expirées et compose les vues. Les prendre en avance laisserait une
  // ligne morte à l'écran une image de plus, et les vues désigneraient une pile
  // qui a bougé depuis.
  const std::span<const ro::ui::ItemObtainToastEntry> obtained =
      pieces_.item_toast.has_value()
          ? pieces_.item_toast->Shown(now_ms, settings_->options().item_obtain())
          : std::span<const ro::ui::ItemObtainToastEntry>{};
  // ── L'ÉCRAN DE VEILLE EFFACE LES FENÊTRES ─────────────────────────────────
  //
  // 🔴 LE DESSIN, PAS L'ÉTAT. Rien n'est fermé, rien n'est déplacé : l'image
  // suivante redessine tout exactement comme avant. C'est la différence avec le
  // « cacher l'interface » du client d'origine, qui ferme ce qui n'est pas sur
  // sa liste blanche — acceptable pour un geste volontaire, pas pour un
  // basculement automatique.
  //
  // ⚠ TOUT CE QUI PRÉCÈDE A TOURNÉ : l'enregistreur a avancé son horloge, la
  // conversation a trié ses lignes, le bandeau a purgé les siennes. Ce qu'on
  // saute est le dessin et les RÉPONSES, c'est-à-dire ce que le joueur aurait
  // demandé — et il n'est pas là.
  if (afk_screen_.hiding_ui(settings_->options().afk())) {
    interface_->DrawAfkOnly(AfkClock(now_ms), pieces_.over_world.Paint());
    // ⚠ LE CURSEUR RESTE S'IL N'EST PAS MASQUÉ : les deux cases sont
    // indépendantes, et rien ne justifie de les lier ici.
    PaintCursor();
    settings_->Tick(now_ms);
    return;
  }

  RefreshGuild(now_ms);
  RefreshParty(now_ms);
  // ⚠ « AUCUNE » ÉTEINT AUSSI UN BANDEAU DÉJÀ LEVÉ.
  windows_.map_sign =
      settings_->options().pages().map_sign.mode() != MapSignMode::kNever
          ? pieces_.map_sign.Stage(
                pieces_.game_images.has_value() ? &*pieces_.game_images : nullptr, now_ms)
          : std::nullopt;
  // L'EFFET DE CHAPEAU DU PANTIN SURVOLÉ. ⚠ APRÈS LES DEUX PANTINS, composés
  // plus haut, qui effacent chacun le rappel de l'image d'avant ; un seul se
  // survole à la fois, et la boutique passe d'abord.
  if (pieces_.doll_effects.has_value()) {
    const ro::sim::HatEffectItems& hats = pieces_.cash_shop.hat_effects();
    if (windows_.cash_shop.state.doll.wanted) {
      pieces_.doll_effects->Advance(
          windows_.cash_shop.state.doll, hats.OrdinalOf(windows_.cash_shop.state.doll_item),
          pieces_.cash_shop.doll_framing(), pieces_.cash_shop.doll_body(), world_, *stage_, now_ms);
    } else if (windows_.description.open && windows_.description.doll.wanted) {
      pieces_.doll_effects->Advance(windows_.description.doll,
                                    hats.OrdinalOf(windows_.description.item_id),
                                    item_description_->doll_framing(),
                                    item_description_->doll_body(), world_, *stage_, now_ms);
    } else {
      pieces_.doll_effects->Rest();
    }
  }
  ComposeMenuIcons(windows_.menu_icons.icons,
                   pieces_.game_images.has_value() ? &*pieces_.game_images : nullptr,
                   settings_->bindings(),
                   MenuIconFactsOf(world_, windows_, ro::vfs::CashShopName(pieces_.item_words)));
  // LES CADRES D'ESSAI, seulement quand leur page est sous les yeux : une
  // vignette animée se recompose à chaque image. Celui des entités, et celui des
  // dégâts — sur les onglets de l'allure, des trajectoires et des couleurs.
  const bool entity_frame =
      windows_.options.open && settings_->options().entity_bars().tuning &&
      windows_.options.section == static_cast<int>(Rank(OptionSection::kEntityBars));
  const bool path_frame =
      windows_.options.open &&
      windows_.options.section == static_cast<int>(Rank(OptionSection::kDamage)) &&
      windows_.options.section_tab != static_cast<int>(DamageTab::kShown);
  if (entity_frame || path_frame) {
    const auto width = static_cast<float>(shell_.width());
    const auto height = static_cast<float>(shell_.height());
    const float anchor =
        ro::render::ScreenAnchorScale(height, ro::render::ApparentScale(camera_), 1.0F);
    // L'ÉCHELLE DU SPRITE SE MESURE SUR LE PERSONNAGE DU JOUEUR, tel que la
    // carte le dessine ; faute de lui, celle d'ancrage. Ses pieds servent de sol
    // au cadre des trajectoires.
    float sprite_scale = anchor;
    glm::vec3 player_ground = camera_.look_at;
    for (std::size_t rank = 0; rank < scene_actors_.ids.size() && rank < scene_actors_.drawn.size();
         ++rank) {
      if (scene_actors_.ids[rank] == world_.player_id()) {
        sprite_scale = ro::render::ActorSpriteScreenScale(camera_, aspect_ratio_,
                                                          scene_actors_.drawn[rank], width, height)
                           .value_or(anchor);
        player_ground = scene_actors_.drawn[rank].ground_position;
      }
    }
    if (path_frame) {
      const HitPopupLook& look = settings_->options().damage_look();
      const ro::sim::Actor* player = world_.player();
      DamagePathStage stage;
      stage.target = pieces_.entity_sampler.Compose(
          pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr, &styles_,
          PathTargetShown(DamagePathOf(look.path_shown)), world_, sprite_scale, anchor, now_ms);
      stage.sprite_scale = sprite_scale;
      stage.ground = SampleGroundOf(
          camera_, aspect_ratio_, width, height, player_ground, sprite_scale,
          player != nullptr ? ClientBodyDegrees(*player, ro::sim::FacingAt(*player, now_ms))
                            : 0.0F);
      stage.font = interface_->DamageFont(look.font_name, look.font_bold);
      settings_->options().pages().damage_path_sample =
          pieces_.damage_path_sampler.Compose(pieces_.numbers, look, std::move(stage), now_ms);
    }
    if (entity_frame) {
      settings_->options().pages().entity_sample = pieces_.entity_sampler.Compose(
          pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr, &styles_,
          EntityPageDrawn(EntityPageTabOf(windows_.options.section_tab)), world_, sprite_scale,
          anchor, now_ms);
      // L'EMBLÈME DE SA GUILDE, pour le joueur et ceux qui lui ressemblent : le
      // cadre montre son personnage à lui.
      ro::ui::EntitySample& sample = settings_->options().pages().entity_sample;
      if (guild_emblems_.has_value() && guild_book_.in_guild() && !sample.banner.empty() &&
          sample.name == world_.player_name()) {
        sample.emblem = guild_emblems_->Of(guild_book_.belonging()->guild_id);
      }
    }
  }
  // LA PAGE « COMMANDS SETTINGS », seulement quand elle est sous les yeux : sa
  // liste nomme chaque item par la table.
  const bool commands_shown =
      windows_.options.open &&
      windows_.options.section == static_cast<int>(Rank(OptionSection::kCommands));
  pieces_.commands.NoteDescribed(windows_.description.open ? windows_.description.item_id : 0);
  if (commands_shown) {
    pieces_.commands.Compose(
        item_description_.has_value() ? &item_description_->facts() : nullptr,
        pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
        pieces_.item_words);
  }
  // LE MODE RÉGLAGE DES JAUGES : des valeurs factices, panneau ouvert seulement.
  const bool tuning = settings_->options().stat_bars().tuning && windows_.options.open;
  const std::int64_t zeny_reference = settings_->options().stat_bars().zeny_reference;
  // Les onglets des compagnons, et le profil d'IA de chacun.
  ro::ui::CompanionTabsView companion_tabs{.homunculus = pieces_.homunculus.view(),
                                           .mercenary = pieces_.mercenary.view()};
  companion_tabs.homunculus.ai_profile =
      pieces_.companion_ai.ProfileLabel(ro::sim::AiCompanion::kHomunculus);
  companion_tabs.mercenary.ai_profile =
      pieces_.companion_ai.ProfileLabel(ro::sim::AiCompanion::kMercenary);
  companion_tabs.homunculus.ai_proposal =
      pieces_.companion_ai.ProposalText(ro::sim::AiCompanion::kHomunculus);
  companion_tabs.mercenary.ai_proposal =
      pieces_.companion_ai.ProposalText(ro::sim::AiCompanion::kMercenary);
  // ⚠ RELU À CHAQUE IMAGE, comme celui de la fiche : il montre la section
  // « Staff » de l'onglet de l'homoncule, et l'identifiant au survol d'une
  // compétence des deux compagnons.
  companion_tabs.homunculus.staff_allowed = Rights().staff_tools;
  companion_tabs.mercenary.staff_allowed = Rights().staff_tools;
  RelabelEmojiGroups(emojis_);
  ListUnlockedHudShown(settings_->options(), windows_, windows_.unlocked_hud);
  interface_->SetSkinLook(settings_->options().pages().skin_look.look());
  const UiStage::Answers answers = interface_->Draw(
      frame_tally_, windows_, talk, chat_preview_.preview(), *chat_tabs_, input_links_, emotions_,
      emojis_, sheet_->groups(), skill_points,
      UiStage::CharacterSheetView{.header = character_->header(),
                                  .slots = character_->slots(),
                                  .stats = character_->stats(),
                                  .presets = presets_->view(),
                                  .titles = title_view_->view(),
                                  .guild = guild_panel_->view(),
                                  .companions = companion_tabs,
                                  .guild_invitation = guild_panel_->invitation(),
                                  .guild_alliance = guild_book_.alliance_offer().has_value()
                                                        ? guild_book_.alliance_offer()->guild_name
                                                        : std::string_view()},
      style_view_->view(), style_preview_->view(), bag_->cells(), bag_->tally(), cart_->cells(),
      cart_->tally(), storage_->cells(), storage_->tally(), BankView::Of(world_),
      settings_->hotkey_view(), shortcuts_->slots(), settings_->options().shortcut_bars(),
      status_view_->cells(), settings_->options().status_bar(),
      tuning ? TuningStatBarValues(now_ms, zeny_reference)
             : StatBarValuesOf(
                   world_, zeny_reference,
                   OwnCastGauge(world_,
                                pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
                                now_ms)),
      tuning ? TuningStatBarWarnings(world_) : StatBarWarningsOf(world_),
      settings_->options().stat_bars(), settings_->options().pvp_hud(),
      settings_->options().sections(), item_description_->view(), description_preview_->view(),
      skill_description_->view(), settings_->options().align_grid(), chat_bubbles_,
      settings_->options().bubbles(), entity_bars_, settings_->options().entity_bars(), cast_bars_,
      settings_->options().pages().cast_bars.look(),
      ro::ui::DamageTexts{.texts = pieces_.numbers.has_value() && link_.is_open()
                                       ? pieces_.numbers->texts_on_screen()
                                       : std::span<const ro::ui::DamageTextView>(),
                          .font_name = settings_->options().damage_look().font_name,
                          .bold = settings_->options().damage_look().font_bold},
      Rights(), MenuFacts(), settings_->macro_commands(),
      // ⚠ LE CADRE DE REPÉRAGE NE SE MONTRE QUE SUR SA PROPRE SECTION, et non
      // dès que le panneau est ouvert : c'est là qu'on règle la zone. Posé sur
      // les treize autres, il barre l'écran de quelqu'un venu changer son
      // volume — et rien à l'écran ne dit d'où vient ce rectangle.
      obtained, settings_->options().item_obtain(), toast_grip_,
      recorder_.view(windows_.options.open &&
                     windows_.options.section == static_cast<int>(Rank(OptionSection::kCapture))),
      world_.server_clock().round_trip_ms(), pieces_.over_world.Paint());
  // 🔴 LA LIGNE VALIDÉE D'ABORD, avant tout ce qui écrit dans la saisie — un
  // Maj+clic, un menu, le carnet : voir `SettleChatLine`.
  SendTypedChat(answers, now_ms);
  // ⚠ UNE JAUGE DÉPLACÉE S'ÉCRIT, MAIS PAS À CHAQUE IMAGE. `Touch` arme
  // l'anti-rebond du fichier : un glisser de deux secondes ne produit qu'une
  // écriture, et celle qui attend part au plus tard à la fermeture.
  if (answers.stat_bars_moved || answers.pvp_hud_moved || answers.menu_icons_moved) {
    settings_->file().Touch();
  }
  if (pieces_.damage_meter.Apply(answers.damage_meter, world_,
                                 settings_->options().pages().damage_meter)) {
    settings_->file().Touch();
  }
  if (answers.damage_meter.open_settings) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kDamageMeter);
  }
  if (answers.damage_meter.dock.has_value()) {
    chat_tabs_->DockMeter(answers.damage_meter.dock->group, answers.damage_meter.dock->rank);
  }
  // ⚠ MÊME RÈGLE POUR LA POIGNÉE DU BANDEAU : l'anti-rebond du fichier fait
  // qu'un glisser de deux secondes ne produit qu'une écriture.
  if (answers.obtained.moved) {
    settings_->NoteItemObtainAnchor(answers.obtained.pos_x, answers.obtained.pos_y);
  }
  // ⚠ APRÈS LE DESSIN : c'est là que l'établi a coché sa case et posé sa
  // couleur, et c'est la remontée de ce que `Refresh` lui avait descendu.
  settings_->NoteStaffTools(windows_);
  // ⚠ ET LA CASE DE L'EN-TÊTE DU GRIMOIRE, pour la même raison et dans le même
  // sens : elle vient d'être cochée par le dessin, au-dessus.
  settings_->NoteGrimoireIcons(windows_);
  // ⚠ ET LE VOLET DU STAFF, qui n'est PAS une case mais une LARGEUR : le joueur
  // l'ouvre en tirant la poignée de la fenêtre, et ce relevé est le seul moyen
  // de le savoir. Muet sans le droit — voir `NoteCharacterStaffPane`.
  settings_->NoteCharacterStaffPane(windows_);
  // « Ignoré des monstres » : un souhait, retenu ici et que la mise en scène de
  // la fiche fait valoir auprès du serveur. Voir `KeepBattleIgnore`.
  if (answers.character.staff.order == ro::ui::StaffOrder::kSetBattleIgnore) {
    settings_->NoteStaffBattleIgnore(answers.character.staff.amount != 0);
  }
  // ⚠ ET LA POSE DE L'AVATAR, dans le même sens : la molette vient de la
  // tourner, la liste vient peut-être d'en changer.
  settings_->NoteCharacterAvatar(windows_);
  // ── CE QUE LA FENÊTRE DES MACROS A DEMANDÉ ────────────────────────────────
  ApplyMacroAnswers(answers.macros, now_ms);
  // ── ET CE QUE LA CAPTURE A DEMANDÉ ────────────────────────────────────────
  ApplyCaptureGesture(answers, now_ms);
  sheet_->Apply(answers.character.grimoire, grimoire_, link_, world_, now_ms);
  character_->Apply(answers.character, link_, world_);
  ApplyPresetAnswers(answers);
  title_view_->Apply(
      answers.character,
      windows_.character.open && windows_.character.tab == ro::ui::CharacterTab::kTitles, link_,
      world_);
  ApplyGuildAnswers(answers);
  // MAJ + CLIC SUR UNE ENTRÉE OU UN ONGLET : son lien dans la saisie du chat.
  // ⚠ AVANT LE GROUPE, qui vide sa réponse en l'appliquant.
  if (const SettingDestination* const linked = AskedSettingLink(answers, windows_);
      linked != nullptr) {
    static_cast<void>(PlaceInChatInput(SettingPendingLink(*linked)));
  }
  ApplyPartyAnswers(now_ms);
  ApplyStyleAnswers(answers, now_ms);
  SendStaffOrder(answers.character.staff, link_, world_);
  ApplyContainerAnswers(answers, now_ms);
  PostClickedItemLink(answers);
  ApplyDescriptionAnswers(answers, now_ms);
  ApplyCardInsert(answers, now_ms);
  FollowCraftGestures(
      pieces_.craft.Apply(answers.make_item, answers.weapon_refine, windows_.make_item,
                          windows_.weapon_refine, settings_->options().pages().make_item.settings(),
                          settings_->options().pages().weapon_refine.settings(), link_, world_,
                          now_ms),
      now_ms);
  ApplyWarpList(answers.warp_list, link_, world_);
  pieces_.cart_choice.Apply(answers.cart_choice, link_);
  pieces_.spell_list.Apply(answers.spell_list, link_);
  pieces_.repair.Apply(answers.repair, link_);
  pieces_.pet.Apply(answers.pet, answers.pet_hatch, windows_.pet, link_, world_, now_ms);
  // LA FICHE DU PET suit sa page de réglages, et sa puce y mène.
  const PetSettings& pet_page = settings_->options().pages().pet.settings();
  windows_.pet.confirm_return_to_egg = pet_page.confirm_return_to_egg;
  windows_.pet.animate_sprite = pet_page.animate_sprite;
  if (answers.pet.settings_asked) {
    GameSettings::OpenOptionsAt(windows_, OptionSection::kPet);
  }
  // Les matériaux et les espèces du panneau d'évolution, les œufs de l'éclosion :
  // des items de catalogue, avec les gestes de partout.
  FollowCatalogItem(answers.pet.link, now_ms);
  FollowCatalogItem(answers.pet_hatch.link, now_ms);
  pieces_.homunculus.Apply(answers.character.homunculus, link_, world_, now_ms,
                           windows_.skill_description);
  pieces_.mercenary.Apply(answers.character.mercenary, link_, world_, now_ms,
                          windows_.skill_description);
  // LA FAIM DU PET ET DE L'HOMONCULE, au palier où ils perdent de l'intimité :
  // dans le bandeau des ramassages, que le joueur a déjà placé et réglé.
  for (std::optional<ToastNotice> hungry :
       {pieces_.pet.TakeHungerNotice(), pieces_.homunculus.TakeHungerNotice()}) {
    if (hungry.has_value() && pieces_.item_toast.has_value()) {
      pieces_.item_toast->Announce(std::move(*hungry), now_ms);
    }
  }
  ReportRefusal(pieces_.talkbox.Apply(answers.talkbox, link_));
  pieces_.companion_ai.ApplyTacticWindow(answers.companion_tactic, now_ms);
  // « Régler… » dans l'onglet d'un compagnon, puis ce que la fenêtre de l'IA a
  // demandé.
  if (answers.character.homunculus.verb == ro::ui::HomunculusVerb::kOpenAi) {
    pieces_.companion_ai.OpenAiWindow(ro::sim::AiCompanion::kHomunculus, windows_.companion_ai);
  }
  if (answers.character.mercenary.open_ai) {
    pieces_.companion_ai.OpenAiWindow(ro::sim::AiCompanion::kMercenary, windows_.companion_ai);
  }
  // LA PROPOSITION DE RECOPIE, répondue depuis l'onglet.
  if (const ro::ui::HomunculusVerb verb = answers.character.homunculus.verb;
      verb == ro::ui::HomunculusVerb::kAcceptAiProposal ||
      verb == ro::ui::HomunculusVerb::kDeclineAiProposal) {
    pieces_.companion_ai.AnswerProposal(ro::sim::AiCompanion::kHomunculus,
                                        verb == ro::ui::HomunculusVerb::kAcceptAiProposal);
  }
  if (answers.character.mercenary.ai_proposal_answer.has_value()) {
    pieces_.companion_ai.AnswerProposal(ro::sim::AiCompanion::kMercenary,
                                        *answers.character.mercenary.ai_proposal_answer);
  }
  pieces_.companion_ai.ApplyAiWindow(answers.companion_ai, windows_.companion_ai, now_ms);
  ServeDebugConsole(pieces_.debug_console, windows_.staff_tools, Rights().staff_tools, world_,
                    frame_tally_, pieces_.companion_ai, now_ms);
  pieces_.npc_illustration.Apply(answers.npc_illustration);
  ApplyContainerSwitches(answers, now_ms);
  ApplyBankAnswers(answers, now_ms);
  if (commands_shown) {
    pieces_.commands.Apply(link_,
                           item_description_.has_value() ? &item_description_->facts() : nullptr);
  }
  shortcuts_->Apply(answers.shortcuts, grimoire_, link_, world_, now_ms);
  settings_->Apply(answers, OptionsTargets(), windows_, &running_, now_ms);
  if (answers.lock_hud) {
    LockWholeHud(*settings_, OptionsTargets(), now_ms);
  }
  if (answers.hide_unlock_notice) {
    settings_->SetSwitch(GameOption::kUnlockNotice, false, OptionsTargets(), now_ms);
  }
  // LA CASE DE L'APPEL D'URGENCE DEMANDE, ET C'EST TOUT : la case suit l'écho.
  if (const std::optional<bool> callable = settings_->options().pages().call.TakeRequest();
      callable.has_value()) {
    ReportRefusal(ro::net::RequestCallable(link_, *callable));
  }
  ApplyMenuGesture(answers.menu, now_ms);
  if (const std::optional<ro::ui::MenuCommand> clicked =
          std::exchange(windows_.menu_icons.clicked, std::nullopt);
      clicked.has_value()) {
    ApplyMenuCommand(*clicked, MenuIconTargets{.windows = &windows_,
                                               .bank = &bank_,
                                               .card_album = &card_album_,
                                               .cash_shop = &pieces_.cash_shop,
                                               .mail = &pieces_.mail,
                                               .achievements = &pieces_.achievements,
                                               .quests = &pieces_.quests,
                                               .mvp_tracker = &pieces_.mvp_tracker,
                                               .battleground = &pieces_.battleground,
                                               .link = &link_,
                                               .world = &world_,
                                               .now_ms = now_ms});
  }
  settings_->TickDisplayRevert(OptionsTargets(), now_ms);
  SendNpcAnswer(answers.npc, link_, world_);
  // ⚠ APRÈS LA RÉPONSE, ET À PART D'ELLE : cliquer un lien dans une page n'est
  // pas répondre au PNJ, et les deux peuvent arriver dans la même image.
  FollowNpcDialogLink(answers.npc, now_ms);
  // ⚠ RETENU POUR L'IMAGE SUIVANTE : l'aperçu se compose avant le dessin, donc
  // avec ce que la réponse PRÉCÉDENTE a rapporté. Un aperçu qui attendrait le
  // clic ne serait plus un aperçu.
  hovered_status_ =
      answers.npc.hovered.kind == ro::ui::DialogTargetKind::kStatus &&
              answers.npc.hovered.id <= std::numeric_limits<ro::sim::StatusEffectId>::max()
          ? static_cast<ro::sim::StatusEffectId>(answers.npc.hovered.id)
          : 0;
  ApplyChatAnswers(answers, now_ms);

  PaintCursor();
  // Les réglages APRÈS le dessin : c'est en dessinant qu'ImGui apprend qu'une
  // fenêtre a bougé. L'écriture n'a lieu que si quelque chose a changé.
  interface_->SaveSettings();
  shell_.layouts().Tick(now_ms);
  settings_->profiles().Tick(OptionsTargets(), now_ms);
  // ⚠ ET CELLE DES OPTIONS ATTEND que le joueur s'arrête : un curseur qu'on
  // fait glisser rend « changé » à chaque image. Voir l'anti-rebond de
  // `SettingsFile`.
  // ⚠ AVANT LE BATTEMENT DU FICHIER : c'est lui qui écrit, et il doit voir dans
  // la même image ce que la fenêtre vient de changer.
  settings_->NoteWindowPlace(OptionsTargets());
  settings_->Tick(now_ms);
}

// ET LE CURSEUR APRÈS TOUT, y compris après les fenêtres. Il n'y en a qu'un dans
// ce client, et c'est celui-ci.
void Game::PaintCursor() {
  if (cursor_quads_.empty()) {
    return;
  }
  pieces_.overlay.Draw(cursor_quads_, static_cast<float>(shell_.width()),
                       static_cast<float>(shell_.height()));
  cursor_quads_.clear();
}

}  // namespace moonclient
