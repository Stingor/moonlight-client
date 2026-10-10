#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/web_address.h"
#include "ro/core/web_preview_policy.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LES LIENS, LES IMAGES ET LES MENTIONS DU CHAT, dans la section Conversation
//
// Ce que le fil fait d'une adresse — confirmer avant d'ouvrir, en montrer
// l'aperçu au survol —, ce qu'il peint en image, les sites que le joueur a
// admis, et la façon dont une ligne qui le nomme se signale.
//
// 🔴 L'APERÇU AU SURVOL ET LES VIGNETTES SONT ÉTEINTS PAR DÉFAUT : une image
// téléchargée sans qu'on l'ait demandé dit au site d'en face qu'on est en
// ligne, et depuis où.
struct ChatWebSetup {
  bool url_confirm = true;
  bool url_preview = false;
  bool pictures = false;
  // La hauteur des vignettes du fil, en pixels, entre `kFewestChatPicturePixels`
  // et `kMostChatPicturePixels`.
  int picture_pixels = 48;
  // Les émoticones du jeu `:nom:` peintes à hauteur de ligne.
  bool emote_images = true;
  // L'icône d'un objet lié, devant son nom.
  bool item_icons = true;
  // Les sites admis par le joueur, « a.com;b.net ».
  std::string preview_hosts;
  std::string open_hosts;
  bool mentions = true;
  // Les mots-clés qui signalent une ligne, « a;b ».
  std::string mention_words;
  bool mention_sound = false;
  bool mention_flash = true;
};

// Les bornes du curseur de la taille des vignettes.
inline constexpr int kFewestChatPicturePixels = 24;
inline constexpr int kMostChatPicturePixels = 128;

inline constexpr int kChatWebRowBase = 8400;
// Une rangée « Retirer » par site admis, de chaque liste.
inline constexpr int kChatPreviewHostRowBase = 8500;
inline constexpr int kChatOpenHostRowBase = 8600;

enum class ChatWebRow : int {
  kUrlConfirm = 0,
  kUrlPreview,
  kPictures,
  kPictureSize,
  kEmoteImages,
  kItemIcons,
  kMentions,
  kMentionWords,
  kMentionSound,
  kMentionFlash,
  // Les intitulés des deux listes de sites admis, posés quand elles ne sont pas
  // vides.
  kPreviewHostsTitle,
  kOpenHostsTitle,
  kCount,
};

// UN SITE QUE LE FIL PEUT ADMETTRE D'UN GESTE : un nom EXACT d'au moins deux
// étiquettes — ni point initial ou final, ni « .. ». Une règle de suffixe
// (« .x.fr ») ne s'écrit qu'à la main, dans le fichier.
[[nodiscard]] bool IsAdmissibleChatHost(std::string_view host);

// « Toujours ouvrir / Toujours afficher » : une adresse lisible, en https,
// sans aucun avertissement, dont le site est admissible.
[[nodiscard]] bool OffersAlwaysFor(const ro::core::WebAddressParts& parts, std::uint8_t warnings);

// UN CLIC OUVRE-T-IL LE NAVIGATEUR SANS LA CONFIRMATION ? Oui pour un site que
// le joueur a dit d'ouvrir (`listed_host`) quand l'adresse permet « Toujours » ;
// oui, confirmation décochée, pour une adresse lisible dont le seul
// avertissement est le http en clair. Toute autre passe par la boîte.
[[nodiscard]] bool OpensWithoutPrompt(const ro::core::WebAddressParts& parts, std::uint8_t warnings,
                                      bool url_confirm, bool listed_host);

static_assert(kChatWebRowBase + static_cast<int>(ChatWebRow::kCount) <= kChatPreviewHostRowBase,
              "la plage des liens du chat chevauche celle de ses sites admis");
static_assert(kChatPreviewHostRowBase + static_cast<int>(ro::core::kMostUserHosts) <=
                  kChatOpenHostRowBase,
              "les sites admis à l'aperçu chevauchent ceux ouverts sans confirmation");

class ChatWebOptions {
 public:
  // ⚠ BORNÉ À CHAQUE LECTURE, SUR PLACE : le fichier se modifie à la main — et
  // se relit d'un profil à l'autre —, et une taille hors du curseur ne doit pas
  // atteindre le fil. Rien n'est recopié : la boucle le lit à chaque image.
  [[nodiscard]] const ChatWebSetup& setup();

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Ajoute ses lignes à celles d'une section.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows);

  // Rend vrai quand la ligne était la sienne.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // Un site admis depuis le fil : l'aperçu, ou l'ouverture sans confirmation.
  // Rend vrai quand la liste a changé.
  // ⚠ SANS EFFET sur un hôte qui n'est pas admissible (`IsAdmissibleChatHost`)
  // ou déjà admis, et au-delà de `kMostUserHosts` sites.
  [[nodiscard]] bool AddPreviewHost(std::string_view host);
  [[nodiscard]] bool AddOpenHost(std::string_view host);

 private:
  ChatWebSetup setup_;
  // LES SITES TELS QUE LA PAGE LES A MONTRÉS : une rangée « Retirer » porte un
  // RANG dans cette liste, et désigne le site qu'on a vu, même si le fil en a
  // admis un autre depuis. ⚠ LES RANGÉES POINTENT DANS CES CHAÎNES.
  std::vector<std::string> shown_preview_hosts_;
  std::vector<std::string> shown_open_hosts_;
};

}  // namespace moonclient
