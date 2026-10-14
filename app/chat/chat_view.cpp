#include "chat/chat_view.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <format>
#include <span>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/build_flavor.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/core/utf8_path.h"
#include "ro/net/action.h"
#include "ro/net/chat_target.h"
#include "ro/net/player_config.h"
#include "ro/net/rank_notices.h"
#include "ro/net/staff_requests.h"
#include "ro/net/warp_memo.h"
#include "ro/net/whisper.h"
#include "ro/net/whisper_blocks.h"
#include "ro/render/actor_pick.h"
#include "ro/render/screen_text.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/movement.h"
#include "ro/sim/skill_banner.h"
#include "ro/sim/skill_tree.h"
#include "ro/sim/world.h"

#include "chat/announce_command.h"
#include "chat/chat_line_color.h"
#include "chat/chat_line_preview.h"
#include "chat/chat_rich_decor.h"
#include "chat/chat_tab_view.h"
#include "chat/dev_commands.h"
#include "chat/native_slash_commands.h"
#include "chat/navigation_command.h"
#include "chat/rank_command.h"
#include "chat/show_equipment_command.h"
#include "chat/stat_raise_command.h"
#include "chat/talk_filter_command.h"
#include "chat/whisper_block_command.h"
#include "game/chat_room_stage.h"
#include "game/market_route.h"
#include "items/warp_memo_report.h"
#include "world/click_action.h"
#include "world/follow_action.h"
#include "world/navi_session.h"

namespace moonclient {
namespace {

// L'ÉCHO D'UN RÉGLAGE DU JEU basculé par sa commande : le jaune pâle de tous
// ces échos, en `0xRRGGBB`.
constexpr std::uint32_t kSettingEchoColor = 0xFFFF64;

// LA BASCULE DES REFUS DE SKILL : éteinte, le jeu tait l'échec sans motif
// précis et le délai de réutilisation — voir `SkillRefusalLine`.
constexpr std::string_view kSkillFailCommand = "/skillfail";

// LA BASCULE DU CHAT DE CHAMP DE BATAILLE : armée, tout ce qu'on tape sans sigil
// part à l'équipe.
constexpr std::string_view kBattleChatCommand = "/battlechat";

// La fraction de fin de vie sur laquelle une bulle s'efface. ⚠ LA FIN SEULE :
// une bulle qui pâlit dès son apparition se lit comme un défaut d'affichage.
constexpr float kBubbleFadeFraction = 0.25F;

}  // namespace

namespace {

// Un agrégat plutôt qu'une paire : le constructeur d'une paire prend ses
// arguments par valeur, et un littéral entier y passe par un int qu'il faut
// rétrécir. Écrit ainsi, chaque numéro est vérifié à la compilation.
struct Command {
  std::string_view name;
  std::uint16_t emotion;
};

// Les commandes d'émoticône du jeu, et le numéro qu'elles envoient.
//
// D'OÙ ELLES VIENNENT. Les numéros sont ceux de emotion_type, dans
// src/map/clif.hpp ; le serveur y écrit lui-même la commande de vingt-deux
// d'entre elles, en commentaire. Les autres sont celles de la liste classique
// du jeu, qui suit exactement le même ordre — de la deuxième à la
// trente-quatrième, sans trou.
constexpr std::array<Command, 66> kEmotionCommands = {{
    {"/!", 0},        {"/?", 1},       {"/ho", 2},      {"/delight", 2},   {"/lv", 3},
    {"/heart", 3},    {"/swt", 4},     {"/sweat", 4},   {"/ic", 5},        {"/aha", 5},
    {"/an", 6},       {"/fret", 6},    {"/ag", 7},      {"/anger", 7},     {"/$", 8},
    {"/money", 8},    {"/...", 9},     {"/gawi", 10},   {"/scissors", 10}, {"/bawi", 11},
    {"/rock", 11},    {"/bo", 12},     {"/paper", 12},  {"/korea", 13},    {"/lv2", 14},
    {"/thx", 15},     {"/thk", 15},    {"/wah", 16},    {"/sry", 17},      {"/heh", 18},
    {"/swt2", 19},    {"/hmm", 20},    {"/no1", 21},    {"/??", 22},       {"/omg", 23},
    {"/oh", 24},      {"/X", 25},      {"/hlp", 26},    {"/go", 27},       {"/sob", 28},
    {"/gg", 29},      {"/kis", 30},    {"/kis2", 31},   {"/pif", 32},      {"/ok", 33},
    {"/bzz", 36},     {"/stare", 36},  {"/awsm", 38},   {"/cool", 38},     {"/pat", 41},
    {"/goodboy", 41}, {"/mp", 42},     {"/sptime", 42}, {"/com", 44},      {"/comeon", 44},
    {"/yawn", 45},    {"/sleepy", 45}, {"/grat", 46},   {"/congrats", 46}, {"/hp", 47},
    {"/hptime", 47},  {"/fsh", 52},    {"/spin", 53},   {"/dum", 55},      {"/crwd", 56},
    {"/otl", 57},
}};

// La commande de NUMÉRO, qui n'existe pas dans le jeu : elle est à nous, pour
// atteindre les quatre-vingt-dix-huit émoticônes du sprite, y compris celles
// qu'aucune commande ne nomme.
constexpr std::string_view kNumberedEmotion = "/e ";

// Le premier octet d'un caractère UTF-8 n'a jamais la forme 10xxxxxx : c'est à
// cela qu'on reconnaît les octets de CONTINUATION.
constexpr std::uint8_t kContinuationMask = 0xC0;
constexpr std::uint8_t kContinuationBits = 0x80;

// Ce que le journal montre : blanc pour une parole, jaune pâle pour ce que le
// serveur écrit — cela ne vient de personne et ne doit pas se lire comme une
// voix.
constexpr std::array<std::uint8_t, 4> kSpokenColor = {255, 255, 255, 255};
constexpr std::array<std::uint8_t, 4> kServerColor = {255, 235, 160, 255};
// La ligne en cours de saisie, et son signe d'attente.
constexpr std::array<std::uint8_t, 4> kTypedColor = {180, 230, 255, 255};
constexpr std::string_view kPrompt = "> ";
constexpr std::string_view kCaret = "_";

// Le numéro que porte une commande numérotée, ou rien.
std::optional<std::uint16_t> NumberedEmotion(std::string_view line) {
  if (!line.starts_with(kNumberedEmotion)) {
    return std::nullopt;
  }
  const std::string_view digits = line.substr(kNumberedEmotion.size());
  if (digits.empty()) {
    return std::nullopt;
  }
  unsigned value = 0;
  for (const char digit : digits) {
    if (digit < '0' || digit > '9') {
      return std::nullopt;
    }
    value = value * 10 + static_cast<unsigned>(digit - '0');
    if (value > 255) {
      return std::nullopt;
    }
  }
  return static_cast<std::uint16_t>(value);
}

}  // namespace

bool IsCraftAtlasCommand(std::string_view line) {
  // Le nom s'arrête au premier blanc ; ce qui suit ne compte pas.
  const std::string_view name = line.substr(0, line.find_first_of(" \t"));
  const auto folded_equal = [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  };
  return std::ranges::any_of(kCraftAtlasCommands, [&](std::string_view command) {
    return std::ranges::equal(name, command, folded_equal);
  });
}

std::optional<std::uint16_t> EmotionCommand(std::string_view line) {
  for (const Command& command : kEmotionCommands) {
    if (line == command.name) {
      return command.emotion;
    }
  }
  return NumberedEmotion(line);
}

std::string_view EmotionCommandOf(std::uint16_t emotion) {
  for (const Command& command : kEmotionCommands) {
    if (command.emotion == emotion) {
      return command.name;
    }
  }
  return {};
}

std::vector<ro::ui::ChatBubbleView> ChatBubblesOnScreen(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, std::uint32_t now_ms, float viewport_width, float viewport_height,
    const ro::ui::ChatBubbleLook& look, const ro::vfs::ItemTable* items,
    const ro::vfs::ItemWords* words, std::deque<DecoratedChatLine>* decorated,
    const ro::sim::SkillTree* skills, const CreatorNames* creators,
    const ro::vfs::MapDisplayNames* maps, const ItemIconLookup* item_icon,
    const SettingReach& settings) {
  static const ItemIconLookup kNoIcon;
  const ItemIconLookup& icon_of = item_icon != nullptr ? *item_icon : kNoIcon;
  std::vector<ro::ui::ChatBubbleView> bubbles;
  if (decorated != nullptr) {
    decorated->clear();
  }
  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    const ro::sim::Actor* actor = world.Find(ids[index]);
    if (actor == nullptr) {
      continue;
    }
    // LA PLUS FRAÎCHE DES DEUX L'EMPORTE : une phrase et une bannière occupent
    // le même emplacement chez le client, qui les envoie par le même message.
    const bool banner =
        actor->announced_skill_id != 0 && actor->announced_skill_since_ms >= actor->chat_since_ms;
    // ⚠ « NE PAS MONTRER MA BULLE » NE CACHE PAS MA BANNIÈRE : ce réglage
    // porte sur le bavardage, et le client n'en tient pas compte pour le nom
    // d'une compétence — qu'il montre au-dessus du lanceur, moi compris.
    if (!banner && !look.show_own && ids[index] == world.player_id()) {
      continue;
    }
    // 🔴 LA VUE D'UNE BULLE POINTE, ELLE NE COPIE PAS. Une phrase appartient à
    // l'acteur et lui survit ; une bannière, elle, se COMPOSE ici, et une chaîne
    // locale meurt à la fin du tour de boucle — les bulles rendues montraient
    // alors des losanges. C'est pour cela que `decorated` existe : il garde ce
    // qui n'appartient à personne d'autre, et une FILE ne déplace pas ce qu'on y
    // a déjà mis.
    if (banner && decorated == nullptr) {
      continue;
    }
    if (banner) {
      DecoratedChatLine compose;
      // ⚠ UN PARTENAIRE INCONNU DONNE UN NOM VIDE, pas le nom de la compétence :
      // le dictionnaire des noms du client rend une chaîne vide pour une entité
      // qu'il ne tient pas — c'est le cas d'un monstre, qui passe son niveau.
      const ro::sim::Actor* partner = world.Find(actor->announced_target_id);
      if (ro::sim::SkillBannerNamesTarget(actor->announced_skill_id) && words != nullptr) {
        // Sans sa phrase, il n'y a pas de bannière : le texte reste vide et la
        // bulle ne s'écrit pas.
        const std::string_view love_text = ro::vfs::LoveSkillText(*words);
        if (!love_text.empty()) {
          compose.text = (partner != nullptr ? partner->name : std::string()) +
                         std::string(ro::sim::kSkillBannerSuffix) + std::string(love_text);
        }
      } else {
        compose.text =
            ro::sim::SkillBannerText(skills, actor->announced_skill_id, world.group_level() > 0);
      }
      decorated->push_back(std::move(compose));
    }
    const std::string_view said =
        banner ? std::string_view(decorated->back().text) : std::string_view(actor->chat_text);
    if (said.empty()) {
      continue;
    }
    const std::uint32_t since_ms = banner ? actor->announced_skill_since_ms : actor->chat_since_ms;
    const float elapsed_ms = now_ms > since_ms ? static_cast<float>(now_ms - since_ms) : 0.0F;
    // Au-delà de la plus longue vie possible, rien ne se décore pour le savoir.
    if (elapsed_ms >= ro::ui::ChatBubbleLongestLifeMs(look)) {
      continue;
    }
    const std::optional<ro::render::ScreenRect> bounds = ro::render::ActorSpeakerScreenBounds(
        camera, aspect_ratio, drawn[index], viewport_width, viewport_height);
    if (!bounds.has_value()) {
      continue;
    }
    // Le MILIEU DU HAUT du corps et de la tête : c'est de là que la bulle monte,
    // et c'est tout ce que `ui` a besoin de savoir du monde.
    ro::ui::ChatBubbleView bubble{
        .head = ImVec2((bounds->min.x + bounds->max.x) * 0.5F, bounds->min.y),
        .text = said,
        .elapsed_ms = elapsed_ms,
        .text_color = banner ? ImU32{0} : ColorOfBubbleBgr(actor->chat_bgr),
        .speaker_id = ids[index]};
    // ⚠ RIEN N'EST RECOPIÉ SANS BALISE NI BALISAGE : la quasi-totalité des
    // phrases n'en porte pas, et leur bulle continue de regarder le texte du
    // monde.
    // ⚠ PAS POUR UNE BANNIÈRE : un nom de compétence ne porte aucune balise, et
    // la réécrire pousserait une SECONDE entrée dans la file en laissant la vue
    // sur la première.
    if (!banner && decorated != nullptr) {
      DecoratedChatLine rewritten;
      // UN ITEM FORGÉ LIÉ se nomme par son forgeron, comme dans le fil.
      if (DecorateBubbleLine(said,
                             ChatDecorTables{.items = items,
                                             .words = words,
                                             .creators = creators,
                                             .maps = maps,
                                             .settings = &settings},
                             icon_of, &rewritten)) {
        decorated->push_back(std::move(rewritten));
        const DecoratedChatLine& kept = decorated->back();
        bubble.text = kept.text;
        bubble.links = kept.links;
        bubble.styles = kept.styles;
        bubble.images = kept.images;
      }
    }
    // 🔴 LA VIE SE MESURE SUR LE TEXTE MONTRÉ, donc après la décoration : une
    // balise d'objet ou un `**` ne rallonge pas la bulle.
    if (elapsed_ms >= ro::ui::ChatBubbleLifeMs(look, bubble.text, bubble.images)) {
      continue;
    }
    bubbles.push_back(bubble);
  }
  return bubbles;
}

namespace {

// `/navi` ET `/navigation`. Rend faux quand la ligne n'en est pas une.
[[nodiscard]] bool RunNavigationCommand(const ChatDispatch& to, const std::string& line,
                                        std::uint32_t said_ms) {
  const NavigationCommand command = ReadNavigationCommand(line);
  if (command.kind == NavigationCommandKind::kNone) {
    return false;
  }
  switch (command.kind) {
    case NavigationCommandKind::kGuide:
      to.navi->GuideToPlace(command.place.map, command.place.x, command.place.y);
      to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                           PlaceDestinationLine(command.place, to.map_names), said_ms);
      break;
    case NavigationCommandKind::kSearch: {
      ro::ui::OpenNavigationSearch(*to.navigation, command.term, /*monsters_only=*/false);
      // ⚠ LA FENÊTRE NE LANCE PAS LA RECHERCHE À L'OUVERTURE, seulement à la
      // frappe : c'est ici qu'elle part, ou à la fin de la lecture des tables.
      to.navi->Search(command.term, /*only_map=*/{}, to.world->map_name());
      break;
    }
    case NavigationCommandKind::kOpen:
      to.navigation->open = true;
      to.navigation->raise = true;
      break;
    case NavigationCommandKind::kNone:
      break;
  }
  return true;
}

// `/where`, et les annonces du staff. Rend faux quand la ligne n'en est pas une.
// Voir `announce_command.h`.
[[nodiscard]] bool RunAnnounceCommand(const ChatDispatch& to, const std::string& line,
                                      std::uint32_t said_ms) {
  const AnnounceCommand command = ReadAnnounceCommand(line);
  switch (command.kind) {
    case AnnounceCommandKind::kNone:
      return false;
    case AnnounceCommandKind::kWhere: {
      // LA CASE OÙ L'ON SE TIENT, et non le départ du pas en cours.
      if (const ro::sim::Actor* self = to.world->player(); self != nullptr) {
        const ro::sim::CellPosition cell = ro::sim::StandingCell(*self);
        to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                             WhereLine(to.world->map_name(), cell.x, cell.y), said_ms,
                             kSettingEchoColor);
      }
      return true;
    }
    case AnnounceCommandKind::kBroadcast:
    case AnnounceCommandKind::kLocalBroadcast:
      break;
  }
  // ⚠ UN GARDE D'AFFICHAGE, PAS UNE PERMISSION : le serveur revalide selon le
  // groupe. Il empêche seulement qu'un joueur croie avoir annoncé — le refus du
  // serveur est muet — et que sa ligne parte en parole publique.
  if (!to.link->is_open() || to.world->group_level() == 0) {
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         ro::core::Tr("Les annonces sont réservées au staff."), said_ms);
    return true;
  }
  const std::string text = AnnouncedText(command, to.world->player_name());
  if (text.empty()) {
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         ro::core::Tr("Rien à annoncer : écris le message après la commande."),
                         said_ms);
    return true;
  }
  ReportRefusal(command.kind == AnnounceCommandKind::kBroadcast
                    ? ro::net::RequestBroadcast(*to.link, text)
                    : ro::net::RequestLocalBroadcast(*to.link, text));
  return true;
}

// `/ex`, `/in`, `/exall`, `/inall`. Rend faux quand la ligne n'en est pas une.
// Voir `whisper_block_command.h`. Hors ligne, rien à demander à personne.
[[nodiscard]] bool RunWhisperBlockCommand(const ChatDispatch& to, const std::string& line,
                                          std::uint32_t said_ms) {
  const WhisperBlockCommand command = ReadWhisperBlockCommand(line);
  if (command.kind == WhisperBlockCommandKind::kNone) {
    return false;
  }
  if (command.kind == WhisperBlockCommandKind::kAllowWithoutName) {
    to.world->RecordChat(
        ro::sim::ChatKind::kServer, 0,
        ro::core::Tr("/in (nom du personnage) : autorise de nouveau ses chuchotements"), said_ms);
    return true;
  }
  if (!to.link->is_open() || to.whisper_blocks == nullptr) {
    return true;
  }
  switch (command.kind) {
    case WhisperBlockCommandKind::kBlock:
    case WhisperBlockCommandKind::kAllow:
      if (!ro::net::RequestWhisperBlock(*to.link, *to.whisper_blocks, command.name,
                                        command.kind == WhisperBlockCommandKind::kBlock
                                            ? ro::sim::WhisperBlockWay::kBlock
                                            : ro::sim::WhisperBlockWay::kAllow)) {
        to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                             ro::core::Tr("Ce nom ne peut pas partir tel quel : il est trop "
                                          "long, ou porte un caractère que le jeu n'écrit pas."),
                             said_ms);
      }
      break;
    case WhisperBlockCommandKind::kList:
      ReportRefusal(ro::net::RequestWhisperBlockList(*to.link));
      break;
    case WhisperBlockCommandKind::kBlockEveryone:
    case WhisperBlockCommandKind::kAllowEveryone:
      ReportRefusal(ro::net::RequestWhisperBlockEveryone(
          *to.link, command.kind == WhisperBlockCommandKind::kBlockEveryone
                        ? ro::sim::WhisperBlockWay::kBlock
                        : ro::sim::WhisperBlockWay::kAllow));
      break;
    case WhisperBlockCommandKind::kNone:
    case WhisperBlockCommandKind::kAllowWithoutName:
      break;
  }
  return true;
}

// `/battlechat` : la barre bascule entre le champ de bataille et la voix haute.
// Hors d'une carte de champ, rien ne s'arme — la barre y reviendrait d'elle-même
// à la voix haute à l'image suivante.
void ToggleBattleChat(const ChatDispatch& to, std::uint32_t said_ms) {
  if (!to.world->map_mode().battlefield) {
    to.world->RecordChat(
        ro::sim::ChatKind::kServer, 0,
        ro::core::Tr("Le chat de champ de bataille ne s'arme que sur une carte de champ de "
                     "bataille."),
        said_ms);
    return;
  }
  const bool armed = to.arming->mode != ro::net::ChatTarget::kBattlefield;
  to.arming->mode = armed ? ro::net::ChatTarget::kBattlefield : ro::net::ChatTarget::kPublic;
  to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                       armed ? ro::core::Tr("Chat de champ de bataille : tout ce que tu écris "
                                            "part à ton équipe. [/battlechat ON]")
                             : ro::core::Tr("Chat de champ de bataille : coupé, tu parles de "
                                            "nouveau à voix haute. [/battlechat OFF]"),
                       said_ms, kSettingEchoColor);
}

// ENVOIE UNE LIGNE AU SERVEUR, là où elle dit aller.
//
// ⚠ LA MÊME LECTURE QUE CELLE DE L'ÉTIQUETTE, avec le même mode armé : c'est ce
// qui garantit que la phrase part là où la barre l'annonçait. Et un chuchotement
// parti attend son accusé : c'est lui qui en composera l'écho.
void SendLine(const ChatDispatch& to, const std::string& line, std::uint32_t said_ms) {
  ro::net::ChatArming armed = to.armed != nullptr ? *to.armed : ro::net::ChatArming{};
  // ⚠ LA CARTE SE RELIT ICI, au moment de l'envoi : une macro peut parler sans
  // que la barre ait été recomposée depuis le dernier changement de carte.
  armed.on_battlefield = to.world->map_mode().battlefield;
  // ⚠ LA PAROLE PART SES MOTS INTERDITS MASQUÉS, quel que soit son canal : le
  // joueur n'est pas arrêté, ses mots si.
  ro::net::ChatIntent intent = ro::net::ReadChatIntent(line, armed);
  intent.text = ro::sim::BannedWords::MaskWith(to.banned, intent.text);
  // ⚠ HORS D'UNE CARTE DE CHAMP, LE SERVEUR N'A AUCUNE ÉQUIPE À QUI RELAYER, et
  // il jette la ligne sans un mot : c'est donc ici qu'on le dit.
  if (intent.target == ro::net::ChatTarget::kBattlefield) {
    if (!to.world->map_mode().battlefield) {
      to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                           ro::core::Tr("Tu n'es pas sur un champ de bataille : personne ne "
                                        "lirait ce message."),
                           said_ms);
      return;
    }
    if (intent.text.empty()) {
      to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                           ro::core::Tr("Rien à dire : écris le message après & ou /bc."), said_ms);
      return;
    }
  }
  const ro::core::Result<void> sent = ro::net::SendChat(*to.link, to.world->player_name(), intent,
                                                        to.world->map_mode().battlefield);
  if (sent && to.whispers != nullptr) {
    static_cast<void>(ro::net::NoteWhisperSent(
        *to.whispers, intent, said_ms,
        ro::net::WhisperNoting{.muted = ro::sim::CannotChat(to.world->player_statuses(),
                                                            to.world->player_status().manner),
                               .channels = to.world->chat_channels()}));
  }
  // ⚠ LE LIEN D'UN CANAL SE SUIT ICI, où passe TOUTE ligne partie au serveur —
  // tapée ou envoyée par une macro —, et quelle que soit sa destination : le
  // serveur exécute la commande d'où qu'elle vienne.
  if (sent && to.line_preview != nullptr) {
    to.line_preview->FollowSentCommand(intent.text, to.world->chat_channels());
  }
  ReportRefusal(sent);
}

}  // namespace

void SaveChatAndSay(const ChatTabsView& tabs, const ChatRoomStage* rooms, bool tabs_too,
                    ro::sim::World& world, std::uint32_t said_ms) {
  ChatLogDump done;
  if (tabs_too) {
    done = tabs.SaveLogs();
  }
  if (rooms != nullptr) {
    if (const std::optional<ro::sim::ChatRoomInside>& inside = rooms->room().inside();
        inside.has_value()) {
      const ChatLogDump room = tabs.SaveRoomLog(rooms->room().lines(), inside->title);
      done.files += room.files;
      done.folder = room.folder;
    }
  }
  // ⚠ ON DIT LE DOSSIER, pas seulement le succès. Le client d'origine annonce
  // « … is Saved. » sans jamais nommer l'endroit, et sa commande est MUETTE
  // quand l'écriture échoue — on a assez souffert de commandes qui ne disent
  // rien pour ne pas en fabriquer une de plus.
  world.RecordChat(ro::sim::ChatKind::kServer, 0,
                   done.files > 0
                       ? std::format("journal : {} fichier(s) dans {}", done.files,
                                     ro::core::Utf8FromPath(done.folder))
                       : std::string("journal : rien à enregistrer, aucun fil ne montre de ligne"),
                   said_ms);
}

void SubmitChat(const ChatDispatch& to, const std::string& line, std::uint32_t said_ms) {
  if (line.empty()) {
    return;
  }
  // ⚠ « /bc texte » ET « /w nom texte » SONT DES PAROLES, pas des commandes du
  // client : elles partent comme les sigils, avant que la table des commandes
  // connues ne les taise.
  if (ro::net::IsSpokenLine(line)) {
    // À QUI, C'EST LA LIGNE QUI LE DIT : les sigils du jeu, pas un sélecteur.
    // Voir ro/net/chat_target.h, qui porte la table et ce que le serveur en fait.
    if (to.link->is_open()) {
      SendLine(to, line, said_ms);
    }
    return;
  }
  if (const std::optional<std::uint16_t> emotion = EmotionCommand(line); emotion.has_value()) {
    if (to.link->is_open()) {
      ReportRefusal(ro::net::RequestEmotion(*to.link, *emotion));
    }
    return;
  }
  // LES COMMANDES DE DÉVELOPPEMENT, en Debug seulement : voir `dev_commands.h`.
  if constexpr (ro::core::kDevTools) {
    if (RunDevelopmentCommand(to, line, said_ms)) {
      return;
    }
  }
  // À la casse près, comme toute la table des commandes du jeu.
  if (ro::core::AsciiEqualsFolded(line, kBattleChatCommand) && to.arming != nullptr) {
    ToggleBattleChat(to, said_ms);
    return;
  }
  if (line == kFixedCameraCommand && to.options != nullptr) {
    const bool fixed = to.options->ToggleFixedCamera(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         fixed ? "caméra : collée au personnage" : "caméra : elle rattrape",
                         said_ms);
    return;
  }
  if (line == kExpandedZoomCommand && to.options != nullptr) {
    const bool on = to.options->ToggleExpandedZoom(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         on ? "dézoom étendu : allumé" : "dézoom étendu : éteint", said_ms);
    return;
  }
  if (line == kLightmapCommand && to.options != nullptr) {
    const bool on = to.options->ToggleLightmaps(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         on ? "ombres de la carte : affichées" : "ombres de la carte : masquées",
                         said_ms);
    return;
  }
  if (line == kFogCommand && to.options != nullptr) {
    const bool on = to.options->ToggleFog(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         on ? "brouillard : affiché" : "brouillard : masqué", said_ms);
    return;
  }
  if (line == kCashShopButtonCommand && to.options != nullptr) {
    const bool shown = to.options->ToggleCashShopButton(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         shown ? ro::core::Tr("bouton du Vote Shop : affiché")
                               : ro::core::Tr("bouton du Vote Shop : masqué"),
                         said_ms);
    return;
  }
  if ((line == kNoShiftCommand || line == kNoShiftShortCommand) && to.options != nullptr) {
    const bool on = to.options->ToggleNoShift(to.option_targets);
    to.world->RecordChat(
        ro::sim::ChatKind::kServer, 0,
        on ? "sans Maj : la cible se force sans la touche" : "sans Maj : tenir Maj pour forcer",
        said_ms);
    return;
  }
  if ((line == kNoCtrlCommand || line == kNoCtrlShortCommand) && to.options != nullptr) {
    const bool on = to.options->ToggleNoCtrl(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         on ? "sans Ctrl : l'attaque se poursuit d'elle-même"
                            : "sans Ctrl : tenir Ctrl pour poursuivre",
                         said_ms);
    return;
  }
  if ((line == kShoppingCommand || line == kShoppingShortCommand) && to.options != nullptr) {
    const bool on = to.options->ToggleShopSetting(ShopRow::kOneClick, to.option_targets);
    to.world->RecordChat(
        ro::sim::ChatKind::kServer, 0,
        on ? ro::core::Tr("Une échoppe s'ouvre désormais d'un simple clic gauche, et se referme "
                          "d'un simple clic droit. [/sh ON]")
           : ro::core::Tr("Une échoppe s'ouvre par un double-clic. [/sh OFF]"),
        said_ms, kSettingEchoColor);
    return;
  }
  if (line == kSkillFailCommand && to.options != nullptr) {
    const bool shown = !to.options->skill_fail_shown();
    to.options->Change(
        ro::ui::OptionsWindowAction{.changed = static_cast<int>(GameOption::kSkillFailShown),
                                    .on = shown},
        to.option_targets, said_ms);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         shown ? ro::core::Tr("Les messages d'échec de skill seront affichés. "
                                              "[Affichage ON]")
                               : ro::core::Tr("Les messages d'échec de skill ne seront plus "
                                              "affichés. [Affichage OFF]"),
                         said_ms, kSettingEchoColor);
    return;
  }
  if (const std::optional<TalkFilterRow> talk = ReadTalkFilterCommand(line);
      talk.has_value() && to.options != nullptr) {
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         ToggleTalkFilter(*talk, *to.options, to.option_targets, said_ms), said_ms,
                         kSettingEchoColor);
    return;
  }
  if (line == kCallCommand && to.options != nullptr) {
    if (!to.options->pages().call.RequestToggle()) {
      to.world->RecordChat(ro::sim::ChatKind::kCall, 0,
                           ro::core::Tr("Le serveur n'a pas encore dit si l'appel d'urgence "
                                        "vous atteint : réessayez une fois en jeu. (/call)"),
                           said_ms);
    }
    return;
  }
  // « MONTRER MON ÉQUIPEMENT » : la même demande que la case de la fiche, et
  // la réponse s'écrit à l'écho. Voir `show_equipment_command.h`.
  if (IsShowEquipmentCommand(line) && to.own_config != nullptr) {
    if (to.link->is_open()) {
      ReportRefusal(ro::net::RequestShowEquipment(
          *to.link, EquipmentShownAsked(to.own_config->equipment_shown())));
    }
    return;
  }
  if ((line == kChatRoomCommand || line == kChatRoomHangulCommand) && to.chat_rooms != nullptr) {
    to.chat_rooms->OpenMake();
    return;
  }
  if (line == kChatRoomLeaveCommand && to.chat_rooms != nullptr && to.chat_rooms->AskLeave()) {
    return;
  }
  if (line == kShowShopCommand && to.options != nullptr) {
    static_cast<void>(to.options->ToggleShopSetting(ShopRow::kSignboardsShown, to.option_targets));
    return;
  }
  if ((line.starts_with(kScaleCommand) || line.starts_with(kScaleAltCommand)) &&
      to.options != nullptr) {
    const std::string_view command =
        line.starts_with(kScaleCommand) ? kScaleCommand : kScaleAltCommand;
    // Sans argument lisible on ne pose RIEN et on dit l'échelle en cours : une
    // faute de frappe ne doit pas changer l'interface, ni faire croire qu'elle
    // a changé.
    float scale = to.options->interface_scale();
    if (const std::optional<float> wanted = ro::core::NumberAfterCommand(line, command);
        wanted.has_value()) {
      scale = to.options->SetInterfaceScale(*wanted, to.option_targets);
    }
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0, std::format("echelle : {:.2f}", scale),
                         said_ms);
    return;
  }
  if ((line == kBattleModeCommand || line == kBattleModeCommandLong) && to.options != nullptr) {
    const bool folded = to.options->ToggleBattleMode(to.option_targets);
    // ⚠ DIT DANS LA CONVERSATION, et c'est le seul endroit possible : la barre
    // vient de disparaître, et un joueur qui ne saurait pas comment la ramener
    // croirait le client cassé. La phrase porte donc la touche.
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         folded ? ro::core::Tr("battle mode : la barre de saisie se replie — "
                                               "Entrée l'ouvre, Échap la referme")
                                : ro::core::Tr("battle mode : la barre de saisie reste là"),
                         said_ms);
    return;
  }
  if (line == kMusicCommand && to.options != nullptr) {
    const bool audible = to.options->ToggleMusic(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         audible ? "musique : rétablie" : "musique : coupée", said_ms);
    return;
  }
  if ((line == kSoundCommand || line == kSoundCommandLong) && to.options != nullptr) {
    const bool audible = to.options->ToggleSound(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         audible ? "bruits : rétablis" : "bruits : coupés", said_ms);
    return;
  }
  if (line == kEffectsShownCommand && to.options != nullptr) {
    const bool shown = to.options->ToggleEffectsShown(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         shown ? "effets : affichés" : "effets : coupés", said_ms);
    return;
  }
  if ((line == kMinEffectCommand || line == kMinEffectAltCommand) && to.options != nullptr) {
    const bool minimal = to.options->ToggleMinimalEffects(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         minimal ? "effets : allégés" : "effets : complets", said_ms);
    return;
  }
  if (line == kSkyEffectsCommand && to.options != nullptr) {
    const bool shown = to.options->ToggleSkyEffectsShown(to.option_targets);
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         shown ? "effets du ciel : affichés" : "effets du ciel : coupés", said_ms);
    return;
  }
  if (to.craft_atlas != nullptr && IsCraftAtlasCommand(line)) {
    to.craft_atlas->open = true;
    to.craft_atlas->raise = true;
    return;
  }
  if (to.navi != nullptr && to.navigation != nullptr && RunNavigationCommand(to, line, said_ms)) {
    return;
  }
  if (RunAnnounceCommand(to, line, said_ms)) {
    return;
  }
  // `/follow` n'est pas du client d'origine : sans cette prise, elle partirait
  // au serveur en parole publique.
  if (to.follow != nullptr && RunFollowCommand(*to.follow, *to.world, line, said_ms)) {
    return;
  }
  if (RunWhisperBlockCommand(to, line, said_ms)) {
    return;
  }
  if (line == kMacroWindowCommand && to.macro_window != nullptr) {
    *to.macro_window = !*to.macro_window;
    to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                         *to.macro_window ? "macros : ouvert" : "macros : ferme", said_ms);
    return;
  }
  if (line == kSaveChatCommand && to.chat_tabs != nullptr) {
    SaveChatAndSay(*to.chat_tabs, to.chat_rooms, /*tabs_too=*/true, *to.world, said_ms);
    return;
  }
  // LE TABLEAU DES CLASSEMENTS. ⚠ LA LIGNE SE PREND MÊME SANS TABLEAU : une
  // commande de ce client ne se dit jamais en public.
  if (IsRankingBoardCommand(line)) {
    if (to.ranking_board_asked != nullptr) {
      *to.ranking_board_asked = true;
    }
    return;
  }
  // LE MARCHÉ. Même règle : la ligne se prend même sans Marché.
  if (IsMarketCommand(line)) {
    if (to.market != nullptr) {
      to.market->AskOpen();
    }
    return;
  }
  // LES CLASSEMENTS : la réponse s'écrit quand elle arrive, ses noms avec —
  // voir `RankNotices`.
  if (const std::optional<ro::net::RankKind> rank = ReadRankCommand(line); rank.has_value()) {
    if (to.link->is_open()) {
      ro::net::RequestRanking(*to.link, *rank);
    }
    return;
  }
  // LA MÉMORISATION DU POINT DE WARP. La demande part sans champ, et la réponse
  // s'écrit quand elle arrive — voir `ro/net/warp_memo.h`. Hors ligne, rien à
  // demander à personne.
  if (IsWarpMemoCommand(line)) {
    if (to.link->is_open()) {
      ReportRefusal(ro::net::RequestWarpMemo(*to.link));
    }
    return;
  }
  if (RunStatRaiseCommand(*to.link, *to.world, line)) {
    return;
  }
  // 🔴 CE QUE LE CLIENT DU JOUEUR NE CONNAÎT PAS PART AU SERVEUR, comme une
  // parole ordinaire et barre oblique comprise : tous la lisent. Ce qu'il
  // connaît et que nous n'avons pas repris se tait. Voir
  // `NativeKnowsSlashCommand`.
  if (!NativeKnowsSlashCommand(line)) {
    if (to.link->is_open()) {
      SendLine(to, line, said_ms);
    }
    return;
  }
  to.world->RecordChat(ro::sim::ChatKind::kServer, 0, "commande inconnue : " + line, said_ms);
}

}  // namespace moonclient
