#include "options/chat_web_options.h"

#include <algorithm>
#include <cmath>
#include <span>

#include "ro/core/i18n.h"
#include "ro/core/web_address.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kUrlConfirmKey = "url_confirm_chat";
constexpr const char* kUrlPreviewKey = "url_preview_chat";
constexpr const char* kPicturesKey = "pictures_chat";
constexpr const char* kPictureSizeKey = "picture_size_chat";
constexpr const char* kEmoteImagesKey = "emote_images_chat";
constexpr const char* kItemIconsKey = "item_icons_chat";
constexpr const char* kPreviewHostsKey = "preview_hosts_chat";
constexpr const char* kOpenHostsKey = "open_hosts_chat";
constexpr const char* kMentionsKey = "mentions_chat";
constexpr const char* kMentionWordsKey = "mention_words_chat";
constexpr const char* kMentionSoundKey = "mention_sound_chat";
constexpr const char* kMentionFlashKey = "mention_flash_chat";

// Le séparateur d'une liste de sites, refusé dans UN site : « a.com;b.net »
// admis d'un geste en ferait deux.
constexpr char kHostSeparator = ';';

[[nodiscard]] int IdOf(ChatWebRow row) { return kChatWebRowBase + static_cast<int>(row); }

[[nodiscard]] int BoundedPicturePixels(int pixels) {
  return std::clamp(pixels, kFewestChatPicturePixels, kMostChatPicturePixels);
}

[[nodiscard]] OptionRowView Toggle(ChatWebRow row, const char* label, const char* help, bool on) {
  return OptionRowView{
      .id = IdOf(row), .label = label, .help = help, .kind = OptionKind::kToggle, .on = on};
}

// UNE LISTE DE SITES ADMIS : son intitulé, puis un « Retirer » par site.
//
// ⚠ MONTRÉE MÊME QUAND LE RÉGLAGE QU'ELLE SERT EST ÉTEINT : on doit pouvoir
// reprendre un accord après avoir éteint l'aperçu, sinon l'éteindre enfermerait
// le joueur avec une liste qu'il ne peut plus défaire. Vide, elle se tait —
// discrète pour qui n'a jamais rien accordé.
void ComposeHostList(std::vector<OptionRowView>& rows, ChatWebRow title, const char* title_text,
                     int first_row_id, std::string_view listed, std::vector<std::string>& shown) {
  shown = ro::core::SplitHostList(listed);
  if (shown.size() > ro::core::kMostUserHosts) {
    shown.resize(ro::core::kMostUserHosts);
  }
  if (shown.empty()) {
    return;
  }
  rows.push_back(OptionRowView{
      .id = IdOf(title), .label = title_text, .kind = OptionKind::kNote, .indented = true});
  // ⚠ LA LISTE EST COMPLÈTE AVANT LA PREMIÈRE VUE : une rangée pointe dans ses
  // chaînes.
  for (std::size_t rank = 0; rank < shown.size(); ++rank) {
    rows.push_back(OptionRowView{.id = first_row_id + static_cast<int>(rank),
                                 .label = Tr("Retirer"),
                                 .kind = OptionKind::kAction,
                                 .object = shown[rank].c_str(),
                                 .small_button = true,
                                 .indented = true});
  }
}

// Ajoute un site à une liste « a.com;b.net », nettoyé comme la liste elle-même.
// Rend vrai quand la liste a changé.
[[nodiscard]] bool AddHostTo(std::string& listed, std::string_view host) {
  if (host.find(kHostSeparator) != std::string_view::npos) {
    return false;
  }
  const std::vector<std::string> added = ro::core::SplitHostList(host);
  if (added.empty() || !IsAdmissibleChatHost(added.front())) {
    return false;
  }
  std::vector<std::string> hosts = ro::core::SplitHostList(listed);
  if (hosts.size() >= ro::core::kMostUserHosts ||
      std::ranges::find(hosts, added.front()) != hosts.end()) {
    return false;
  }
  hosts.push_back(added.front());
  listed = ro::core::JoinHostList(hosts);
  return true;
}

// Retire de la liste le site montré à ce rang. Rend vrai quand la rangée était
// l'une des siennes, qu'il y ait eu un site à retirer ou non.
[[nodiscard]] bool RemoveShownHost(const ro::ui::OptionsWindowAction& asked, int first_row_id,
                                   std::span<const std::string> shown, std::string& listed) {
  if (asked.changed < first_row_id ||
      asked.changed >= first_row_id + static_cast<int>(ro::core::kMostUserHosts)) {
    return false;
  }
  const auto rank = static_cast<std::size_t>(asked.changed - first_row_id);
  if (rank < shown.size()) {
    std::vector<std::string> hosts = ro::core::SplitHostList(listed);
    std::erase(hosts, shown[rank]);
    listed = ro::core::JoinHostList(hosts);
  }
  return true;
}

}  // namespace

const ChatWebSetup& ChatWebOptions::setup() {
  setup_.picture_pixels = BoundedPicturePixels(setup_.picture_pixels);
  return setup_;
}

void ChatWebOptions::Declare(SettingsFile& into) {
  into.Declare(kUrlConfirmKey, &setup_.url_confirm);
  into.Declare(kUrlPreviewKey, &setup_.url_preview);
  into.Declare(kPicturesKey, &setup_.pictures);
  into.Declare(kPictureSizeKey, &setup_.picture_pixels);
  into.Declare(kEmoteImagesKey, &setup_.emote_images);
  into.Declare(kItemIconsKey, &setup_.item_icons);
  into.Declare(kPreviewHostsKey, &setup_.preview_hosts);
  into.Declare(kOpenHostsKey, &setup_.open_hosts);
  into.Declare(kMentionsKey, &setup_.mentions);
  into.Declare(kMentionWordsKey, &setup_.mention_words);
  into.Declare(kMentionSoundKey, &setup_.mention_sound);
  into.Declare(kMentionFlashKey, &setup_.mention_flash);
}

void ChatWebOptions::ComposeInto(std::vector<OptionRowView>& rows) {
  rows.push_back(Toggle(
      ChatWebRow::kUrlConfirm, Tr("Avertir avant d'ouvrir un lien"),
      Tr("Une adresse postée dans le chat vient d'un autre joueur, et le texte affiché n'a aucun "
         "rapport obligé avec la destination réelle. La confirmation montre l'adresse COMPLÈTE "
         "avant d'ouvrir le navigateur.\n\nEn la décochant, un clic sur un lien ouvre "
         "directement le navigateur."),
      setup_.url_confirm));
  rows.back().heading = Tr("Liens et images");
  ComposeHostList(rows, ChatWebRow::kOpenHostsTitle, Tr("Sites ouverts sans confirmation :"),
                  kChatOpenHostRowBase, setup_.open_hosts, shown_open_hosts_);

  rows.push_back(Toggle(
      ChatWebRow::kUrlPreview, Tr("Aperçu des images au survol"),
      Tr("Survoler un lien d'image en montre le contenu, sans ouvrir le navigateur.\n\nLes "
         "images ne sont chargées que depuis des hébergeurs connus (Discord, imgur, le site "
         "Moonlight), depuis vos sites autorisés, et pour les adresses que vous avez choisi "
         "d'afficher une fois. Sur les hébergeurs connus, celui qui a posté le lien ne peut pas "
         "savoir qui l'a regardé. Un lien vers n'importe quel autre site reste un lien "
         "ordinaire, à ouvrir soi-même."),
      setup_.url_preview));
  ComposeHostList(rows, ChatWebRow::kPreviewHostsTitle, Tr("Vos sites autorisés :"),
                  kChatPreviewHostRowBase, setup_.preview_hosts, shown_preview_hosts_);

  // LA CASE DIT S'IL Y EN A, LE CURSEUR DIT LAQUELLE : un seul curseur où zéro
  // vaudrait « aucune » répondrait à deux questions à la fois.
  rows.push_back(Toggle(ChatWebRow::kPictures, Tr("Images et emotes"),
                        Tr("Montre en vignettes, dans le fil, les images que le relai Discord "
                           "dépose sur le site du serveur, et les emotes de Discord.\n\nToute "
                           "autre adresse d'image reste un lien, dont l'aperçu se règle à part. "
                           "Décoché, rien n'est téléchargé. Les emotes du jeu ont leur propre "
                           "case."),
                        setup_.pictures));
  // ⚠ GRISÉ ET NON RETIRÉ : on voit la taille qui s'appliquera.
  rows.push_back(OptionRowView{
      .id = IdOf(ChatWebRow::kPictureSize),
      .label = Tr("Taille des images"),
      .help = Tr("Hauteur des vignettes, de 24 à 128 pixels.\n\nAu-delà d'une hauteur de ligne, "
                 "la ligne s'agrandit pour accueillir l'image : le fil reste lisible, il "
                 "s'aère."),
      .kind = OptionKind::kSlider,
      .value = static_cast<float>(BoundedPicturePixels(setup_.picture_pixels)),
      .lowest = static_cast<float>(kFewestChatPicturePixels),
      .highest = static_cast<float>(kMostChatPicturePixels),
      .format = "%.0f px",
      .indented = true,
      .disabled = !setup_.pictures});
  rows.push_back(Toggle(ChatWebRow::kEmoteImages, Tr("Emotes du jeu en image"),
                        Tr("Une emote du jeu écrite « :nom: » — celles de la palette, que "
                           "Discord montre aussi — se peint à la hauteur de la ligne, depuis les "
                           "sprites du jeu : rien n'est téléchargé. Une seule par ligne : les "
                           "suivantes restent écrites.\n\nDécoché, elle reste son « :nom: »."),
                        setup_.emote_images));
  rows.push_back(Toggle(ChatWebRow::kItemIcons, Tr("Icônes d'items"),
                        Tr("L'icône d'un item lié, devant son nom, à la hauteur de la ligne."),
                        setup_.item_icons));

  rows.push_back(Toggle(
      ChatWebRow::kMentions, Tr("Signaler les mentions"),
      Tr("Une ligne qui vous nomme — le nom de votre personnage, ou l'un de vos mots-clés — se "
         "détache du fil sur un bandeau. Le mot se cherche entier, majuscules ou non ; une "
         "lettre accentuée se compare telle quelle.\n\nSeule la parole compte : public, "
         "chuchotement reçu, groupe, guilde, canaux, clan, champ de bataille et annonces. Les "
         "messages du serveur, du combat et du butin ne signalent jamais rien."),
      setup_.mentions));
  rows.back().heading = Tr("Mentions");
  const bool silent = !setup_.mentions;
  rows.push_back(OptionRowView{
      .id = IdOf(ChatWebRow::kMentionWords),
      .label = Tr("Mots-clés"),
      .help = Tr("D'autres mots qui vous signalent une ligne, séparés par « ; » : "
                 "« Ana;mvp;Baphomet ». Deux caractères au moins ; chacun se cherche en mot "
                 "entier."),
      .kind = OptionKind::kText,
      .text = setup_.mention_words.c_str(),
      .indented = true,
      .disabled = silent});
  OptionRowView sound =
      Toggle(ChatWebRow::kMentionSound, Tr("Son à la mention"),
             Tr("Le son d'un chuchotement, joué à l'arrivée d'une ligne qui vous signale. Deux "
                "secondes au moins entre deux sons."),
             setup_.mention_sound);
  sound.indented = true;
  sound.disabled = silent;
  rows.push_back(sound);
  OptionRowView flash = Toggle(
      ChatWebRow::kMentionFlash, Tr("Faire clignoter la barre des tâches"),
      Tr("Seulement quand la fenêtre du jeu n'a pas le focus : son bouton clignote jusqu'à ce "
         "que vous y reveniez."),
      setup_.mention_flash);
  flash.indented = true;
  flash.disabled = silent;
  rows.push_back(flash);
}

bool ChatWebOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (RemoveShownHost(asked, kChatPreviewHostRowBase, shown_preview_hosts_, setup_.preview_hosts) ||
      RemoveShownHost(asked, kChatOpenHostRowBase, shown_open_hosts_, setup_.open_hosts)) {
    return true;
  }
  if (asked.changed < kChatWebRowBase ||
      asked.changed >= kChatWebRowBase + static_cast<int>(ChatWebRow::kCount)) {
    return false;
  }
  switch (static_cast<ChatWebRow>(asked.changed - kChatWebRowBase)) {
    case ChatWebRow::kUrlConfirm:
      setup_.url_confirm = asked.on;
      break;
    case ChatWebRow::kUrlPreview:
      setup_.url_preview = asked.on;
      break;
    case ChatWebRow::kPictures:
      setup_.pictures = asked.on;
      break;
    case ChatWebRow::kPictureSize:
      setup_.picture_pixels = BoundedPicturePixels(static_cast<int>(std::lround(asked.value)));
      break;
    case ChatWebRow::kEmoteImages:
      setup_.emote_images = asked.on;
      break;
    case ChatWebRow::kItemIcons:
      setup_.item_icons = asked.on;
      break;
    case ChatWebRow::kMentions:
      setup_.mentions = asked.on;
      break;
    case ChatWebRow::kMentionWords:
      setup_.mention_words = asked.text;
      break;
    case ChatWebRow::kMentionSound:
      setup_.mention_sound = asked.on;
      break;
    case ChatWebRow::kMentionFlash:
      setup_.mention_flash = asked.on;
      break;
    case ChatWebRow::kPreviewHostsTitle:
    case ChatWebRow::kOpenHostsTitle:
    case ChatWebRow::kCount:
      break;
  }
  return true;
}

bool ChatWebOptions::AddPreviewHost(std::string_view host) {
  return AddHostTo(setup_.preview_hosts, host);
}

bool ChatWebOptions::AddOpenHost(std::string_view host) {
  return AddHostTo(setup_.open_hosts, host);
}

bool IsAdmissibleChatHost(std::string_view host) {
  // ⚠ LE POINT INITIAL SEUL SE TESTE ICI : `IsUsableHostRule` refuse déjà
  // l'étiquette vide — « .. », point final — et le nom d'une seule étiquette.
  return !host.empty() && !host.starts_with('.') && ro::core::IsUsableHostRule(host);
}

bool OffersAlwaysFor(const ro::core::WebAddressParts& parts, std::uint8_t warnings) {
  return parts.valid && parts.scheme == "https" && warnings == 0 &&
         IsAdmissibleChatHost(parts.host);
}

bool OpensWithoutPrompt(const ro::core::WebAddressParts& parts, std::uint8_t warnings,
                        bool url_confirm, bool listed_host) {
  if (listed_host && OffersAlwaysFor(parts, warnings)) {
    return true;
  }
  // 🔴 SEUL LE HTTP EN CLAIR PASSE SANS LA BOÎTE : « @ », un nom en punycode ou
  // hors ASCII trompent l'œil sur le site, et c'est la boîte qui le dit.
  const auto tolerated = static_cast<std::uint8_t>(ro::core::kWebWarnPlainHttp);
  return !url_confirm && parts.valid && (warnings & static_cast<std::uint8_t>(~tolerated)) == 0;
}

}  // namespace moonclient
