#include "options/card_album_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kNameIconKey = "album_icon_of_name";
constexpr const char* kSourcesKey = "album_badges";
constexpr const char* kDropperRimKey = "album_trim_nature";
constexpr const char* kAutoSacrificeKey = "album_sacrifice_without_confirmation";

[[nodiscard]] OptionRowView Toggle(CardAlbumRow row, const char* label, const char* help, bool on) {
  return OptionRowView{.id = CardAlbumRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kToggle,
                       .on = on};
}

}  // namespace

void CardAlbumOptions::Declare(SettingsFile& into) {
  into.Declare(kNameIconKey, &look_.name_icon);
  into.Declare(kSourcesKey, &look_.sources);
  into.Declare(kDropperRimKey, &look_.dropper_rim);
  into.Declare(kAutoSacrificeKey, &look_.auto_sacrifice);
}

void CardAlbumOptions::Compose() {
  rows_.clear();
  // Rien ne l'ouvre depuis le jeu : ni fenêtre du client, ni icône de menu. Ce
  // bouton et la touche à lier sont donc les DEUX seules portes, et elles
  // doivent être ici.
  rows_.push_back(OptionRowView{
      .id = CardAlbumRowId(CardAlbumRow::kOpen),
      .label = Tr("Ouvrir l'album"),
      .help = Tr("La touche se règle dans la configuration des raccourcis, sous « Album de "
                 "cartes »."),
      .kind = OptionKind::kAction,
      .object = open_key_.empty() ? Tr("Aucune touche") : open_key_.c_str()});
  rows_.push_back(Toggle(CardAlbumRow::kNameIcon, Tr("Icône de la carte devant son nom"),
                         Tr("Ajoute la petite icône d'inventory de la carte devant son nom, "
                            "sous chaque pochette. Le nom se réduit d'autant : sur des pochettes "
                            "étroites, gagner l'icône coûte quelques lettres."),
                         look_.name_icon));
  rows_.push_back(Toggle(CardAlbumRow::kSources, Tr("Macarons « où la trouver »"),
                         Tr("Pose « O » et « M » au coin d'une pochette quand la carte figure "
                            "dans l'Old Card Album ou le Mystical Card Album. Le survol donne sa "
                            "chance exacte, le clic ouvre la description de l'album — et sa table "
                            "de tirage complète. La donnée est celle du client, la même que "
                            "l'onglet « Probabilités » d'une description."),
                         look_.sources));
  rows_.push_back(Toggle(CardAlbumRow::kDropperRim, Tr("Liseré selon la nature du monstre"),
                         Tr("Colore le bord de la pochette d'après le monstre qui lâche la carte : "
                            "BLEU pour un mini-boss, ORANGE pour un MVP, noir pour tous les "
                            "autres. Quand plusieurs monstres lâchent la même carte, c'est le "
                            "plus coriace qui donne la couleur. La donnée vient du serveur, avec "
                            "le catalogue."),
                         look_.dropper_rim));
  rows_.push_back(Toggle(CardAlbumRow::kAutoSacrifice, Tr("Sacrifier sans confirmation"),
                         Tr("La première copie d'une carte est sacrifiée dès qu'elle est offerte "
                            "à l'album (glisser, menu « Vers l'album »), sans la question "
                            "« Sacrifier cette carte ? ». Le sacrifice reste irréversible : la "
                            "pochette débloquée et le message le disent après coup."),
                         look_.auto_sacrifice));
}

bool CardAlbumOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kCardAlbumRowBase ||
      asked.changed >= kCardAlbumRowBase + static_cast<int>(CardAlbumRow::kCount)) {
    return false;
  }
  switch (static_cast<CardAlbumRow>(asked.changed - kCardAlbumRowBase)) {
    case CardAlbumRow::kNameIcon:
      look_.name_icon = asked.on;
      break;
    case CardAlbumRow::kSources:
      look_.sources = asked.on;
      break;
    case CardAlbumRow::kDropperRim:
      look_.dropper_rim = asked.on;
      break;
    case CardAlbumRow::kAutoSacrifice:
      look_.auto_sacrifice = asked.on;
      break;
    case CardAlbumRow::kOpen:
    case CardAlbumRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
