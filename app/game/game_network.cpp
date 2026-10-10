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
#include <utility>

#include <SDL3/SDL.h>

#include "ro/core/log.h"
#include "ro/net/action.h"
#include "ro/net/attack_reach.h"
#include "ro/net/bug_report.h"
#include "ro/net/card_album.h"
#include "ro/net/cart_choice.h"
#include "ro/net/cash_shop.h"
#include "ro/net/character_ledger.h"
#include "ro/net/command_settings.h"
#include "ro/net/friends.h"
#include "ro/net/guild.h"
#include "ro/net/inventory.h"
#include "ro/net/item_facts.h"
#include "ro/net/item_news.h"
#include "ro/net/map_guidance.h"
#include "ro/net/member_statuses.h"
#include "ro/net/monster_sheet.h"
#include "ro/net/movement.h"
#include "ro/net/npc_dialog.h"
#include "ro/net/npc_illustration.h"
#include "ro/net/party.h"
#include "ro/net/player_config.h"
#include "ro/net/refusal_text.h"
#include "ro/net/repair_list.h"
#include "ro/net/server_message.h"
#include "ro/net/spell_list.h"
#include "ro/net/title.h"
#include "ro/net/warp_memo.h"
#include "ro/net/world_notice.h"
#include "ro/net/world_update.h"
#include "ro/render/gl_context.h"
#include "ro/sim/damage_meter.h"
#include "ro/sim/movement.h"
#include "ro/sim/server_clock.h"
#include "ro/sim/world.h"
#include "ro/vfs/warp_memo_words.h"

#include "game/bug_report_context.h"
#include "game/connected_session.h"
#include "game/game.h"
#include "game/surface_announcer.h"
#include "input/camera_aim.h"
#include "items/warp_memo_report.h"
#include "world/attack_intent.h"
#include "world/click_action.h"
#include "world/monster_auras.h"

namespace moonclient {

ro::core::Result<void> Game::PumpNetwork(std::uint32_t now_ms) {
  // Le réseau AVANT le dessin : une image doit montrer l'état le plus récent,
  // pas celui d'avant la lecture.
  if (moving_.has_value()) {
    AdvanceZoneMove();
  }
  if (!link_.is_open()) {
    return {};
  }
  ro::net::Connection::Polled polled = link_.Poll();
  // LA CIBLE D'UN REFUS DE PORTÉE, s'il en arrive un. ⚠ RELEVÉE ICI ET AGIE
  // PLUS BAS : la poursuite a besoin des positions que la trame vient de
  // recaler, et reprendre depuis la boucle la ferait partir de l'état d'avant.
  std::optional<std::uint32_t> refused_target;
  // LE MOTIF D'UNE ÉVICTION, s'il en arrive une : la partie s'arrête après le
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
    // 🔴 EN SALON, LA PAROLE REVIENT AU SALON, et le monde ne l'écrit pas : le
    // serveur ne nous fait plus entendre la carte.
    ro::net::FrameOutcome outcome = pieces_.chat_rooms.ClaimLine(received, world_, now_ms);
    // LES CHUCHOTEMENTS AVANT LE MONDE : eux seuls savent qui sont nos amis et
    // ce qui attend un accusé.
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = pieces_.whispers.ApplyFrame(received, world_, friends_, guild_book_, now_ms);
    }
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = ro::net::ApplyFrame(received, world_, now_ms);
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
    // LES TITRES VIVENT HORS DU MONDE, et leur traducteur passe APRÈS lui : la
    // carte de nom du joueur intéresse les deux, et chacun y lit sa part.
    const ro::net::FrameOutcome titled =
        ro::net::ApplyTitleFrame(received, titles_, world_.player_id());
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = titled;
    }
    // LA GUILDE AUSSI : son carnet vit hors du monde, et le départ d'un membre
    // se reconnaît au personnage joué.
    const ro::net::FrameOutcome guilded =
        ro::net::ApplyGuildFrame(received, guild_book_, ticket_.character_id);
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = guilded;
    }
    // LE GROUPE, de même, reconnu par le compte joué ; l'horloge date le PS d'un
    // membre. Et LES AMIS, dans la même passe.
    const ro::net::FrameOutcome grouped =
        ro::net::ApplyPartyFrame(received, party_, world_.player_id(), now_ms);
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = grouped;
    }
    const ro::net::FrameOutcome befriended = ro::net::ApplyFriendFrame(received, friends_);
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = befriended;
    }
    // ET CE QU'UN SCRIPT MONTRE SUR LA CARTE : les repères, et les demandes de
    // navigation qui attendent qu'on les prenne.
    const ro::net::FrameOutcome guided =
        ro::net::ApplyMapGuidanceFrame(received, navi_.guidance, now_ms);
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = guided;
    }
    // LE BOSS D'UN CONVEX MIRROR, et les nouvelles d'objets : le ramassage d'un
    // membre du groupe, une pièce abîmée — que le sac lit aussi.
    for (const ro::net::FrameOutcome told :
         {ro::net::ApplyBossInfoFrame(received, navi_.guidance, world_, now_ms),
          ro::net::ApplyItemNewsFrame(received, pieces_.item_news, now_ms)}) {
      if (outcome == ro::net::FrameOutcome::kUnhandled) {
        outcome = told;
      }
    }
    // ET LES ÉTATS DES MEMBRES, que la réponse sur la cible et les transitions
    // du monde portent aussi.
    if (party_panel_.has_value()) {
      const ro::net::FrameOutcome stated = ro::net::ApplyMemberStatusFrame(
          received, party_panel_->member_statuses(), party_, world_.player_id(), now_ms);
      if (outcome == ro::net::FrameOutcome::kUnhandled) {
        outcome = stated;
      }
    }
    // ET L'ÉQUIPEMENT MONTRÉ PAR UN AUTRE JOUEUR, avec son refus.
    const ro::net::FrameOutcome inspected = ro::net::ApplyViewEquipFrame(received, inspected_);
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = inspected;
    }
    // ET LES MESSAGES PAR NUMÉRO, qui s'écrivent dans la conversation.
    const ro::net::FrameOutcome said = ro::net::ApplyServerMessageFrame(
        received, world_, pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr, now_ms);
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = said;
    }
    // ET LA LISTE DES CHARRETTES QUE LE SERVEUR PROPOSE, qui ouvre sa fenêtre.
    const ro::net::FrameOutcome carted =
        ro::net::ApplyCartChoiceFrame(received, pieces_.cart_choice.choice());
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = carted;
    }
    // ET LES SORTS QU'AUTOSPELL PROPOSE, qui ouvrent leur liste.
    const ro::net::FrameOutcome spelled =
        ro::net::ApplySpellListFrame(received, pieces_.spell_list.offer());
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = spelled;
    }
    // ET LES PIÈCES QUE REPAIR WEAPON PROPOSE, avec l'issue de la réparation.
    const ro::net::FrameOutcome repaired =
        ro::net::ApplyRepairFrame(received, pieces_.repair.offer());
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = repaired;
    }
    pieces_.repair.SayOutcome(world_, now_ms);
    // ET LES TROIS COMPAGNONS QUI ONT UNE FICHE.
    for (const ro::net::FrameOutcome companion :
         {pieces_.pet.ApplyFrame(received, world_, now_ms),
          pieces_.homunculus.ApplyFrame(received, world_, pieces_, now_ms),
          pieces_.mercenary.ApplyFrame(received, world_, now_ms)}) {
      if (outcome == ro::net::FrameOutcome::kUnhandled) {
        outcome = companion;
      }
    }
    pieces_.companion_ai.ObserveFrame(received, pieces_.homunculus, pieces_.mercenary, now_ms);
    // ET LA BOUTIQUE DE VOTE, avec la carte des items à effet de chapeau. ⚠
    // ELLE LIT AUSSI LES CHANGEMENTS DE CARTE, que le monde vient d'appliquer :
    // la boutique se ferme avec eux.
    const ro::net::FrameOutcome shopped =
        ro::net::ApplyCashShopFrame(received, pieces_.cash_shop.shop());
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = shopped;
    }
    const ro::net::FrameOutcome hatted =
        ro::net::ApplyHatEffectItemsFrame(received, pieces_.cash_shop.hat_effects());
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = hatted;
    }
    // ET LES COMMERCES — boutiques de PNJ, échoppes, troc. ⚠ EUX AUSSI LISENT
    // LES CHANGEMENTS DE CARTE, qui les ferment.
    if (const ro::net::FrameOutcome traded = pieces_.trade.ApplyFrame(received, world_.inventory());
        outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = traded;
    }
    // ET LE COURRIER, daté sur l'horloge réelle : le serveur annonce
    // l'expiration en secondes restantes. ⚠ IL LIT AUSSI LES CHANGEMENTS DE
    // CARTE, qui remettent l'écriture à zéro chez le serveur.
    if (const ro::net::FrameOutcome mailed =
            pieces_.mail.ApplyFrame(received, MailNowUnixSeconds());
        outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = mailed;
    }
    // ET LES SALONS. ⚠ APRÈS LE MONDE : l'entrée lit l'enseigne qu'il tient.
    if (const ro::net::FrameOutcome roomed =
            pieces_.chat_rooms.ApplyFrame(received, world_, pieces_.item_words, now_ms);
        outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = roomed;
    }
    // LES SUCCÈS, dont 0x0A23 et 0x0A26 sont aussi lus par les titres : chacun
    // y prend sa part.
    if (const ro::net::FrameOutcome achieved =
            pieces_.achievements.ApplyFrame(received, windows_.achievements, world_, now_ms);
        outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = achieved;
    }
    if (const ro::net::FrameOutcome quested =
            pieces_.quests.ApplyFrame(received, windows_.quests, world_, now_ms);
        outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = quested;  // Le journal des quêtes.
    }
    // ET L'ILLUSTRATION QU'UN SCRIPT MONTRE, ou efface.
    const ro::net::FrameOutcome illustrated =
        ro::net::ApplyNpcIllustrationFrame(received, pieces_.npc_illustration.slot());
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = illustrated;
    }
    // ET LA FICHE D'UN MONSTRE, avec le relevé du skill Sense.
    if (monster_sheet_.has_value()) {
      const ro::net::FrameOutcome sheeted =
          ro::net::ApplyMonsterSheetFrame(received, monster_sheet_->book());
      if (outcome == ro::net::FrameOutcome::kUnhandled) {
        outcome = sheeted;
      }
    }
    // ET CE QUE LE SERVEUR SEUL SAIT D'UN OBJET : ses sources, son script et le
    // nom de son forgeron, que la description demande.
    if (item_description_.has_value()) {
      if (const ro::net::FrameOutcome told =
              ro::net::ApplyItemFactsFrame(received, item_description_->facts());
          outcome == ro::net::FrameOutcome::kUnhandled) {
        outcome = told;
      }
      // ET LES RÉGLAGES DE COMMANDE, APRÈS LES FAITS : la liste des presets
      // retient comme sauvegardée la liste de ramassage qu'ils viennent de lire.
      if (const ro::net::FrameOutcome commanded = ro::net::ApplyCommandSettingsFrame(
              received, pieces_.commands.settings(), item_description_->facts());
          outcome == ro::net::FrameOutcome::kUnhandled) {
        outcome = commanded;
      }
    }
    // ET CE QUE LE SERVEUR RÉPOND À /memo — l'accusé, ou le refus de la carte —,
    // écrit dans la conversation avec les phrases du client.
    {
      const std::optional<std::uint8_t> memo_ack = ro::net::ReadWarpMemoAck(received);
      const std::optional<std::int16_t> map_refusal = ro::net::ReadMapInfoNotice(received);
      std::optional<WarpMemoLine> memo_line;
      if (memo_ack.has_value()) {
        memo_line = WarpMemoLineOfAck(*memo_ack);
      } else if (map_refusal.has_value()) {
        memo_line = WarpMemoLineOfMapInfo(*map_refusal);
      }
      if (memo_line.has_value()) {
        const std::string_view memo_text =
            ro::vfs::WarpMemoText(pieces_.item_words, memo_line->message);
        // Une phrase vide ne s'écrit pas : pas de ligne vide dans la
        // conversation.
        if (!memo_text.empty()) {
          world_.RecordChat(ro::sim::ChatKind::kServer, 0, std::string(memo_text), now_ms,
                            memo_line->color);
        }
      }
      // Lue, même sans phrase : un code que le client ne dit pas n'est pas une
      // trame perdue.
      if (memo_ack.has_value() || map_refusal.has_value()) {
        outcome = ro::net::FrameOutcome::kApplied;
      }
    }
    // ET L'ACCUSÉ D'UN RAPPORT DE BUG, que la boîte montre cinq secondes.
    if (const std::optional<std::uint8_t> ack = ro::net::ReadBugReportAck(received);
        ack.has_value()) {
      windows_.bug_report.ack_status = *ack;
      windows_.bug_report.ack_fresh = true;
      if (outcome == ro::net::FrameOutcome::kUnhandled) {
        outcome = ro::net::FrameOutcome::kIgnored;
      }
    }
    // ET L'ALBUM DE CARTES. ⚠ SA RÉPONSE PEUT ÉMETTRE : un sacrifice réussi range
    // aussitôt le reste de la pile dans la pochette qu'il vient d'ouvrir.
    if (const ro::net::FrameOutcome albumed = card_album_.ApplyFrame(received, link_, now_ms);
        outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = albumed;
    }
    // ET LE CARNET DE CHASSE MVP, daté sur l'horloge réelle : le serveur y date
    // tout en secondes. ⚠ DÈS LA POIGNÉE DE MAIN : l'invitation en attente
    // arrive avant toute ouverture.
    if (const ro::net::FrameOutcome hunted =
            pieces_.mvp_tracker.ApplyFrame(received, LocalUnixSeconds(), now_ms);
        outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = hunted;
    }
    // ET LE CHAMP DE BATAILLE : la file d'inscription, puis le combat.
    if (const ro::net::FrameOutcome fought =
            pieces_.battleground.ApplyFrame(received, world_, now_ms);
        outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = fought;
    }
    // ET CE QUE LE SERVEUR DIT DU NÔTRE. ⚠ CHAQUE ANNONCE DE L'ÉQUIPEMENT
    // PARLE, même quand la valeur ne change pas : c'est la réponse au clic.
    // Celle des costumes se tait, et elle revient à chaque carte.
    const std::uint32_t announced_before = own_config_.equipment_announcements();
    const std::uint32_t calls_before = own_config_.call_announcements();
    const ro::net::FrameOutcome configured = ro::net::ApplyConfigFrame(received, own_config_);
    if (own_config_.equipment_announcements() != announced_before) {
      SayEquipmentShown(now_ms);
    }
    if (own_config_.call_announcements() != calls_before) {
      SayCallable(now_ms);
    }
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = configured;
    }
    // ET CE QU'AUCUNE FENÊTRE NE MONTRE ENCORE : réputation, packs d'émotions,
    // jeu d'équipement de rechange.
    if (const ro::net::FrameOutcome recorded = ro::net::ApplyLedgerFrame(received, ledger_);
        outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = recorded;
    }
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
    // 🔴 UNE ÉVICTION FERME LE JEU, comme un kick du staff le demande : le
    // serveur a déjà coupé la liaison, et rester ouvert laisserait le joueur
    // devant un monde figé. La boîte dit pourquoi avant que la fenêtre ne
    // disparaisse — la conversation, elle, disparaît avec.
    //
    // ⚠ HORS DU PLEIN ÉCRAN D'ABORD, et la fenêtre du jeu pour parente : en
    // plein écran exclusif, la boîte s'ouvrirait derrière l'image figée, et la
    // boucle attendrait une réponse que personne ne peut donner. Le réglage,
    // lui, ne change pas : seule la fenêtre de cette partie-ci en sort.
    const std::string why = ro::net::BanText(*evicted);
    ro::core::LogError("session close par le serveur (code {}) : {}", *evicted, why);
    SDL_Window* window = gl_->window();
    SDL_SetWindowFullscreen(window, false);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, std::string(kWindowTitle).c_str(),
                             why.c_str(), window);
    link_.Close();
    running_ = false;
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
    ro::core::LogError("liaison perdue : {}", DescribeLinkLoss(polled.status, link_.error()));
    link_.Close();
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

  // LE FILET DE LA PAGE DE DIALOGUE, pour la même raison : une page dont le
  // paquet terminal n'arrive jamais — un script qui écrit puis dort, ou qui
  // s'arrête sur `end` — attend sinon pour toujours. Il se mesure en SILENCE,
  // donc il ne peut se déclencher qu'ici, une fois le drain fini.
  static_cast<void>(world_.CommitNpcPageIfIdle(now_ms));

  // LA POIGNÉE DE MAIN D'INTÉGRITÉ, aussitôt la liaison ouverte : le serveur arme
  // son minuteur à l'entrée en carte, et ce qu'on lui doit tient en un paquet.
  // Ce que le journal en dit ne se répète pas — seul un CHANGEMENT
  // d'état s'écrit, sinon le renvoi toutes les trois secondes noierait le reste.
  if (link_.is_open()) {
    handshake_.Pump(link_, now_ms);
    if (handshake_.state() != reported_handshake_) {
      reported_handshake_ = handshake_.state();
      // 🔴 DEUX DES CINQ ÉTATS ANNONCENT LA FIN DE LA SESSION — le refus, qui
      // donne cinq secondes, et l'abandon après cinq envois. Les écrire au même
      // rang que « émise » et « acceptée » les faisait disparaître du filtre par
      // gravité à l'instant précis où le joueur y cherche pourquoi il a été
      // éjecté.
      const bool cut_off = reported_handshake_ == ro::net::MoonclientHandshakeState::kRefused ||
                           reported_handshake_ == ro::net::MoonclientHandshakeState::kGivenUp;
      if (cut_off) {
        ro::core::LogWarning("{}", ro::net::DescribeHandshake(reported_handshake_));
      } else {
        ro::core::LogInfo("{}", ro::net::DescribeHandshake(reported_handshake_));
      }
      if (reported_handshake_ == ro::net::MoonclientHandshakeState::kVerified) {
        // LE CATALOGUE DE LA BOUTIQUE, une fois par partie : le personnage est
        // sur sa carte, et le serveur écoute.
        pieces_.cash_shop.RequestCatalogueOnce(link_);
      }
    }
    // 🔴 LE SERVEUR NOUS RECONNAÎT : c'est alors qu'il accepte nos annonces, et il
    // les oublie à chaque entrée en zone. Sans elles, il refuse toute commande
    // d'album et dépouille les dialogues de leurs images. Le carnet y entre et en
    // sort avec son interrupteur.
    pieces_.surfaces.Pump(
        link_, handshake_.state() == ro::net::MoonclientHandshakeState::kVerified,
        ShownSurfaces(settings_->options().pages().mvp_tracker.settings().enabled));
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
    const MapArrival arrival{&*vfs_,  &*stage_, &planner_,    &world_,         &terrain_,
                             &water_, &models_, &background_, &attempted_map_, flatten};
    if (auto entered = EnterAnnouncedMap(arrival); !entered) {
      return std::unexpected(entered.error());
    }
    // Même carte comprise : toute entrée repose la vue d'arrivée.
    camera_rig_.EnterMap(camera_, stage_->name);
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

  // La caméra suit le personnage, et tremble quand un effet le demande.
  AimCamera(camera_, (*stage_), world_, planner_, now_ms, aim_);

  // La cible visée se POURSUIT jusqu'à être à portée, puis se frappe. ⚠ APPELÉE
  // MÊME SANS POURSUITE EN COURS : le refus du serveur en rouvre une, et c'est
  // ce qui relance la course derrière une cible qui s'éloigne.
  //
  // ⚠ ET LE REFUS ÉCARTE LA COMPÉTENCE EN ATTENTE : le client n'a qu'UNE action
  // en attente, que ce refus réécrit. Deux poursuites vivantes demanderaient
  // deux marches par image, vers des cases différentes.
  //
  // ⚠ LE RÉARMEMENT REPREND LE GESTE COURANT, et c'est une APPROXIMATION
  // assumée : l'action tirée a été effacée, donc ce qu'elle avait retenu est
  // perdu. Le refus arrive un aller-retour après le coup, si bien que la touche
  // a rarement changé entre les deux — mais si elle a changé, c'est le nouvel
  // état qui gagne, et non celui du clic.
  if (AdvanceAttack(link_, world_, planner_, PlannedMapOf(*stage_), attack_,
                    grimoire_.approach_clock(), refused_target,
                    Targeting::WantsContinuousAttack(
                        SiegeMap(world_mouse_.engagement, world_.map_mode()),
                        (SDL_GetModState() & SDL_KMOD_CTRL) != 0, settings_->options().no_ctrl()),
                    now_ms, &targeting_, &world_mouse_.engagement)) {
    grimoire_.CancelPending();
  }

  // L'ACTION EN ATTENTE AVANCE D'UN TOUR : on marche vers la cible désignée
  // tant qu'elle est hors de portée, et la compétence ne part qu'une fois
  // arrivé. Le client natif la relance au même endroit de son tour de boucle.
  // Voir `ro/sim/pending_action.h`.
  if (link_.is_open()) {
    grimoire_.AdvancePending(link_, world_, planner_, PlannedMapOf(*stage_), now_ms);
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
                                   companion_books, grimoire_.armed() != nullptr, now_ms)) {
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
  // MAJ + CLIC SUR UNE CARTE : son lien dans la saisie du chat.
  if (const std::optional<ro::vfs::ItemLink> reposted = pieces_.cash_shop.TakeReposted();
      reposted.has_value()) {
    PostChatLink(*reposted);
  }
  // LES COMMERCES : ce que leurs fenêtres ont rendu, et les vues qu'elles
  // dessineront. Un objet du sac se décrit par sa case, qui dit raffinement et
  // cartes ; un objet de la boutique, par son seul identifiant.
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
  // LES SALONS : ce que leurs fenêtres ont rendu, les nouvelles, et les vues.
  pieces_.chat_rooms.Advance(
      windows_.chat_room,
      ChatRoomContext{
          .link = link_,
          .world = world_,
          .words = pieces_.item_words,
          .stores =
              ChatRoomStoreState{.in_room = pieces_.chat_rooms.room().in_room(),
                                 .vending_open = pieces_.trade.vending().vending().shop_open(),
                                 .buying_store_open = pieces_.trade.buying_store().store().open()},
          .now_ms = now_ms});
  // LES SUCCÈS : ce que la fenêtre et le panneau ont rendu, et leurs vues.
  pieces_.achievements.Advance(windows_.achievements, link_, world_, now_ms);
  if (std::optional<BugReportContext> report = pieces_.achievements.TakeBugReport();
      report.has_value()) {
    AskBugReport(windows_.bug_report, std::move(*report));
  }
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
    windows_.description.item_id = described->item_id;
    windows_.description.from_slot = described->bag_slot;
    windows_.description.from_holder = ro::ui::ItemHolder::kInventory;
    // L'OBJET D'UN AUTRE se décrit comme un lien : raffinement, cartes et
    // options voyagent avec lui.
    if (described->link.has_value() && item_description_.has_value()) {
      item_description_->NoteLinked(*described->link);
      windows_.description.from_slot = 0;
      windows_.description.from_holder = ro::ui::ItemHolder::kChatLink;
    }
    windows_.description.open = true;
    windows_.description.place_at_mouse = true;
    windows_.description.raise = true;
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
  // Et ce que le ramassage dit dans la conversation : le gain, le refus.
  // ⚠ INDÉPENDANT DU BANDEAU, comme chez le client : couper l'un ne coupe
  // pas l'autre.
  if (pieces_.item_report.has_value()) {
    pieces_.item_report->Notice(world_, creators, now_ms);
    pieces_.item_report->NoticeNews(pieces_.item_news, world_, party_, creators);
  }
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
