// CE QUI LIT UNE TRAME APRÈS LE MONDE : les livres, les panneaux et les pièces
// qui vivent hors de lui — titres, guilde, groupe, amis, commerces, courrier,
// succès, compagnons, fiches, carnets.
//
// ⚠ CHACUN A SA CHANCE SUR CHAQUE TRAME, DANS L'ORDRE DE LEUR APPEL : un lecteur
// qui la traite ne la retire pas aux suivants, et l'issue retenue est la
// première qui ne soit pas « non traitée », sauf exception dite sur place.
// Plusieurs trames intéressent deux lecteurs à la fois — la carte de nom du
// joueur, les annonces de succès.

#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/adoption.h"
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
#include "ro/net/manner_points.h"
#include "ro/net/map_guidance.h"
#include "ro/net/member_statuses.h"
#include "ro/net/monster_sheet.h"
#include "ro/net/movement.h"
#include "ro/net/npc_illustration.h"
#include "ro/net/party.h"
#include "ro/net/player_config.h"
#include "ro/net/progress_bar.h"
#include "ro/net/repair_list.h"
#include "ro/net/server_message.h"
#include "ro/net/spell_list.h"
#include "ro/net/status_chat.h"
#include "ro/net/title.h"
#include "ro/net/user_count.h"
#include "ro/net/warp_memo.h"
#include "ro/net/world_update.h"
#include "ro/sim/world.h"
#include "ro/vfs/warp_memo_words.h"

#include "game/game.h"
#include "items/warp_memo_report.h"
#include "world/player_report.h"

namespace moonclient {

ro::net::FrameOutcome Game::ApplyFrameBeyondWorld(const ro::net::Frame& received,
                                                  ro::net::FrameOutcome outcome,
                                                  std::uint32_t now_ms) {
  const ro::sim::BannedWords* banned = &pieces_.banned_words;
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
      ro::net::ApplyGuildFrame(received, guild_book_, ticket_.character_id, banned);
  if (outcome == ro::net::FrameOutcome::kUnhandled) {
    outcome = guilded;
  }
  // LE GROUPE, de même, reconnu par le compte joué ; l'horloge date le PS d'un
  // membre.
  const ro::net::FrameOutcome grouped =
      ro::net::ApplyPartyFrame(received, party_, world_.player_id(), now_ms, banned);
  if (outcome == ro::net::FrameOutcome::kUnhandled) {
    outcome = grouped;
  }
  // LES AMIS ; CE QU'UN SCRIPT MONTRE SUR LA CARTE (repères, navigation à
  // prendre), le boss d'un Convex Mirror, les nouvelles d'objets (ramassage
  // d'un membre, pièce abîmée), l'issue d'une alliance ou d'une hostilité,
  // l'adoption, les fiches du staff, l'accusé d'un signalement, les points de
  // manière, la ligne d'un état, l'équipement montré par un autre joueur,
  // avec son refus, le jeu d'équipement de rechange, la pièce posée ou
  // retirée — son nom, puis ce qu'elle apporte —, et les éditions de carte que
  // le serveur tient.
  for (const ro::net::FrameOutcome told :
       {ro::net::ApplyFriendFrame(received, friends_),
        ro::net::ApplyViewEquipFrame(received, inspected_),
        ro::net::ApplyMapGuidanceFrame(received, navi_.guidance, now_ms),
        ro::net::ApplyBossInfoFrame(received, navi_.guidance, world_, now_ms),
        ro::net::ApplyItemNewsFrame(received, pieces_.item_news, now_ms),
        ro::net::ApplyGuildRelationAnswerFrame(received, world_, now_ms),
        ro::net::ApplyAdoptionFrame(received, pieces_.adoption.desk()),
        pieces_.entity_inspector.ApplyFrame(received), pieces_.chat_block_log.ApplyFrame(received),
        PlayerReportDesk::ApplyFrame(received, world_, now_ms),
        ro::net::ApplyMannerPointFrame(received, world_, now_ms),
        ro::net::ApplyStatusChatFrame(received, world_, party_, now_ms),
        character_->SayEquipSwitch(received, world_, now_ms),
        character_->SayEquipChange(received, world_, now_ms),
        character_->SayEquipAttributes(received, world_, now_ms),
        pieces_.map_edit_sync.ApplyFrame(received)}) {
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
  // ET LES MESSAGES PAR NUMÉRO, qui s'écrivent dans la conversation.
  const ro::net::FrameOutcome said = ro::net::ApplyServerMessageFrame(
      received, world_, pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr, now_ms,
      ItemBaseNames(pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr));
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
  if (const ro::net::FrameOutcome mailed = pieces_.mail.ApplyFrame(received, MailNowUnixSeconds());
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
  // ET CE QU'UN SCRIPT POSE À L'ÉCRAN : l'illustration, montrée ou effacée, et
  // la barre de progression — voir ro/net/progress_bar.h.
  for (const ro::net::FrameOutcome scripted :
       {ro::net::ApplyNpcIllustrationFrame(received, pieces_.npc_illustration.slot()),
        ro::net::ApplyProgressFrame(received, world_, pieces_.progress, now_ms)}) {
    if (outcome == ro::net::FrameOutcome::kUnhandled) {
      outcome = scripted;
    }
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
  // ET LE COMPTE DES JOUEURS CONNECTÉS, que `/who` a demandé.
  if (const std::optional<std::int32_t> users = ro::net::ReadUserCount(received);
      users.has_value()) {
    world_.RecordChat(ro::sim::ChatKind::kServer, 0,
                      std::string(ro::core::Tr("Joueurs actuellement connectés :")) + " " +
                          std::to_string(*users),
                      now_ms);
    outcome = ro::net::FrameOutcome::kApplied;
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
  // ET LES CLASSEMENTS, LES POINTS DE RANG ET LES AVIS D'ÉTOILE. ⚠ ÉMET : un
  // classement demande les noms qui lui manquent.
  if (const ro::net::FrameOutcome ranked =
          pieces_.rank_notices.ApplyFrame(received, world_, link_, now_ms);
      outcome == ro::net::FrameOutcome::kUnhandled) {
    outcome = ranked;
  }
  // ET LA RECHERCHE DE GROUPE, dont les avis s'écrivent dans la conversation.
  if (const ro::net::FrameOutcome booked =
          pieces_.party_booking.ApplyFrame(received, world_, now_ms);
      outcome == ro::net::FrameOutcome::kUnhandled) {
    outcome = booked;
  }
  // ET LE CHAMP DE BATAILLE : la file d'inscription, puis le combat.
  if (const ro::net::FrameOutcome fought =
          pieces_.battleground.ApplyFrame(received, world_, now_ms);
      outcome == ro::net::FrameOutcome::kUnhandled) {
    outcome = fought;
  }
  // 🔴 CHAQUE DÉCLARATION DE CARTE APPELLE « JE SUIS PRÊT À JOUER », comme
  // dans le jeu : c'est elle qui fait redire l'instance par le serveur.
  if (const auto told = ro::net::AnswerMapProperty(received, link_); !told) {
    ro::core::LogError("prêt à jouer refusé : {}", ro::core::Describe(told.error()));
  }
  // ET L'INSTANCE, datée sur l'horloge réelle : le serveur annonce ses
  // échéances en secondes Unix.
  if (const ro::net::FrameOutcome instanced =
          pieces_.memorial.ApplyFrame(received, static_cast<std::int64_t>(std::time(nullptr)));
      outcome == ro::net::FrameOutcome::kUnhandled) {
    outcome = instanced;
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
  // ET LE CLASSEMENT SURVIVORS : ses quatre ZC se lisent tous, jamais ignorés.
  if (const ro::net::FrameOutcome survivors_read = survivors_ranking_.ApplyFrame(received, now_ms);
      outcome == ro::net::FrameOutcome::kUnhandled) {
    outcome = survivors_read;
  }
  // ET CE QU'AUCUNE FENÊTRE NE MONTRE ENCORE : réputation, packs d'émotions,
  // jeu d'équipement de rechange.
  if (const ro::net::FrameOutcome recorded = ro::net::ApplyLedgerFrame(received, ledger_);
      outcome == ro::net::FrameOutcome::kUnhandled) {
    outcome = recorded;
  }
  return outcome;
}

}  // namespace moonclient
