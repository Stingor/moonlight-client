#include "chat/chat_tab_file.h"

#include <cstddef>
#include <string>
#include <system_error>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// Les champs d'un onglet, en anglais (ADR 0093). ⚠ GRAVÉS : les changer rend à
// tous les joueurs les onglets d'origine, sans que rien ne le dise.
constexpr const char* kTabsField = "tabs";
constexpr const char* kNameField = "name";
constexpr const char* kFilterField = "filter";
constexpr const char* kGroupField = "window";
constexpr const char* kLockedField = "locked";
constexpr const char* kContentField = "content";
constexpr const char* kMeterContent = "damage_meter";
// L'apparence propre à l'onglet. ⚠ TOUT SOUS `look` : le drapeau qui la
// commande et les valeurs qu'il commande se lisent ensemble, et une valeur
// réglée à la main sans le drapeau ne ferait rien du tout.
constexpr const char* kLookField = "look";
constexpr const char* kLookOwnField = "own";
constexpr const char* kLookFontField = "font";
constexpr const char* kLookPaddingField = "padding";
constexpr const char* kLookGapField = "line_gap";
constexpr const char* kLookBodyField = "body";

constexpr const char* kFileHeader =
    "# Les onglets de la conversation.\n"
    "#\n"
    "# Un onglet porte un nom (« name ») et ses filtres (« filter ») : vingt-huit\n"
    "# cases, vrai ou faux, dans l'ordre des types du client d'origine — public,\n"
    "# chat public, message privé, chat de groupe, chat de guilde, chat\n"
    "# d'alliance, objets ramassés/lâchés, équipement mis/retiré, altérations\n"
    "# d'état, objets obtenus par le groupe, altérations d'état du groupe, échecs\n"
    "# de compétence, configuration du groupe, équipement endommagé,\n"
    "# informations WOE, recherche de groupe, combat, combat du groupe,\n"
    "# expérience, quête, champ de bataille, chat du clan, messages d'appel, EXP\n"
    "# de contribution, attributs d'équipement —, une vingt-sixième pour les\n"
    "# annonces du serveur, une vingt-septième pour le relai Discord et une\n"
    "# vingt-huitième pour l'armement et le lancer de ses propres skills. Une case\n"
    "# absente est décochée, sauf celle du relai Discord, qui suit alors le chat\n"
    "# public.\n"
    "#\n"
    "# Une liste vide laisse les onglets d'origine : une conversation sans onglet\n"
    "# n'aurait nulle part où s'écrire.\n"
    "#\n"
    "# « window » dit où l'onglet est montré : absente ou zéro, c'est la fenêtre\n"
    "# principale ; un même numéro réunit plusieurs onglets dans une même\n"
    "# flottante. Les numéros ne se réemploient pas : ils nomment la fenêtre, donc\n"
    "# ce qui lui garde sa position et sa taille.\n"
    "#\n"
    "# « look » ne paraît que si l'onglet a une apparence à lui ; sans elle, il\n"
    "# suit les réglages généraux de la conversation, et leurs changements à\n"
    "# venir. « body » est une couleur en ABGR, transparence comprise.\n"
    "#\n"
    "# « content: damage_meter » marque l'onglet où le compteur de dégâts est\n"
    "# rangé ; il n'a pas de filtres. Absent, l'onglet montre le fil.\n";

// Les cases lues d'une liste. Au-delà de la dernière case connue, ce qu'une
// version plus récente aurait ajouté est ignoré ; en deçà, ce qui manque est
// décoché.
//
// ⚠ SAUF LE RELAI DISCORD, VENU APRÈS LES AUTRES : une liste qui s'arrête
// avant sa case a été écrite sans lui, et il y prend la valeur du chat public,
// dont il est la suite. Sans cela, l'onglet « Tout » d'un fichier d'avant
// cacherait le relai sans que le joueur l'ait jamais décoché.
//
// ⚠ « MES SKILLS (VISÉE ET LANCER) », VENUE APRÈS LUI, RESTE DÉCOCHÉE quand
// elle manque : c'est aussi son état d'un onglet neuf — voir
// `kDefaultChatTypes`.
[[nodiscard]] ChatTypeMask FilterOf(const YAML::Node& listed) {
  ChatTypeMask accepted = 0;
  if (!listed || !listed.IsSequence()) {
    return accepted;
  }
  std::size_t rank = 0;
  for (const YAML::Node& box : listed) {
    if (rank >= ro::sim::kChatTypeCount) {
      break;
    }
    if (box.IsScalar() && box.as<bool>(false)) {
      accepted |= ChatTypeMask{1} << rank;
    }
    ++rank;
  }
  if (rank <= static_cast<std::size_t>(ro::sim::ChatType::kDiscord) &&
      (accepted & MaskOf(ro::sim::ChatType::kPublicChat)) != 0) {
    accepted |= MaskOf(ro::sim::ChatType::kDiscord);
  }
  return accepted;
}

}  // namespace

std::filesystem::path ChatTabFilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kChatTabFileName;
}

void LoadChatTabs(const std::filesystem::path& client_root, ChatTabs& into) {
  const std::filesystem::path path = ChatTabFilePath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return;  // première partie : ceux d'origine font l'affaire
  }
  YAML::Node whole;
  try {
    whole = YAML::LoadFile(path.string());
  } catch (const YAML::Exception& broken) {
    // La seule exception qu'on attrape, et pour la même raison qu'ailleurs : un
    // fichier abîmé ne doit pas empêcher de jouer.
    ro::core::LogWarning(
        "onglets de conversation : fichier illisible ({}) — les onglets d'origine sont rendus",
        broken.what());
    return;
  }
  // Une racine qui n'est pas une table n'a pas de champs : voir
  // `LoadChatHistory`.
  if (!whole.IsMap()) {
    return;
  }
  // ⚠ UN FICHIER SANS `tabs` LAISSE CEUX D'ORIGINE — c'est aussi le cas d'un
  // fichier aux clés françaises, dont les filtres désignaient d'autres cases :
  // les relire rangerait chaque message ailleurs que là où le joueur l'attend.
  const YAML::Node listed = whole[kTabsField];
  if (!listed || !listed.IsSequence()) {
    return;
  }

  std::vector<ChatTab> read;
  for (const YAML::Node& entry : listed) {
    if (!entry.IsMap()) {
      continue;
    }
    const YAML::Node named = entry[kNameField];
    if (!named || !named.IsScalar()) {
      continue;  // un onglet sans nom n'a pas de languette à cliquer
    }
    ChatTab tab;
    tab.name = named.as<std::string>(std::string());
    if (tab.name.empty()) {
      continue;
    }
    tab.accepted = FilterOf(entry[kFilterField]);
    // La fenêtre qui le montre. Absente, c'est la principale.
    tab.group = entry[kGroupField].as<std::uint32_t>(0U);
    tab.locked = entry[kLockedField].as<bool>(false);
    if (entry[kContentField].as<std::string>(std::string()) == kMeterContent) {
      tab.content = ChatTabContent::kDamageMeter;
      tab.accepted = 0;
    }
    if (const YAML::Node look = entry[kLookField]; look && look.IsMap()) {
      tab.look.own = look[kLookOwnField].as<bool>(false);
      tab.look.font_percent = look[kLookFontField].as<int>(0);
      tab.look.padding_pixels = look[kLookPaddingField].as<int>(0);
      tab.look.line_gap_pixels = look[kLookGapField].as<int>(0);
      tab.look.body = look[kLookBodyField].as<std::uint32_t>(0U);
    }
    read.push_back(std::move(tab));
  }
  into.Replace(std::move(read));
}

bool SaveChatTabs(const std::filesystem::path& client_root, const ChatTabs& from) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kTabsField << YAML::Value << YAML::BeginSeq;
  for (const ChatTab& tab : from.tabs()) {
    out << YAML::BeginMap;
    out << YAML::Key << kNameField << YAML::Value << tab.name;
    // ⚠ LE COMPTEUR N'A PAS DE FILTRES : il ne montre aucune ligne, et des
    // cases décochées sur lui feraient croire qu'il en filtre.
    if (tab.content == ChatTabContent::kDamageMeter) {
      out << YAML::Key << kContentField << YAML::Value << kMeterContent;
    } else {
      // ⚠ TOUTES LES CASES, TOUJOURS, dans l'ordre des numéros : une liste
      // qui sauterait les cases décochées ne dirait plus à quel type chacune
      // répond.
      out << YAML::Key << kFilterField << YAML::Value << YAML::Flow << YAML::BeginSeq;
      for (std::size_t rank = 0; rank < ro::sim::kChatTypeCount; ++rank) {
        out << ((tab.accepted & (ChatTypeMask{1} << rank)) != 0);
      }
      out << YAML::EndSeq;
    }
    // ⚠ ÉCRITE SEULEMENT QUAND ELLE N'EST PAS LA PRINCIPALE : un « window: 0 »
    // sur chaque onglet dirait ce que son absence dit déjà, dans un fichier que
    // le joueur peut ouvrir.
    if (tab.group != 0) {
      out << YAML::Key << kGroupField << YAML::Value << tab.group;
    }
    if (tab.locked) {
      out << YAML::Key << kLockedField << YAML::Value << true;
    }
    // ⚠ ÉCRITE SEULEMENT QUAND L'ONGLET EN A UNE : un onglet qui suit les
    // réglages généraux n'a rien à dire, et écrire quatre valeurs mortes sous
    // un drapeau faux ferait croire qu'elles agissent.
    if (tab.look.own) {
      out << YAML::Key << kLookField << YAML::Value << YAML::BeginMap;
      out << YAML::Key << kLookOwnField << YAML::Value << true;
      out << YAML::Key << kLookFontField << YAML::Value << tab.look.font_percent;
      out << YAML::Key << kLookPaddingField << YAML::Value << tab.look.padding_pixels;
      out << YAML::Key << kLookGapField << YAML::Value << tab.look.line_gap_pixels;
      out << YAML::Key << kLookBodyField << YAML::Value << tab.look.body;
      out << YAML::EndMap;
    }
    out << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;

  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(ChatTabFilePath(client_root), text);
      !written) {
    // ⚠ CE N'EST PAS LE VIDE QUI SURVIT, mais le fichier PRÉCÉDENT : l'écriture
    // est atomique, donc elle n'a rien remplacé.
    ro::core::LogWarning(
        "onglets de conversation : écriture impossible ({}) — les onglets de cette partie ne "
        "seront pas retenus, ceux d'avant reviendront",
        ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
