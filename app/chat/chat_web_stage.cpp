#include "chat/chat_web_stage.h"

#include <algorithm>
#include <cstddef>
#include <system_error>
#include <utility>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/core/web_address.h"
#include "ro/render/emotion.h"
#include "ro/ui/chat_emote_names.h"

#include "capture/screenshot_folder.h"

namespace moonclient {
namespace {

// Les adresses dont on garde les faits d'une image à l'autre. Au-delà, tout
// est oublié au `Compose` suivant, et relu au besoin : un fil montre bien
// moins d'adresses à la fois.
constexpr std::size_t kMostKnownUrls = 256;

constexpr std::string_view kPlainPrefix = "http://";
constexpr std::string_view kSecurePrefix = "https://";

[[nodiscard]] char LowerAscii(char letter) {
  return (letter >= 'A' && letter <= 'Z') ? static_cast<char>(letter - 'A' + 'a') : letter;
}

[[nodiscard]] bool StartsNoCase(std::string_view text, std::string_view lowered_prefix) {
  return text.size() >= lowered_prefix.size() &&
         std::equal(lowered_prefix.begin(), lowered_prefix.end(), text.begin(),
                    [](char wanted, char letter) { return wanted == LowerAscii(letter); });
}

// 🔴 UNE ADRESSE OUVRE UN PROGRAMME DU SYSTÈME, et elle vient d'un autre
// joueur : seuls `http://` et `https://`, majuscules ou non, partent au
// navigateur. Un autre schéma ouvrirait ce que le système lui associe.
[[nodiscard]] bool OpensInBrowser(std::string_view openable) {
  return StartsNoCase(openable, kPlainPrefix) || StartsNoCase(openable, kSecurePrefix);
}

void LaunchBrowser(const std::string& openable) {
  if (!OpensInBrowser(openable)) {
    ro::core::LogWarning("chat : adresse refusée — {}", openable);
    return;
  }
  if (!SDL_OpenURL(openable.c_str())) {
    ro::core::LogWarning("chat : {} n'a pas pu s'ouvrir", openable);
  }
}

// Une liste de sites telle que la page la montre : découpée, nettoyée, et
// bornée au même plafond.
[[nodiscard]] std::vector<std::string> BoundedHostList(std::string_view listed) {
  std::vector<std::string> hosts = ro::core::SplitHostList(listed);
  if (hosts.size() > ro::core::kMostUserHosts) {
    hosts.resize(ro::core::kMostUserHosts);
  }
  return hosts;
}

}  // namespace

void ChatWebStage::Configure(const ChatWebSetup& setup) {
  // ⚠ CHAMP À CHAMP, ET LES LISTES À PART : la boucle appelle ici à chaque
  // image, et une copie entière recopierait leurs chaînes à chaque fois.
  if (setup.url_preview != setup_.url_preview) {
    ++generation_;
  }
  setup_.url_confirm = setup.url_confirm;
  setup_.url_preview = setup.url_preview;
  setup_.pictures = setup.pictures;
  setup_.picture_pixels = setup.picture_pixels;
  setup_.emote_images = setup.emote_images;
  setup_.item_icons = setup.item_icons;
  if (setup.preview_hosts != preview_hosts_seen_) {
    preview_hosts_seen_ = setup.preview_hosts;
    RebuildAllowList();
  }
  if (setup.open_hosts != open_hosts_seen_) {
    open_hosts_seen_ = setup.open_hosts;
    open_hosts_ = BoundedHostList(open_hosts_seen_);
  }
}

void ChatWebStage::RebuildAllowList() {
  const std::vector<std::string> hosts = BoundedHostList(preview_hosts_seen_);
  allow_ = ro::core::PreviewAllowList(hosts);
  for (const std::string& once : allowed_once_) {
    allow_.AllowOnce(once);
  }
  cache_.SetAllowList(allow_);
  ++generation_;
}

void ChatWebStage::AllowOnce(const std::string& openable) {
  if (std::ranges::find(allowed_once_, openable) == allowed_once_.end()) {
    allowed_once_.push_back(openable);
    if (allowed_once_.size() > ro::core::kMostAllowedOnce) {
      allowed_once_.erase(allowed_once_.begin());
    }
  }
  RebuildAllowList();
}

void ChatWebStage::NoteEmotions(std::span<const ro::ui::ChatEmotion> emotions) {
  emotes_by_action_.assign(ro::ui::kChatEmoteActions, ro::ui::ChatEmotion{});
  for (const ro::ui::ChatEmotion& emotion : emotions) {
    const std::optional<std::size_t> action = ro::render::EmotionAction(emotion.id);
    // ⚠ LA PREMIÈRE QUI DESSINE UNE ACTION LA GARDE : deux numéros du protocole
    // peuvent partager la même image.
    if (action.has_value() && *action < emotes_by_action_.size() &&
        emotes_by_action_[*action].frames.empty()) {
      emotes_by_action_[*action] = emotion;
    }
  }
}

void ChatWebStage::Compose(ro::ui::ChatWindows& chat, std::uint32_t /*now_ms*/) {
  // 🔴 LE CACHE AVANT TOUT LE RESTE : il libère des textures, et rien de cette
  // image ne doit encore en citer une.
  cache_.Poll();
  if (cache_.generation() != facts_cache_generation_ || generation_ != facts_generation_ ||
      facts_.size() > kMostKnownUrls) {
    facts_.clear();
    facts_cache_generation_ = cache_.generation();
    facts_generation_ = generation_;
  }
  seen_pictures_.clear();
  if (!hovered_this_frame_) {
    hovered_.clear();
  }
  hovered_this_frame_ = false;

  ro::ui::ChatRichArt& rich = chat.rich;
  rich.emotes_by_action = emotes_by_action_;
  rich.emote_images = setup_.emote_images;
  rich.item_icons = setup_.item_icons;
  rich.picture_pixels = setup_.pictures ? static_cast<float>(setup_.picture_pixels) : 0.0F;
  rich.seen_pictures = &seen_pictures_;

  // LA VISIONNEUSE : son image en grand, redemandée quand le cache ne la
  // connaît plus — l'éviction a pu la reprendre.
  ro::ui::ChatPictureViewerView& view = chat.viewer_view;
  view = ro::ui::ChatPictureViewerView{};
  if (chat.viewer.open && !chat.viewer.url.empty()) {
    view.state = cache_.State(chat.viewer.url, ChatPictureSize::kLarge);
    if (view.state == ro::ui::ChatPictureState::kUnknown) {
      cache_.Request(chat.viewer.url, ChatPictureSize::kLarge);
      view.state = cache_.State(chat.viewer.url, ChatPictureSize::kLarge);
    }
    if (const ChatPictureTextures* ready = cache_.Ready(chat.viewer.url, ChatPictureSize::kLarge);
        ready != nullptr && !ready->frames.empty()) {
      view.texture = ready->frames.front().texture;
      view.width_px = ready->width_px;
      view.height_px = ready->height_px;
      // ⚠ UNE IMAGE FIXE N'A PAS D'ANIMATION : sa liste reste vide.
      if (ready->frames.size() > 1) {
        view.frames = ready->frames;
      }
    }
  }
}

void ChatWebStage::FillFrom(std::string_view openable, ro::ui::ChatPictureState& state,
                            ImTextureID& texture, float& width_px, float& height_px,
                            std::span<const ro::ui::ChatPictureFrame>& frames) const {
  state = cache_.State(openable, ChatPictureSize::kPreview);
  const ChatPictureTextures* ready = cache_.Ready(openable, ChatPictureSize::kPreview);
  if (ready == nullptr || ready->frames.empty()) {
    return;
  }
  texture = ready->frames.front().texture;
  width_px = ready->width_px;
  height_px = ready->height_px;
  frames = ready->frames.size() > 1 ? std::span<const ro::ui::ChatPictureFrame>(ready->frames)
                                    : std::span<const ro::ui::ChatPictureFrame>();
}

ChatWebStage::KnownUrl& ChatWebStage::Known(std::string_view written) {
  if (const auto known = facts_.find(written); known != facts_.end()) {
    return known->second;
  }
  KnownUrl& known = facts_[std::string(written)];
  known.openable = ro::core::OpenableWebAddress(written);
  const ro::core::WebAddressParts parts = ro::core::ParseWebAddress(known.openable);
  // ⚠ « TOUJOURS AFFICHER » NE S'OFFRE QU'AVEC UN HÔTE : sans hôte admissible —
  // adresse illisible, avertissement, règle de suffixe —, il reste vide.
  if (OffersAlwaysFor(parts, ro::core::WebAddressWarnings(known.openable))) {
    known.host = parts.host;
  }
  ro::ui::ChatUrlFacts& facts = known.facts;
  facts.host = known.host;
  facts.allowed = allow_.Previewable(known.openable);
  // L'infobulle ne dit l'état de l'aperçu qu'à qui l'a demandé : par le
  // réglage, ou par « Afficher cette image ».
  facts.statuses = setup_.url_preview || allow_.AllowedOnce(known.openable);
  FillFrom(known.openable, facts.preview, facts.texture, facts.width_px, facts.height_px,
           facts.frames);
  return known;
}

const ro::ui::ChatUrlFacts* ChatWebStage::FactsOf(std::string_view written) {
  if (written.empty()) {
    return nullptr;
  }
  return &Known(written).facts;
}

void ChatWebStage::FillPicture(std::string_view url, ro::ui::ChatInlineImage& image) {
  // UNE VIGNETTE LIT SON ÉTAT SUR LA MÊME ÉTAGÈRE QUE L'APERÇU : ses faits
  // sont ceux de l'adresse.
  const ro::ui::ChatUrlFacts& facts = Known(url).facts;
  image.state = facts.preview;
  if (facts.preview == ro::ui::ChatPictureState::kReady && facts.texture != 0) {
    image.texture = facts.texture;
    image.width_px = facts.width_px;
    image.height_px = facts.height_px;
    image.frames = facts.frames;
  }
}

void ChatWebStage::RequestSeen(const ChatTabsView& tabs) {
  if (!setup_.pictures) {
    return;
  }
  for (const std::uint32_t key : seen_pictures_) {
    const std::string_view url = tabs.PictureUrlOf(key);
    if (url.empty()) {
      continue;
    }
    // ⚠ SEULE UNE IMAGE QUE LE CACHE NE CONNAÎT PAS PART : une demande en vol ou
    // un échec gardé ne se redemandent pas à chaque image.
    const KnownUrl& known = Known(url);
    if (known.facts.preview == ro::ui::ChatPictureState::kUnknown) {
      cache_.Request(known.openable, ChatPictureSize::kPreview);
    }
  }
}

void ChatWebStage::NoteHovered(std::string_view written, std::uint32_t now_ms) {
  const KnownUrl& known = Known(written);
  if (known.openable != hovered_) {
    hovered_ = known.openable;
    hovered_since_ms_ = now_ms;
  }
  hovered_this_frame_ = true;
  // L'APERÇU AU SURVOL : par le réglage, ou pour une adresse accordée une fois.
  // La liste blanche tranche encore au départ du téléchargement.
  if (!setup_.url_preview && !allow_.AllowedOnce(known.openable)) {
    return;
  }
  if (now_ms - hovered_since_ms_ >= ro::core::kPreviewHoverDelayMs &&
      known.facts.preview == ro::ui::ChatPictureState::kUnknown) {
    cache_.Request(known.openable, ChatPictureSize::kPreview);
  }
}

bool ChatWebStage::OpensWithoutAsking(std::string_view host) const {
  return std::ranges::any_of(open_hosts_, [host](const std::string& rule) {
    return ro::core::HostMatchesRule(host, rule);
  });
}

void ChatWebStage::Open(std::string_view written, ro::ui::ChatWindows& chat) {
  const std::string openable = ro::core::OpenableWebAddress(written);
  if (!OpensInBrowser(openable)) {
    ro::core::LogWarning("chat : adresse refusée — {}", openable);
    return;
  }
  const ro::core::WebAddressParts parts = ro::core::ParseWebAddress(openable);
  const std::uint8_t warnings = ro::core::WebAddressWarnings(openable);
  // 🔴 UNE ADRESSE QUI NE SE LIT PAS EST SUSPECTE : elle passe toujours par la
  // confirmation, réglage ou non, et ne se range jamais parmi les sites sûrs.
  // Voir `OpensWithoutPrompt` pour la règle entière.
  if (OpensWithoutPrompt(parts, warnings, setup_.url_confirm, OpensWithoutAsking(parts.host))) {
    LaunchBrowser(openable);
    return;
  }
  ro::ui::ChatUrlPrompt& prompt = chat.url_prompt;
  prompt.written = std::string(written);
  prompt.openable = openable;
  prompt.host = parts.host;
  prompt.warnings = warnings;
  prompt.unreadable = !parts.valid;
  prompt.offer_always = OffersAlwaysFor(parts, warnings);
  // ⚠ DEMANDÉE À L'IMAGE DU CLIC : la boîte naît sous la souris, là où le
  // joueur vient de cliquer.
  prompt.asked = true;
}

void ChatWebStage::Follow(const ro::ui::ChatLinkAsk& asked, std::string_view written,
                          ro::ui::ChatWindows& chat) {
  if (written.empty()) {
    return;
  }
  const std::string openable = ro::core::OpenableWebAddress(written);
  switch (asked.gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
      Open(written, chat);
      break;
    case ro::ui::ChatLinkGesture::kCopyCode:
      ImGui::SetClipboardText(openable.c_str());
      break;
    case ro::ui::ChatLinkGesture::kShowImageOnce:
      // CETTE ADRESSE-LÀ, POUR CETTE FOIS : rien n'est retenu de son site.
      AllowOnce(openable);
      cache_.Request(openable, ChatPictureSize::kPreview);
      break;
    case ro::ui::ChatLinkGesture::kAllowImageHost: {
      // ⚠ UN ACCÈS PERMANENT AU SURVOL : il passe par sa confirmation, et ne
      // s'offre qu'à un hôte que la liste admet tel quel.
      const ro::core::WebAddressParts parts = ro::core::ParseWebAddress(openable);
      if (OffersAlwaysFor(parts, ro::core::WebAddressWarnings(openable))) {
        chat.host_prompt.host = parts.host;
        chat.host_prompt.asked = true;
      }
      break;
    }
    case ro::ui::ChatLinkGesture::kViewImage:
      // Agrandir est un geste explicite : l'adresse est accordée pour cette fois
      // si son site ne l'est pas déjà, comme « Afficher cette image ».
      if (!allow_.Previewable(openable)) {
        AllowOnce(openable);
      }
      chat.viewer.url = openable;
      chat.viewer.open = true;
      chat.viewer.raise = true;
      chat.viewer.answer = ro::ui::ChatViewerAnswer::kNone;
      cache_.Request(openable, ChatPictureSize::kLarge);
      break;
    case ro::ui::ChatLinkGesture::kNothing:
    case ro::ui::ChatLinkGesture::kRepost:
    case ro::ui::ChatLinkGesture::kMenu:
    case ro::ui::ChatLinkGesture::kOpenDatabase:
    case ro::ui::ChatLinkGesture::kItemInfo:
    case ro::ui::ChatLinkGesture::kWhoDrops:
    case ro::ui::ChatLinkGesture::kMarketSearch:
    case ro::ui::ChatLinkGesture::kTryStyle:
    case ro::ui::ChatLinkGesture::kOpenBestiary:
    case ro::ui::ChatLinkGesture::kOpenInNavigation:
    case ro::ui::ChatLinkGesture::kPlaceContents:
    case ro::ui::ChatLinkGesture::kStaffWarp:
    case ro::ui::ChatLinkGesture::kVoteShop:
    case ro::ui::ChatLinkGesture::kWhisperWindow:
    case ro::ui::ChatLinkGesture::kInviteParty:
    case ro::ui::ChatLinkGesture::kInviteGuild:
    case ro::ui::ChatLinkGesture::kAddFriend:
    case ro::ui::ChatLinkGesture::kMonsterSheet:
    case ro::ui::ChatLinkGesture::kShareRespawn:
    case ro::ui::ChatLinkGesture::kGuideToTomb:
    case ro::ui::ChatLinkGesture::kWhereToFind:
    case ro::ui::ChatLinkGesture::kMobInfo:
    case ro::ui::ChatLinkGesture::kWhereIs:
    case ro::ui::ChatLinkGesture::kAddToMvpLog:
    case ro::ui::ChatLinkGesture::kInviteMvp:
    case ro::ui::ChatLinkGesture::kPostWhereToFind:
    case ro::ui::ChatLinkGesture::kGoThere:
      break;
  }
}

void ChatWebStage::Answer(ro::ui::ChatWindows& chat, const std::filesystem::path& client_root) {
  ro::ui::ChatUrlPrompt& prompt = chat.url_prompt;
  switch (std::exchange(prompt.answer, ro::ui::ChatUrlAnswer::kNone)) {
    case ro::ui::ChatUrlAnswer::kOpen:
      LaunchBrowser(prompt.openable);
      // ⚠ « TOUJOURS » NE SE LIT QU'AVEC « OUVRIR », et seulement là où la
      // boîte l'offrait : la boîte le remet à faux partout ailleurs.
      // ⚠ RIEN N'EST RETENU ICI : le réglage est la seule liste, et `Configure`
      // la relit à l'image suivante.
      if (prompt.always && prompt.offer_always && !prompt.host.empty()) {
        host_to_open_always_ = prompt.host;
      }
      prompt.always = false;
      break;
    case ro::ui::ChatUrlAnswer::kCopy:
      // L'ADRESSE QUI SE SERAIT OUVERTE, et non celle qui est écrite : c'est
      // elle que la boîte montrait.
      ImGui::SetClipboardText(prompt.openable.c_str());
      break;
    case ro::ui::ChatUrlAnswer::kCancel:
    case ro::ui::ChatUrlAnswer::kNone:
      break;
  }

  ro::ui::ChatHostPrompt& host = chat.host_prompt;
  if (std::exchange(host.granted, false) && !host.host.empty()) {
    host_to_allow_ = host.host;
  }

  ro::ui::ChatPictureViewerState& viewer = chat.viewer;
  switch (std::exchange(viewer.answer, ro::ui::ChatViewerAnswer::kNone)) {
    case ro::ui::ChatViewerAnswer::kSave: {
      const std::filesystem::path folder = client_root / kSavedPicturesFolder;
      std::error_code failed;
      std::filesystem::create_directories(folder, failed);
      if (failed) {
        ro::core::LogWarning("chat : le dossier {} n'a pas pu se créer — {}",
                             ro::core::Utf8FromPath(folder), failed.message());
      }
      cache_.RequestSave(viewer.url, folder);
      break;
    }
    case ro::ui::ChatViewerAnswer::kOpenInBrowser:
      Open(viewer.url, chat);
      break;
    case ro::ui::ChatViewerAnswer::kCopyAddress:
      ImGui::SetClipboardText(viewer.url.c_str());
      break;
    case ro::ui::ChatViewerAnswer::kNone:
      break;
  }
}

std::optional<std::string> ChatWebStage::TakeHostToAllow() {
  return std::exchange(host_to_allow_, std::nullopt);
}

std::optional<std::string> ChatWebStage::TakeHostToOpenAlways() {
  return std::exchange(host_to_open_always_, std::nullopt);
}

std::optional<ChatSaveOutcome> ChatWebStage::TakeSaved() { return cache_.TakeSaved(); }

}  // namespace moonclient
