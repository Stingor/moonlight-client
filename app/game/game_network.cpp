// CE QUI ARRIVE DU SERVEUR, et ce que son arrivée met en mouvement.
//
// ⚠ CETTE ÉTAPE NE FAIT PAS QUE LIRE. Une trame appliquée change le monde, et
// plusieurs choses ne peuvent se décider qu'ENSUITE : la carte annoncée qu'il
// faut charger, la caméra qui suit, l'objet qu'on ramasse en arrivant dessus,
// les nombres que les coups font naître. Les séparer du drain les ferait
// travailler sur l'état d'avant.
//
// ⚠ ET CE QUI FINIT DE LUI-MÊME N'ARRIVE PAR AUCUNE TRAME : un état échu, une
// page de dialogue dont le paquet terminal ne vient pas. Ceux-là se mesurent au
// temps, et c'est ici qu'on les balaie — une fois le drain fini.

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <SDL3/SDL.h>

#include "ro/core/log.h"
#include "ro/net/action.h"
#include "ro/net/attack_reach.h"
#include "ro/net/link_loss.h"
#include "ro/net/movement.h"
#include "ro/net/npc_dialog.h"
#include "ro/net/progress_bar.h"
#include "ro/net/refusal_text.h"
#include "ro/net/world_notice.h"
#include "ro/net/world_update.h"
#include "ro/render/gl_context.h"
#include "ro/render/map_effect_math.h"
#include "ro/sim/damage_meter.h"
#include "ro/sim/movement.h"
#include "ro/sim/server_clock.h"
#include "ro/sim/world.h"

#include "game/bug_report_context.h"
#include "game/connected_session.h"
#include "game/game.h"
#include "game/handshake_pump.h"
#include "game/interface_sound.h"
#include "game/level_up_cue.h"
#include "input/camera_aim.h"
#include "loading/loading_plans.h"
#include "loading/loading_presenter.h"
#include "world/attack_intent.h"
#include "world/click_action.h"
#include "world/follow_action.h"
#include "world/item_broadcast_report.h"
#include "world/monster_auras.h"
#include "world/scene_graphics.h"

namespace moonclient {

void Game::FollowGmOutfit() {
  windows_.staff_tools.admin = Rights().staff_tools && world_.group_level() >= kAdminMinGroupLevel;
  const bool worn = ro::ui::GmOutfitWorn(windows_.staff_tools);
  // 🔴 LE NÔTRE SUIT LA CASE, ET NON L'ÉCHO DU SERVEUR : il se voit dès le clic,
  // et chaque réapparition le pose de nouveau.
  world_.DressPlayerAsGameMaster(worn);
  if (link_.is_open()) {
    pieces_.gm_outfit.Pump(link_,
                           handshake_.state() == ro::net::MoonclientHandshakeState::kVerified,
                           windows_.staff_tools.admin, worn);
  }
}

ro::core::Result<void> Game::PumpNetwork(std::uint32_t now_ms) {
  // Le réseau AVANT le dessin : une image doit montrer l'état le plus récent,
  // pas celui d'avant la lecture.
  // 🔴 MAIS LA FLÈCHE DU JEU DÉCIDE AVANT LUI : voir `DecideSelectionMarker`.
  DecideSelectionMarker(now_ms);
  // LA BOÎTE DE PERTE A-T-ELLE EU SA RÉPONSE ? Voir `game_link_loss.cpp`.
  FollowLinkLoss();
  if (moving_.has_value()) {
    AdvanceZoneMove(now_ms);
  }
  if (!link_.is_open()) {
    // ⚠ SANS LIAISON, PAS DE SILENCE À MESURER — un changement de serveur de
    // zone en cours, qui a son propre délai (`ro::net::ZoneMoveOverdue`), ou
    // une perte déjà dite. La garde repart de zéro avec la liaison suivante.
    link_watch_.Restart(now_ms);
    windows_.link_notice.unsteady = false;
    return {};
  }
  // ⚠ AVANT LA LECTURE : c'est l'écart avec l'image précédente qui dit si la
  // boucle a gelé, et un gel n'est pas un silence. Voir `ro::net::LinkWatch`.
  link_watch_.NoteLoop(now_ms);
  ro::net::Connection::Polled polled = link_.Poll();
  // LA CIBLE D'UN REFUS DE PORTÉE, s'il en arrive un. ⚠ RELEVÉE ICI ET AGIE
  // PLUS BAS : la poursuite a besoin des positions que la trame vient de
  // recaler, et reprendre depuis la boucle la ferait partir de l'état d'avant.
  std::optional<std::uint32_t> refused_target;
  // LE MOTIF D'UNE ÉVICTION, s'il en arrive une : la perte se déclare après le
  // drain, une fois la ligne écrite dans la conversation.
  std::optional<std::uint8_t> evicted;
  // LE SERVEUR QUI PASSE LA MAIN, s'il le fait. ⚠ CE QUI SUIT SA TRAME SUR
  // L'ANCIENNE LIAISON NE SE LIT PAS : ce serveur ne sert plus le personnage.
  std::optional<ro::net::CharacterPhase::Handoff> server_move;
  for (const ro::net::Frame& received : polled.frames) {
    // LE RETOUR À LA SÉLECTION ACCORDÉ : ce serveur ne sert plus le personnage,
    // et ce qui suit sur sa liaison ne se lit pas.
    if (FollowCharacterSelect(received, now_ms)) {
      break;
    }
    // 🔴 CE QU'ON DEVRA DIRE SI CETTE TRAME-CI CHANGE LA CARTE, lu AVANT de
    // l'appliquer : le monde ferme alors la fenêtre du PNJ, et après coup il n'y
    // a plus rien à interroger. Le script, lui, reste attaché au personnage —
    // sans ce mot, plus aucun PNJ ne répond jusqu'à ce que le serveur le délie
    // de lui-même, une minute plus tard.
    const std::uint32_t entries_before = world_.map_entries();
    const std::optional<ro::net::NpcFarewell> farewell = ro::net::NpcFarewellDue(world_);
    // UN COUP SE JUGE AUSSI SUR L'ÉTAT D'AVANT : voir `PetStage::NoteBeforeWorld`.
    const auto rests = [&](const ro::sim::Actor& actor) {
      return ActorRestsAt(pieces_.wardrobe ? &*pieces_.wardrobe : nullptr, world_, planner_, actor,
                          now_ms);
    };
    pieces_.pet.NoteBeforeWorld(received, world_, rests, now_ms);
    // 🔴 EN SALON, LA PAROLE REVIENT AU SALON, et le monde ne l'écrit pas : le
    // serveur ne nous fait plus entendre la carte.
    ro::net::FrameOutcome outcome = pieces_.chat_rooms.ClaimLine(received, world_, now_ms);
    // LES CHUCHOTEMENTS AVANT LE MONDE : eux seuls savent qui sont nos amis et
    // ce qui attend un accusé. La parole masquée aussi.
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = pieces_.whispers.ApplyFrame(received, world_, friends_, guild_book_, now_ms);
    }
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = ro::net::ApplyFrame(received, world_, now_ms);
    }
    // LA MONTÉE DE NIVEAU DU JOUEUR : son effet, et son son. Voir `CueLevelUp`.
    if (const std::optional<std::string_view> sound = CueLevelUp(received, world_, now_ms);
        sound.has_value() && vfs_ != nullptr) {
      const InterfaceSoundFile file{.place = InterfaceSoundPlace::kSoundArchive,
                                    .path = std::string(*sound)};
      if (const ro::core::Result<void> played =
              PlayInterfaceSound(audio_, *vfs_, client_root_, file);
          !played) {
        ro::core::LogWarning("son de montée de niveau : {}", ro::core::Describe(played.error()));
      }
    }
    // 🔴 LES REPÈRES ET LES POSITIONS DE LA GUILDE SE VIDENT ICI, trame par
    // trame, et non après la boucle : le serveur redit ceux de la nouvelle carte
    // dans la foulée du changement, souvent dans la même lecture, et les vider
    // plus tard effacerait ce qu'il vient de dire.
    if (world_.map_entries() != entries_before) {
      navi_.guidance.viewpoints.Clear();
      navi_.guidance.boss.reset();
      guild_book_.ForgetMemberCells();
    }
    // CE QUI VIT HORS DU MONDE lit la trame à son tour : voir
    // `ApplyFrameBeyondWorld`.
    outcome = ApplyFrameBeyondWorld(received, outcome, now_ms);
    if (world_.map_entries() != entries_before && farewell.has_value()) {
      ReportRefusal(ro::net::SayNpcFarewell(link_, *farewell));
    }
    if (const std::optional<std::uint32_t> too_far = ro::net::TargetTooFar(received);
        too_far.has_value()) {
      refused_target = too_far;
    }
    if (const std::optional<std::uint8_t> code = ro::net::EvictionOf(received); code.has_value()) {
      evicted = code;
    }
    frame_tally_.Record(received, outcome);
    handshake_.NoteFrame(received.opcode);
    if (outcome == ro::net::FrameOutcome::kUndecodable &&
        reported_undecodable_.insert(received.opcode).second) {
      ro::core::LogWarning("trame indéchiffrable : {:#06x}, {} octet(s)", received.opcode,
                           received.bytes.size());
    }
    if (outcome == ro::net::FrameOutcome::kUnhandled &&
        reported_unhandled_.insert(received.opcode).second) {
      ro::core::LogInfo("trame non traitée : {:#06x}, {} octet(s)", received.opcode,
                        received.bytes.size());
    }
    server_move = ro::net::ServerMoveOf(received, ticket_.character_id);
    if (server_move.has_value()) {
      break;
    }
  }
  if (evicted.has_value()) {
    // 🔴 UNE ÉVICTION EST UNE PERTE DE LIAISON QUI DIT SON MOTIF : le serveur a
    // déjà coupé, et rester sur la carte laisserait le joueur devant un monde
    // figé. Même boîte, même retour à l'écran de connexion que toute perte —
    // dessinée DANS le jeu, donc visible en plein écran comme en fenêtre.
    //
    // ⚠ SANS LIGNE DE PLUS : le monde a écrit la sienne en lisant le paquet.
    ro::core::LogError("session close par le serveur (code {}) : {}", *evicted,
                       ro::net::BanText(*evicted));
    DeclareLinkLoss(ro::net::EvictionText(*evicted), false, now_ms);
    return {};
  }
  if (server_move.has_value()) {
    // ⚠ AVANT LE CONSTAT DE PERTE : le serveur quitté ferme volontiers la
    // liaison juste après, et ce n'est pas une panne.
    StartZoneMove(*server_move, now_ms);
  } else if (back_to_characters_) {
    // ⚠ PAS UNE PERTE : le serveur ferme volontiers la liaison après avoir
    // rendu la main, et la partie s'arrête de toute façon.
    return {};
  } else if (polled.status != ro::net::LinkStatus::kOpen) {
    // LE DÉTAIL TECHNIQUE AU JOURNAL, ce qu'il veut dire au joueur.
    ro::core::LogError("liaison perdue : {}", DescribeLinkLoss(polled.status, link_.error()));
    DeclareLinkLoss(ro::net::LinkLossText(ro::net::LinkLossCauseOf(polled.status)), true, now_ms);
  } else {
    // ET LE SILENCE, que rien ne ferme : voir `ro::net::LinkWatch`.
    WatchLinkSilence(!polled.frames.empty(), now_ms);
  }

  ExpireCharacterSelect(now_ms);

  // Les repères éphémères qui ont fait leur temps.
  navi_.guidance.viewpoints.PurgeExpired(now_ms);

  // CE QUE LE SERVEUR A RÉPONDU au groupe et aux amis, dans la conversation.
  SayPartyNotices(party_, friends_, world_, now_ms);

  // CE QUE LA DERNIÈRE IMAGE A CONSTATÉ, rendu au monde avant la suivante : les
  // corps éteints s'en vont, les effets annulés aussi. Voir `SettleWorld`.
  SettleWorld(world_, scene_actors_, now_ms);
  // LES RESTES HORS DE LA VUE DU SERVEUR, que rien n'a retirés : dits au journal,
  // pour qu'on retrouve un jour la trame qui manquait.
  for (const ro::sim::PurgedActor& gone :
       sight_purge_.Sweep(world_, world_.session().view_cells, now_ms)) {
    ro::core::LogInfo("acteur {} (classe {}) purgé, {} : à ({}, {}) cases", gone.actor_id, gone.job,
                      gone.reason == ro::sim::PurgeReason::kZeroHealth
                          ? "debout à zéro PV"
                          : "hors de la vue du serveur",
                      gone.dx_cells, gone.dy_cells);
  }
  // LES EFFETS QUE LES DÉPARTS LAISSENT, là où l'acteur était : il n'est déjà
  // plus là pour les porter.
  for (const ro::sim::Departure& left : world_.departures().Take()) {
    if (effects_.has_value() && stage_.has_value()) {
      const std::array<float, 3> ground = GroundPositionAt(*stage_, left.cell.x, left.cell.y);
      static_cast<void>(
          effects_->PlayById(left.effect_id, glm::vec3(ground[0], ground[1], ground[2]), now_ms));
    }
  }
  // ET CEUX QUI SORTENT DE VUE, dont une copie s'efface encore à l'écran.
  for (ro::sim::FadingActor& leaving : world_.departures().TakeFading()) {
    scene_actors_.fading.push_back(std::move(leaving));
  }
  // LES EFFETS D'OFFICE de qui vient de paraître ou de se cacher.
  DressMonsters(world_,
                pieces_.wardrobe.has_value() ? pieces_.wardrobe->monster_size_effects() : nullptr,
                now_ms);

  // ⚠ CE QUI FINIT DE LUI-MÊME N'EST ANNONCÉ PAR PERSONNE : un état dont la
  // durée s'achève ne vaut aucun paquet, et sans ce balayage son icône
  // resterait jusqu'au suivant, quel qu'il soit.
  world_.ExpirePlayerStatuses(now_ms);
  // ET LA BARRE DE PROGRESSION DU JOUEUR, échue ou annulée : le script attend.
  ReportRefusal(ro::net::ReportProgressDone(link_, world_, pieces_.progress, now_ms));

  // LE FILET DE LA PAGE DE DIALOGUE, pour la même raison : une page dont le
  // paquet terminal n'arrive jamais — un script qui écrit puis dort, ou qui
  // s'arrête sur `end` — attend sinon pour toujours. Il se mesure en SILENCE,
  // donc il ne peut se déclencher qu'ici, une fois le drain fini.
  static_cast<void>(world_.CommitNpcPageIfIdle(now_ms));

  // LA POIGNÉE DE MAIN D'INTÉGRITÉ, aussitôt la liaison ouverte, et ce qu'elle
  // débloque : voir `PumpHandshake`.
  if (link_.is_open()) {
    // Le bit du classement survivors n'y entre que si la table des longueurs
    // connaît ses ZC.
    PumpHandshake(link_, handshake_, reported_handshake_, pieces_,
                  settings_->options().pages().mvp_tracker.settings().enabled,
                  survivors_ranking_.caps(), now_ms);
  }

  // L'ANNEAU DES DIX PREMIERS S'ACCORDE AU RANG DE CHACUN, à chaque battement.
  //
  // 🔴 ICI PLUTÔT QU'À L'ARRIVÉE DU PAQUET, et c'est ce qui le fait revenir : le
  // serveur ne redit un rang que lorsqu'il CHANGE, jamais à l'entrée en vue. Un
  // joueur qui meurt et revient, ou qui sort de la vue et y rentre, est un acteur
  // NEUF — sans cette passe il ne porterait plus jamais rien.
  //
  // ⚠ ELLE EST MUETTE TANT QUE RIEN NE BOUGE : voir `World::RefreshPvpRings`.
  world_.RefreshPvpRings(now_ms);

  // LE LANCEUR SUIT SA CIBLE DES YEUX pendant son incantation : le client le
  // retourne vers elle à chaque image, et aucun paquet ne le dit. Voir
  // `World::TrackCastTargets`.
  world_.TrackCastTargets(now_ms);

  // Le battement d'horloge. Sans lui, le serveur raccroche au bout d'une minute
  // sans rien recevoir, proprement et donc sans un mot.
  if (link_.is_open() && now_ms - last_tick_ms_ >= ro::net::kTickIntervalMs) {
    last_tick_ms_ = now_ms;
    // ⚠ AVANT L'ENVOI, et non après : ce qu'on mesure part d'ici.
    world_.server_clock().NoteTickSent(now_ms);
    ReportRefusal(ro::net::RequestTick(link_, now_ms));
  }

  // Le serveur a fait passer le monde sur une autre carte.
  if (world_.map_entries() != acknowledged_entries_) {
    acknowledged_entries_ = world_.map_entries();
    // Tout ce qui se voyait s'éteint, même carte ou non : voir `ForgetWhatPlayed`.
    ForgetWhatPlayed();
    // ET LA CAMÉRA SE REPOSE, au lieu de glisser jusqu'à la nouvelle place.
    // Toute arrivée passe ici, le warp de trois cases comme le changement de
    // carte, et aucune des deux n'est une marche : voir CameraTracking::Reset.
    aim_.tracking.Reset();
    // ⚠ LA SECOUSSE SE RETIRE AVANT QUE LA CAMÉRA SE REPOSE : laissée, elle
    // serait ensuite retranchée d'une pose qui ne la porte pas, et la pose
    // mémorisée de la carte quittée la garderait.
    aim_.shake.Undo(camera_);
    // 🔴 ET LA VEILLE S'ARRÊTE NET, sans sa rampe : dérouler deux secondes
    // d'animation sur une carte qu'on est en train de remplacer n'aurait aucun
    // sens. ⚠ AVANT le chargement, tant que la pose de repos a encore un sens à
    // remettre — celui-ci réécrit la caméra.
    // ⚠ ET L'ANNONCE D'ABSENCE N'Y TOUCHE PAS : la session survit au changement
    // de carte, donc le miroir aussi. Voir `AfkScreen::ForgetAnnounce`.
    afk_screen_.EndNow(afk_idle_, camera_);
    // 🔴 LES SONS DE LA CARTE QUITTÉE SE TAISENT AVANT LE CHARGEMENT, qui dure :
    // voir `ActorSounds::LeaveMapAmbience`. ⚠ Seulement quand une AUTRE carte
    // se charge — c'est la garde même d'`EnterAnnouncedMap`.
    if (world_.map_name() != attempted_map_ && actor_sounds_.has_value()) {
      actor_sounds_->LeaveMapAmbience(audio_);
    }
    const std::string left_map = stage_->name;
    const bool flatten =
        ro::render::GreyWorldFlattens(settings_->options().pages().grey_world.look());
    const bool rounded = settings_->options().graphics().rounded_ground();
    // L'ÉCRAN DE CHARGEMENT, POUR UNE AUTRE CARTE SEULEMENT : la même ne se
    // relit pas, et rien ne s'afficherait qu'un clignotement.
    std::optional<LoadingPresenter> loading;
    if (world_.map_name() != attempted_map_) {
      loading.emplace(shell_, MapPlan(rounded), "changement de carte");
    }
    const MapArrival arrival{&*vfs_,          &*stage_,
                             &planner_,       &world_,
                             &terrain_,       &water_,
                             &models_,        &background_,
                             &attempted_map_, flatten,
                             &map_edits_,     settings_->options().graphics().real_time_shadows(),
                             rounded,         loading.has_value() ? loading->progress() : nullptr};
    if (auto entered = EnterAnnouncedMap(arrival); !entered) {
      return std::unexpected(entered.error());
    }
    loading.reset();
    // Même carte comprise : toute entrée repose la vue d'arrivée.
    camera_rig_.EnterMap(camera_, stage_->name);
    // ⚠ LE CHARGEMENT N'EST PAS UN SILENCE : on ne lisait pas pendant qu'il
    // bloquait. La garde repart d'ici, à l'heure de sa FIN et non à celle de
    // l'image qui l'a commencé.
    link_watch_.Restart(static_cast<std::uint32_t>(SDL_GetTicks()));
    // 🔴 ET LE CIEL SE REDEMANDE. Ses nœuds vivent aussi longtemps que la carte,
    // donc ceux de la précédente survivraient au voyage — et ceux de la nouvelle
    // ne se lèveraient jamais, le drapeau étant resté posé.
    sky_raised_ = false;
    // ⚠ L'HORLOGE DES MODÈLES, ELLE, NE REPART QU'AVEC LA SCÈNE : un téléport sur
    // la même carte ne recharge rien, chez le natif comme ici, et ses fontaines
    // continuent de tourner au lieu de revenir à leur première image. Les
    // émetteurs de la carte suivent la même règle.
    if (stage_->name != left_map) {
      model_clock_ = ro::render::ModelClock();
      map_effects_raised_ = false;
    }
    // L'avis part MÊME si la carte n'a pas pu se charger : sans lui le serveur
    // laisse le personnage hors de la carte et n'écoute plus rien — ni la
    // liste de blocage rejouée. ⚠ SAUF PENDANT UN CHANGEMENT DE SERVEUR :
    // `MapPhase` l'envoie elle-même au nouveau, et l'ancien ne l'écoute plus.
    if (!moving_.has_value()) {
      if (auto told = ro::net::NotifyMapLoaded(link_); !told) {
        ro::core::LogError("avis de chargement refusé : {}", ro::core::Describe(told.error()));
      }
      pieces_.whispers.NoteMapEntered();
      pieces_.companion_ai.NoteArrival(static_cast<std::uint32_t>(SDL_GetTicks()));
    }
  }

  // La caméra suit le personnage, et tremble quand un effet le demande — si
  // `/quake` le permet.
  aim_.shake.enabled = settings_->options().graphics().screen_quake();
  AimCamera(camera_, (*stage_), world_, planner_, now_ms, aim_);
  // L'ÉDITEUR DE CARTE peut regarder ailleurs que le personnage : voir
  // `MapEditorSession::view_point`.
  if (const std::optional<std::array<float, 3>>& point = map_editor_.view_point();
      point.has_value() && map_editor_.active()) {
    const ro::render::MapEffectFrame frame{stage_->scene.gnd.width(), stage_->scene.gnd.height()};
    camera_.look_at = frame.ToWorld(glm::vec3((*point)[0], (*point)[1], (*point)[2]));
  }
  // LE FLOU DE MOUVEMENT suit la caméra une fois posée : voir `SpinBlurOfFrame`.
  spin_blur_frame_ =
      SpinBlurOfFrame(spin_meter_, settings_->options().graphics(), camera_,
                      aim_.shake.pitch_offset(), camera_rig_.TakeDragged(), now_ms, aspect_ratio_);

  // La cible visée se POURSUIT jusqu'à être à portée, puis se frappe. ⚠ APPELÉE
  // MÊME SANS POURSUITE EN COURS : le refus du serveur en rouvre une, et c'est
  // ce qui relance la course derrière une cible qui s'éloigne.
  //
  // ⚠ ET LE REFUS ÉCARTE LA COMPÉTENCE EN ATTENTE : le client n'a qu'UNE action
  // en attente, que ce refus réécrit. Deux poursuites vivantes demanderaient
  // deux marches par image, vers des cases différentes.
  //
  // ⚠ LE RÉARMEMENT REPREND LE TYPE DU DERNIER CLIC D'ACTION, et non l'état de
  // Ctrl à l'arrivée du refus : voir `RearmedAttackRepeats`.
  //
  // UNE ÉCHOPPE ATTEINTE se demande aux commerces, plus bas dans l'image.
  std::optional<StoreVisitDue> store_reached;
  if (AdvanceAttack(link_, world_, planner_, PlannedMapOf(*stage_), attack_,
                    grimoire_.approach_clock(), refused_target,
                    RearmedAttackRepeats(world_mouse_.engagement), now_ms, &targeting_,
                    &world_mouse_.engagement, &store_reached)) {
    grimoire_.CancelPending();
  }
  if (store_reached.has_value()) {
    pieces_.trade.VisitStore(store_reached->owner_id, store_reached->kind);
  }

  // L'ACTION EN ATTENTE AVANCE D'UN TOUR : on marche vers la cible désignée
  // tant qu'elle est hors de portée, et la compétence ne part qu'une fois
  // arrivé. Le client natif la relance au même endroit de son tour de boucle.
  // Voir `ro/sim/pending_action.h`.
  if (link_.is_open()) {
    grimoire_.AdvancePending(link_, world_, planner_, PlannedMapOf(*stage_), now_ms);
    // LE SUIVI D'UN JOUEUR, au même tour : voir `world/follow_rules.h`.
    TickFollow(world_mouse_.follow,
               {.link = &link_,
                .world = &world_,
                .planner = &planner_,
                .walk = PlannedMapOf(*stage_),
                .walk_clock = &grimoire_.approach_clock(),
                .deferred_walk = &world_mouse_.deferred_walk,
                .player_gesture = scene_actors_.player_gesture,
                .entity_menu = &windows_.entity_menu.state,
                .in_chat_room = pieces_.chat_rooms.room().in_room(),
                .left_held = (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0,
                .now_ms = now_ms});
  }
  // L'IA DES COMPAGNONS, après le changement de carte qu'elle doit voir.
  const CompanionAiBooks companion_books{
      .party = party_,
      .skill_tree = pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
      .monster_sheets = monster_sheet_.has_value() ? &monster_sheet_->book() : nullptr,
      .walk_map = stage_.has_value() ? PlannedMapOf(*stage_) : nullptr,
      // La guilde du livre ; à défaut, celle que le serveur pose sur notre
      // propre acteur (ZC_CHANGE_GUILD, qu'il envoie aussi au joueur lui-même).
      .owner_guild_id = guild_book_.belonging().has_value()
                            ? static_cast<std::uint32_t>(guild_book_.belonging()->guild_id)
                        : world_.player() != nullptr ? world_.player()->guild_id
                                                     : 0};
  if (pieces_.companion_ai.Advance(link_, world_, pieces_.homunculus, pieces_.mercenary,
                                   companion_books, grimoire_.armed() != nullptr,
                                   world_mouse_.engagement, now_ms)) {
    grimoire_.Disarm();
  }
  if (const std::uint32_t wanted = pieces_.companion_ai.TakeWantedMonsterSheet();
      wanted != 0 && monster_sheet_.has_value()) {
    monster_sheet_->NoteWanted(wanted);
  }
  // Le mode Ciblage APRÈS les deux poursuites : un sort qui vient de partir
  // désigne sa cible, et ce que l'interface a reçu à l'image précédente se joue.
  AdvanceTargeting(now_ms);
  // Le menu du clic droit APRÈS le Ciblage : un cadre du HUD l'ouvre, et ses
  // lignes désignent, masquent ou attaquent la cible.
  AdvanceEntityMenu(now_ms);
  // « Voir l'équipement » APRÈS le menu qui la demande.
  AdvanceViewEquip(now_ms);
  // La fiche d'un monstre de même, après le menu qui l'ouvre.
  AdvanceMonsterSheet(now_ms);
  // L'Atlas des recettes, avec les autres fenêtres qui notent un survol.
  AdvanceCraftAtlas(now_ms);
  AdvanceNavigation(now_ms);
  // La boutique de vote : ce que sa fenêtre a rendu, et la vue qu'elle dessinera.
  pieces_.cash_shop.Advance(windows_.cash_shop.state, std::exchange(windows_.cash_shop.answer, {}),
                            link_, world_, pieces_.item_words, styles_, now_ms);
  windows_.cash_shop.view = pieces_.cash_shop.view();
  // MAJ + CLIC SUR UNE CARTE : son lien dans la saisie du chat ; et ce que son
  // menu a choisi d'autre que la description.
  if (const std::optional<ro::vfs::ItemLink> reposted = pieces_.cash_shop.TakeReposted();
      reposted.has_value()) {
    PostChatLink(*reposted);
  }
  if (const std::optional<ro::ui::ChatLinkAsk> chosen = pieces_.cash_shop.TakeChosen();
      chosen.has_value()) {
    FollowCatalogItem(*chosen, now_ms);
  }
  // LES COMMERCES : ce que leurs fenêtres ont rendu, et les vues qu'elles
  // dessineront. Un objet du sac se décrit par sa case, qui dit raffinement et
  // cartes ; un objet de la boutique, par son seul identifiant.
  // L'ONGLET DE VENTE D'UN PNJ DEMANDE LA BASE DU SERVEUR : « Ajouter par type »
  // range chaque ligne sous le type qu'elle lui donne.
  if (windows_.trade.npc_shop.state.open &&
      windows_.trade.npc_shop.state.tab == ro::ui::NpcShopTab::kSell) {
    database_.Read();
  }
  pieces_.trade.deal().RefuseProposals(settings_->options().pages().deal_refusal.refuses_deals());
  pieces_.trade.Advance(
      windows_.trade, link_, world_, pieces_.item_words,
      item_description_.has_value() ? item_description_->creator_names() : CreatorNames(),
      settings_->options().inventory_sell_locked(), pieces_.chat_rooms.room().in_room(), now_ms);
  // UNE VISITE REFERMÉE D'UN CLIC DROIT EMPORTE LA DESCRIPTION OUVERTE, comme
  // au client d'origine : c'était le plus souvent celle d'un article.
  if (pieces_.trade.TakeClosedVisits()) {
    windows_.description.open = false;
  }
  // LE COURRIER : les silences, ce que ses fenêtres ont rendu, les nouvelles,
  // et les vues qu'elles dessineront. Une pièce jointe se décrit comme un
  // objet de commerce.
  pieces_.mail.Advance(
      windows_.mail, link_, world_, pieces_.item_words,
      item_description_.has_value() ? item_description_->creator_names() : CreatorNames(), now_ms);
  AskBugReport(windows_.bug_report, pieces_.mail.TakeBugReport());
  // LES SALONS : ce que leurs fenêtres ont rendu, les nouvelles, et les vues.
  pieces_.chat_rooms.Advance(
      windows_.chat_room,
      ChatRoomContext{
          .link = link_,
          .world = world_,
          .words = pieces_.item_words,
          .friends = friends_,
          .stores =
              ChatRoomStoreState{.in_room = pieces_.chat_rooms.room().in_room(),
                                 .vending_open = pieces_.trade.vending().vending().shop_open(),
                                 .buying_store_open = pieces_.trade.buying_store().store().open()},
          .now_ms = now_ms});
  // LES SUCCÈS : ce que la fenêtre et le panneau ont rendu, et leurs vues.
  pieces_.achievements.Advance(windows_.achievements, link_, world_, now_ms);
  AskBugReport(windows_.bug_report, pieces_.achievements.TakeBugReport());
  std::optional<TradeItemFocus> described = pieces_.trade.TakeDescribed();
  // UNE CARTE DE LA BOUTIQUE DE VOTE se décrit comme une offre de commerce :
  // l'item n'est à personne, et son lien porte ce qu'on en sait.
  if (std::optional<ro::vfs::ItemLink> voted = pieces_.cash_shop.TakeDescribed();
      voted.has_value()) {
    described = TradeItemFocus{.item_id = voted->item_id, .link = std::move(voted)};
  }
  if (std::optional<TradeItemFocus> mailed = pieces_.mail.TakeDescribed(); mailed.has_value()) {
    described = std::move(mailed);
  }
  if (std::optional<TradeItemFocus> rewarded = pieces_.achievements.TakeDescribed();
      rewarded.has_value()) {
    described = std::move(rewarded);
  }
  if (described.has_value()) {
    FollowTradeItem(*described, now_ms);
  }

  // L'objet visé se ramasse en arrivant à côté de lui — ou s'oublie, si l'on n'y
  // arrive pas.
  if (pickup_.has_value()) {
    // La marche que le plancher avait retenue se redemande d'abord.
    if (const auto toward =
            ro::sim::DuePickupWalk(world_, *pickup_, grimoire_.approach_clock(), now_ms)) {
      ReportRefusal(ro::net::RequestMove(link_, *toward));
    }
    if (const ro::sim::Actor* self = world_.player(); self != nullptr) {
      switch (ro::sim::AdvancePickup(world_, planner_.PathFor(*self), *pickup_, now_ms)) {
        case ro::sim::PickupStep::kWait:
          break;
        case ro::sim::PickupStep::kPickUp:
          ReportRefusal(ro::net::RequestPickup(link_, pickup_->item_id));
          pickup_.reset();
          break;
        case ro::sim::PickupStep::kGiveUp:
          pickup_.reset();
          break;
      }
    }
  }

  // Les coups nouveaux font naître leurs nombres — et l'aperçu des réglages,
  // tant qu'il est allumé, ses coups factices.
  if (pieces_.numbers.has_value()) {
    const HitPopupLook& damage_look = settings_->options().damage_look();
    pieces_.numbers->Notice(world_, pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr,
                            damage_look);
    pieces_.numbers->Rehearse(now_ms, world_, damage_look);
  }
  // La plaisanterie et le cri font PARLER le personnage : c'est le seul effet du
  // lot qui émette vers le serveur, et chaque réplique se permet à part.
  talk_.Notice(world_, *vfs_, link_,
               TalkSwitches{.scream = settings_->options().speak_scream(),
                            .frost_joke = settings_->options().speak_frost_joke()},
               now_ms);
  // LES FORGERONS, pour les deux : un item forgé ramassé se nomme par le sien.
  const CreatorNames creators =
      item_description_.has_value() ? item_description_->creator_names() : CreatorNames();
  // Les ramassages neufs ouvrent leur ligne de bandeau. ⚠ DANS LE POMPAGE et
  // non dans le dessin : c'est ici que les faits du monde se relèvent, et une
  // image sautée ne doit pas perdre un objet.
  if (pieces_.item_toast.has_value()) {
    pieces_.item_toast->Notice(world_, settings_->options().item_obtain(), creators, now_ms);
  }
  // LES BULLES D'ANNONCE, pour la même raison : une image sautée ne doit en
  // perdre aucune.
  StageBalloons(now_ms);
  // Et ce que le ramassage dit dans la conversation : le gain, le refus.
  // ⚠ INDÉPENDANT DU BANDEAU, comme chez le client : couper l'un ne coupe
  // pas l'autre.
  if (pieces_.item_report.has_value()) {
    pieces_.item_report->Notice(world_, creators, now_ms);
    pieces_.item_report->NoticeNews(pieces_.item_news, world_, party_, creators);
  }
  // Ce que le serveur annonce à tous sur des objets : rare, raffinage, grade.
  ReportItemBroadcasts(pieces_.item_news, world_);
  // Et les refus de compétence, nommés et avec l'objet qui manque.
  pieces_.skill_refusals.Notice(
      world_, pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
      settings_->options().skill_fail_shown());
  // Et ce que les coups y écrivent, à l'instant de chaque frappe.
  if (pieces_.battle_log.has_value()) {
    pieces_.battle_log->Notice(world_, pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr,
                               party_, now_ms);
  }
  return {};
}

}  // namespace moonclient
