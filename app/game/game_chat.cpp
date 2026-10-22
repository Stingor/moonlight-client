// LA CONVERSATION, VUE DE LA BOUCLE : ce qui se prépare avant le dessin — les
// onglets triés, les pastilles des liens, la destination armée —, puis ce que
// ses fenêtres ont rendu.
//
// ⚠ LES DEUX MOITIÉS ENCADRENT LE DESSIN : la préparation avant toutes les
// vues, le routage après toutes les réponses, parce qu'il réordonne les
// onglets. Les liens eux-mêmes vivent dans `game_chat_links.cpp`.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/net/action.h"
#include "ro/net/chat_target.h"
#include "ro/net/discord_relay.h"
#include "ro/net/player_settings.h"
#include "ro/sim/chat_mention.h"
#include "ro/ui/chat_input.h"
#include "ro/ui/chat_whisper_list.h"
#include "ro/ui/chat_window.h"

#include "chat/chat_image_cache.h"
#include "chat/chat_item_link.h"
#include "chat/chat_line_preview.h"
#include "chat/chat_rich_decor.h"
#include "chat/chat_view.h"
#include "game/game.h"
#include "game/setting_destination_open.h"
#include "options/chat_web_options.h"
#include "options/game_option_ids.h"
#include "world/click_action.h"

namespace moonclient {
namespace {

// QUI PARLE, ET OÙ : ce qui, hors de la ligne, en fait la place. Le compteur et
// la retenue le lisent du même endroit.
[[nodiscard]] ChatWireSpeaker WireSpeakerOf(const ro::sim::World& world,
                                            const ChatLinePreviewMaker& preview) {
  return ChatWireSpeaker{.name = world.player_name(),
                         .channels = world.chat_channels(),
                         .bound_channel = preview.bound_channel(),
                         .battlefield = world.map_mode().battlefield,
                         .may_wear_fake_name = world.group_level() > 0,
                         .relayed_to_discord = ro::net::IsDiscordRelayMap(world.map_name())};
}

// « Enregistrée : <chemin> », ou l'échec.
[[nodiscard]] std::string SavedPictureLine(const ChatSaveOutcome& saved) {
  if (!saved.ok) {
    return ro::core::Tr("L'image n'a pas pu être enregistrée.");
  }
  const std::u8string where = saved.written.u8string();
  const std::string path(where.begin(), where.end());
  // ⚠ LE FORMAT SE TRADUIT DANS L'APPEL MÊME : c'est là que le compilateur en
  // vérifie les arguments. La première passe mesure, la seconde écrit.
  const int needed = std::snprintf(nullptr, 0, ro::core::Tr("Enregistrée : %s"), path.c_str());
  if (needed <= 0) {
    return path;
  }
  std::string line(static_cast<std::size_t>(needed), '\0');
  std::snprintf(line.data(), line.size() + 1, ro::core::Tr("Enregistrée : %s"), path.c_str());
  return line;
}

}  // namespace

void Game::StageChat(std::uint32_t now_ms) {
  // LES ONGLETS TRIENT AVANT LE DESSIN : c'est ici que le genre d'une ligne
  // sert, et la fenêtre n'en voit rien.
  //
  // ⚠ LE JOURNAL D'ABORD : la moisson écrit dedans, et une ligne moissonnée
  // avant son ouverture n'y entrerait jamais. C'est une CASE qu'on repousse à
  // chaque image, pas un évènement — l'ordre de chargement des réglages n'est
  // pas quelque chose sur quoi il faut parier.
  chat_tabs_->KeepLog(settings_->options().chat_look().keep_log);
  // ⚠ POSÉES À CHAQUE IMAGE, et non une fois : la conversation vit dès l'écran de
  // sélection, la table de l'installation n'est lue qu'à l'entrée en jeu. Sans
  // elles, une balise d'objet resterait à l'écran telle quelle.
  chat_tabs_->ReadsItems(
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
      &pieces_.item_words);
  chat_tabs_->ReadsMaps(pieces_.map_display_names.has_value() ? &*pieces_.map_display_names
                                                              : nullptr);
  // LE COMPTEUR RANGÉ PARMI LES ONGLETS. Rangé ou flottant, c'est le profil de
  // disposition qui le dit ; éteint dans les réglages, sa languette se cache
  // sans qu'il sorte. Elle porte ce que le joueur a choisi d'y lire.
  // ⚠ COMPOSÉE À L'IMAGE PRÉCÉDENTE : le compteur se compose après la
  // conversation, et une image de retard ne se voit pas.
  chat_tabs_->HideMeter(!settings_->options().pages().damage_meter.look().shown);
  chat_tabs_->FollowMeterDock(windows_.damage_meter.docked);
  chat_tabs_->LabelMeter(pieces_.damage_meter.tab_label());
  // LES CONVERSATIONS 1:1 AVANT LE TRI : ce qu'elles prennent au fil n'y paraît
  // pas, et elles le prennent en lisant les lignes neuves. L'alarme sonne pour
  // celles qu'une ligne vient d'ouvrir.
  WhisperOptions& whisper_page = settings_->options().pages().whispers;
  const bool whisper_rang =
      pieces_.whispers.Advance(windows_.whispers,
                               WhisperContext{.link = link_,
                                              .world = world_,
                                              .setup = whisper_page.setup(),
                                              .now_ms = now_ms,
                                              .judge_mvp = [this](const MvpLink& link) {
                                                return pieces_.mvp_tracker.FactsFor(link);
                                              }}) > 0;
  if (whisper_rang && vfs_ != nullptr) {
    PlayWhisperAlarm(audio_, *vfs_, client_root_, whisper_page.setup().alarm);
  }
  // QUI VIENT DE NOUS CHUCHOTER entre dans les récents de la box « Pseudo » :
  // c'est souvent à lui qu'on veut répondre.
  chat_preview_.recent_whispers().FollowChat(world_.chat_lines(), world_.player_name());
  // LA BARRE A REPRIS LE CLAVIER : un lien posé y retourne, plus dans la
  // dernière conversation 1:1.
  if (windows_.chat.main.input.was_active) {
    pieces_.whispers.ReleaseLinks();
  }
  // ── LE RELAI DISCORD ───────────────────────────────────────────────────
  // Ses lignes se filtrent par la case « Discord » des onglets ; un relai que
  // le serveur dit coupé pour ce personnage se rallume, une fois.
  ro::net::ReviveDiscordRelay(link_, world_);
  // ── LES ADRESSES, LES IMAGES ET LES MENTIONS ───────────────────────────
  // ⚠ AVANT LE TRI : le cache libère ses textures ici, et la décoration lit
  // les faits que `Compose` a pu oublier.
  const ChatWebSetup& web = settings_->options().pages().chat_web.setup();
  pieces_.chat_web.Configure(web);
  pieces_.chat_web.Compose(windows_.chat, now_ms);
  const ro::sim::MentionRules& mention_rules =
      pieces_.mentions.RulesFor(world_.player_name(), world_.player_id(), web.mention_words);
  // ⚠ LES LIGNES SE CONSOMMENT MÊME CASE DÉCOCHÉE : la cocher ne doit pas faire
  // sonner ce qui s'est dit avant. ⚠ UN CHUCHOTEMENT QUI VIENT DE SONNER NE
  // SONNE PAS DEUX FOIS : son alarme vaut aussi pour la mention.
  if (pieces_.mentions.Advance(world_.chat_lines(), mention_rules, now_ms) && web.mentions &&
      !whisper_rang) {
    if (web.mention_sound && vfs_ != nullptr) {
      PlayWhisperAlarm(audio_, *vfs_, client_root_, /*alarm=*/true);
    }
    // La barre des tâches ne clignote que pour qui regarde ailleurs.
    if (web.mention_flash && gl_ != nullptr && gl_->window() != nullptr &&
        (SDL_GetWindowFlags(gl_->window()) & SDL_WINDOW_INPUT_FOCUS) == 0) {
      SDL_FlashWindow(gl_->window(), SDL_FLASH_UNTIL_FOCUSED);
    }
  }
  chat_tabs_->SeesRich(ChatRichContext{
      .markup = true,
      .item_icons = web.item_icons,
      .emote_images = web.emote_images,
      .pictures = web.pictures,
      .url_facts = [this](std::string_view written) { return pieces_.chat_web.FactsOf(written); },
      .picture = [this](
                     std::string_view url,
                     ro::ui::ChatInlineImage& image) { pieces_.chat_web.FillPicture(url, image); },
      .item_icon = [this](std::uint32_t item_id) -> ImTextureID {
        if (!pieces_.item_icons.has_value()) {
          return 0;
        }
        const ro::render::IconTexture& icon = pieces_.item_icons->Get(item_id);
        return icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : 0;
      },
      .naming_seqs =
          web.mentions ? pieces_.mentions.naming_seqs() : std::span<const std::uint64_t>()});
  chat_tabs_->SeesConversations(&pieces_.whispers.desk());
  chat_tabs_->MasksConversations(&pieces_.banned_words);
  chat_tabs_->SeesOwnName(world_.player_name());
  chat_tabs_->JudgesPlayers([this](std::string_view name) {
    const MvpInviteCircle hunt{
        .book = pieces_.mvp_tracker.book(),
        .guild = guild_book_,
        .friends = friends_,
        .enabled = settings_->options().pages().mvp_tracker.settings().enabled,
        .link_ready = pieces_.mvp_tracker.link_ready()};
    return JudgePlayerLink(name, PlayerLinkCircle{.party = party_,
                                                  .guild = guild_book_,
                                                  .friends = friends_,
                                                  .own_account_id = world_.player_id(),
                                                  .own_char_id = ticket_.character_id,
                                                  .mvp = &hunt});
  });
  // LES RESPAWNS DE MVP DU FIL : l'heure du serveur, qui dit lesquels sont
  // échus, et ce que leur menu sait d'avance — le carnet seul le sait.
  chat_tabs_->ReadsMvpClock(
      MvpLinkClock{.server_now_s = pieces_.mvp_tracker.ServerNow(LocalUnixSeconds()),
                   .book = &pieces_.mvp_tracker.book()});
  // LES LIENS DE RÉGLAGE DU FIL : une destination que le lecteur n'a pas — l'onglet
  // d'un compagnon absent — reste en texte inerte.
  chat_tabs_->ReadsSettingReach(SettingReachOf(pieces_));
  chat_tabs_->JudgesMvpLinks(
      [this](const MvpLink& link) { return pieces_.mvp_tracker.FactsFor(link); });
  // L'APPARENCE PRÊTE LE TEXTE RICHE à toutes les fenêtres : une copie, parce
  // que ce pointeur-là n'est pas un réglage.
  ro::ui::ChatAppearance look = settings_->options().chat_look();
  look.rich = &windows_.chat.rich;
  look.staff_warp = Rights().staff_tools;
  chat_tabs_->Refresh(windows_.chat, world_.chat_lines(), now_ms, look, look.diagnostic);
  // LE FIL DU SALON, APRÈS : il prend ses jetons de liens à la suite de ceux de
  // la conversation. Et la barre se prête à sa salle tant qu'il dure.
  chat_tabs_->RefreshRoom(pieces_.chat_rooms.room().lines(), pieces_.chat_rooms.room().room_seq());
  // « ENREGISTRER LE CHAT », choisi dans le menu d'un membre : le salon seul.
  if (pieces_.chat_rooms.TakeSaveAsked()) {
    SaveChatAndSay(*chat_tabs_, &pieces_.chat_rooms, /*tabs_too=*/false, world_, now_ms);
  }
  // LES CONVERSATIONS 1:1, de même.
  chat_tabs_->RefreshWhispers(world_.chat_lines(), pieces_.whispers.desk().conversations());
  windows_.chat.main.input_lent = pieces_.chat_rooms.LendsInput();
  // ⚠ ÉLAGUÉS AVANT D'ÊTRE PEINTS : un libellé que le joueur a effacé de sa
  // saisie n'a plus de pastille à porter, et sa place au plafond se libère.
  // ⚠ CE QUE LA SAISIE PORTERA À CETTE IMAGE, dépôt en attente compris : une
  // ligne retenue pour sa longueur y revient par lui, avec ses liens.
  const ro::ui::ChatInputState& input = windows_.chat.main.input;
  std::string typed = input.text.data();
  typed += input.pending_insert;
  PruneChatLinks(typed, &pending_links_);
  input_links_.clear();
  for (std::size_t rank = 0; rank < pending_links_.size(); ++rank) {
    // UN RESPAWN POSÉ SE RELIT DE SA BALISE : son menu sait ce que celui du fil
    // sait.
    const ro::ui::MvpLinkMenuFacts* facts = nullptr;
    if (MvpLink posed;
        pending_links_[rank].kind == ro::ui::ChatLinkKind::kMvp &&
        ParseMvpLink(pending_links_[rank].wire, &posed, nullptr) == MvpLinkRead::kRead) {
      facts = pieces_.mvp_tracker.FactsFor(posed);
    }
    // UNE RECHERCHE POSÉE DE MÊME : son menu sait si elle a un lieu.
    ro::ui::NaviSearchMenuFacts navi;
    if (NaviSearchLink posed; pending_links_[rank].kind == ro::ui::ChatLinkKind::kNaviSearch &&
                              ParseNaviSearchLink(pending_links_[rank].wire, &posed, nullptr) ==
                                  NaviSearchLinkRead::kRead) {
      navi = NaviSearchFactsOf(posed);
    }
    input_links_.push_back(ro::ui::ChatInputLink{.label = pending_links_[rank].shown,
                                                 .kind = pending_links_[rank].kind,
                                                 .item_id = pending_links_[rank].item_id,
                                                 .token = static_cast<std::uint32_t>(rank),
                                                 .mvp = facts,
                                                 .navi = navi});
  }
  // OÙ PART CE QU'ON ÉCRIT : la fenêtre ne peut pas le dire elle-même — les
  // sigils vivent dans `net`, et la place restante se compte dans la page de
  // codes du fil.
  // ⚠ REFAIT ICI, AVANT TOUT CE QUI LE LIT : la box a pu changer à l'image
  // précédente, et une vue composée une seule fois figerait le destinataire.
  armed_chat_.whisper_to = windows_.chat.main.input.whisper_to.data();
  // ⚠ LA VUE SE REPOSE À CHAQUE IMAGE : la chaîne qui la porte peut avoir
  // déménagé depuis la dernière, et une vue périmée armerait un canal au nom
  // illisible.
  armed_chat_.channel = armed_channel_;
  // « & » n'est un sigil que sur une carte de champ : l'aperçu et l'envoi le
  // lisent tous deux d'ici.
  armed_chat_.on_battlefield = world_.map_mode().battlefield;
  // Ctrl+Entrée ne vise le groupe que si l'on en a un.
  armed_chat_.in_party = party_.in_party();
  // ⚠ LE CHAMP DE BATAILLE NE RESTE PAS ARMÉ HORS DE SA CARTE : le serveur n'y a
  // plus d'équipe à qui relayer, et chaque ligne sans sigil serait refusée.
  if (armed_chat_.mode == ro::net::ChatTarget::kBattlefield && !world_.map_mode().battlefield) {
    armed_chat_.mode = ro::net::ChatTarget::kPublic;
  }
  // 🔴 LA PLACE SE COMPTE SUR LA LIGNE QUI PART : chaque libellé posé y reprend
  // sa balise, qui n'a pas sa taille. Compter la saisie affichée annoncerait
  // une place que le serveur coupera.
  const std::string outgoing = ResolveChatLinks(typed, pending_links_);
  chat_preview_.Refresh(outgoing.c_str(), WireSpeakerOf(world_, chat_preview_), armed_chat_,
                        pieces_.chat_rooms.room().in_room());
  // LES COMMANDES PROPOSÉES, à la mesure du groupe que le serveur a dit : un
  // compte du staff voit les siennes dès que le niveau arrive.
  chat_commands_.Refresh(world_.group_level(), pieces_.job_names);
  chat_preview_.SetCommands(chat_commands_.hints());
}

void Game::SendTypedChat(const UiStage::Answers& answers, std::uint32_t now_ms) {
  // Ce qu'on a tapé ne vient que de la saisie principale — prêtée ou non au
  // salon : elle seule a une saisie.
  for (const UiStage::ChatWindowAsk& ask : answers.chat) {
    if (!ask.asked.said.has_value()) {
      continue;
    }
    // ⚠ CE QUI PART N'EST PAS CE QUI EST TAPÉ : chaque libellé de lien y
    // reprend sa balise.
    SettledChatLine settled =
        SettleChatLine(*ask.asked.said, windows_.chat.main.input, &pending_links_,
                       WireSpeakerOf(world_, chat_preview_), armed_chat_);
    if (settled.held_notice.has_value()) {
      world_.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(*settled.held_notice), now_ms,
                        kHeldChatColor);
    } else if (settled.outgoing.has_value()) {
      SubmitChat(
          *chat_dispatch_, *settled.outgoing, now_ms,
          ro::net::ChatKeys{.party = ask.asked.said_keys.ctrl, .guild = ask.asked.said_keys.alt});
      // ⚠ LE DESTINATAIRE SE RETIENT D'APRÈS CE QUI PART VRAIMENT, pas d'après
      // la box : une ligne peut nommer quelqu'un d'autre par son `/w`, et c'est
      // celui-là qu'on voudra retrouver dans la liste.
      chat_preview_.recent_whispers().Note(
          ro::net::ReadChatIntent(*ask.asked.said, armed_chat_).recipient,
          ro::ui::ChatWhisperWay::kOutgoing);
    }
  }
  // L'ÉMOTICONE CLIQUÉE part en `:nom:`, seule sur sa ligne, par le mode armé.
  // ⚠ SANS LES LIENS POSÉS : ils appartiennent à la phrase qui attend dans la
  // saisie, et régler celle-ci les effacerait.
  for (const UiStage::ChatWindowAsk& ask : answers.chat) {
    if (ask.asked.sent_emote.empty()) {
      continue;
    }
    SettledChatLine settled =
        SettleChatLine(ask.asked.sent_emote, windows_.chat.main.input, nullptr,
                       WireSpeakerOf(world_, chat_preview_), armed_chat_);
    if (settled.outgoing.has_value()) {
      SubmitChat(*chat_dispatch_, *settled.outgoing, now_ms);
    }
  }
}

void Game::ApplyChatAnswers(const UiStage::Answers& answers, std::uint32_t now_ms) {
  // 🔴 APRÈS LE DESSIN DE TOUTES LES FENÊTRES, jamais entre deux : un
  // retrait, un déplacement ou un détachement réordonne la liste des onglets,
  // et la fenêtre suivante lirait des rangs périmés.
  // ⚠ TOUS CES GESTES PARLENT DE RANGS RELEVÉS AU TRI, donc d'avant toute
  // modification. C'est juste parce qu'IL N'Y EN A QU'UN PAR IMAGE : un joueur
  // n'a qu'une souris, et deux fenêtres ne reçoivent pas deux clics dans la
  // même image. Le jour où un geste viendrait d'ailleurs — une macro, un
  // raccourci — il faudrait retraduire entre deux.
  DamageMeterOptions& meter = settings_->options().pages().damage_meter;
  // L'ONGLET DU COMPTEUR LÂCHÉ HORS DE TOUTE BANDE redevient sa fenêtre, là
  // où on l'a lâché — et non une conversation qui ne porterait que lui.
  const ro::ui::ChatDrop& drop = answers.chat_drop;
  if (drop.asked && drop.to_new_window && chat_tabs_->IsMeter(drop.from_group, drop.from_tab)) {
    chat_tabs_->UndockMeter();
    windows_.damage_meter.place_at = drop.at;
  } else {
    chat_tabs_->ApplyDrop(drop);
  }
  for (const UiStage::ChatWindowAsk& ask : answers.chat) {
    // ⚠ AVANT `Apply`, qui réordonne : fermer la languette du compteur éteint
    // le compteur, comme la croix de sa fenêtre.
    if (chat_tabs_->IsMeter(ask.group, ask.asked.remove_tab)) {
      meter.SetShown(false);
      settings_->file().Touch();
    }
    chat_tabs_->Apply(ask.group, ask.asked, windows_.chat.For(ask.group));
    if (pieces_.damage_meter.Apply(ask.asked.meter, world_, meter)) {
      settings_->file().Touch();
    }
    if (ask.asked.meter.open_settings) {
      GameSettings::OpenOptionsAt(windows_, OptionSection::kDamageMeter);
    }
    FollowChatLink(ask.asked.link, now_ms);
    // La ligne validée est déjà partie, ou retenue : voir `SendTypedChat`.
    // ⚠ LE MODE S'ARME APRÈS L'ENVOI, et jamais avant : choisir une destination
    // ne doit pas changer où part la phrase qu'on vient de valider dans la même
    // image.
    // L'émoticone SANS NOM DE CHAT part TOUT DE SUITE en geste du jeu : elle
    // n'a pas de `:nom:` que le fil saurait peindre.
    if (ask.asked.chose_emotion >= 0 && link_.is_open()) {
      ReportRefusal(
          ro::net::RequestEmotion(link_, static_cast<std::uint16_t>(ask.asked.chose_emotion)));
    }
    if (ask.asked.forget_whispers) {
      chat_preview_.recent_whispers().Forget();
    }
    // LE MOT QUI REMPLACE LA BARRE PRÊTÉE ramène la salle devant.
    if (ask.asked.lent_input_asked) {
      pieces_.chat_rooms.Raise();
    }
    // LA PORTE VERS LES RÉGLAGES, à défaut d'une puce : le cadre de la chatbox
    // n'a pas de barre de titre où en poser une.
    if (ask.asked.settings_asked) {
      GameSettings::OpenOptionsAt(windows_, OptionSection::kChat);
    }
    // ⚠ CE QUE LE SÉLECTEUR REND EST UN RANG, ET UN RANG NE DIT PAS UN CANAL :
    // tous partagent le même genre. C'est la liste de l'image en cours qui
    // traduit, et le nom se RECOPIE — la liste, elle, sera refaite.
    const std::span<const ChatLinePreviewMaker::SendChoice> choices = chat_preview_.choices();
    if (ask.asked.chose_mode >= 0 &&
        static_cast<std::size_t>(ask.asked.chose_mode) < choices.size()) {
      const ChatLinePreviewMaker::SendChoice& chosen =
          choices[static_cast<std::size_t>(ask.asked.chose_mode)];
      armed_chat_.mode = chosen.target;
      armed_channel_ = chosen.channel;
      armed_chat_.channel = armed_channel_;
    }
    // CTRL + MOLETTE : l'onglet qui a son apparence la garde pour lui, sinon
    // c'est la taille générale qui bouge — par le même chemin que le panneau.
    if (ask.asked.zoom_steps != 0 && !chat_tabs_->ZoomLook(ask.group, ask.asked.zoom_steps)) {
      const int percent =
          std::clamp(settings_->options().chat_look().font_percent +
                         ask.asked.zoom_steps * ro::ui::kChatFontStepPercent,
                     ro::ui::kSmallestChatFontPercent, ro::ui::kLargestChatFontPercent);
      settings_->SetValue(GameOption::kChatFont, static_cast<float>(percent), OptionsTargets(),
                          now_ms);
    }
    // CE QU'UNE FLOTTANTE A PIOCHÉ va dans la saisie de la principale.
    if (!ask.asked.insert_into_main.empty() &&
        ro::ui::AppendPickedToChatInput(windows_.chat.main.input, ask.asked.insert_into_main)) {
      windows_.chat.main.input.unfolded = true;
    }
  }
  // ── LES ADRESSES ET LES IMAGES, APRÈS LE DESSIN ─────────────────────────
  // Les vignettes que le fil a posées sans les avoir, puis les réponses des
  // confirmations et de la visionneuse.
  pieces_.chat_web.RequestSeen(*chat_tabs_);
  pieces_.chat_web.Answer(windows_.chat, client_root_);
  ChatWebOptions& web = settings_->options().pages().chat_web;
  // ⚠ LE RÉGLAGE EST LA SEULE LISTE : l'étape la relit à l'image suivante, et
  // le fichier ne se réécrit que si elle a changé.
  if (const std::optional<std::string> host = pieces_.chat_web.TakeHostToAllow();
      host.has_value() && web.AddPreviewHost(*host)) {
    settings_->file().Touch();
  }
  if (const std::optional<std::string> host = pieces_.chat_web.TakeHostToOpenAlways();
      host.has_value() && web.AddOpenHost(*host)) {
    settings_->file().Touch();
  }
  if (const std::optional<ChatSaveOutcome> saved = pieces_.chat_web.TakeSaved();
      saved.has_value()) {
    world_.RecordChat(ro::sim::ChatKind::kServer, 0, SavedPictureLine(*saved), now_ms);
  }
}

}  // namespace moonclient
