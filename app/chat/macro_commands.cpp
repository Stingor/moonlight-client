// LE CATALOGUE DES COMMANDES QU'UNE MACRO PEUT PORTER.
//
// ⚠ CE FICHIER NE SE LIT PAS DE BOUT EN BOUT : on y cherche UNE commande, on
// corrige sa description, on s'en va. C'est un catalogue, et les descriptions
// viennent de la configuration du SERVEUR — voir l'en-tête.
//
// ⚠ CHAQUE DESCRIPTION EST UN LITTÉRAL À SA PLACE D'APPEL, et non une table
// qu'on parcourrait : le contrôle des traductions ne relève que les littéraux,
// et une table traduite par `Tr(variable)` n'entrerait dans aucun catalogue. La
// répétition est le prix, et elle est visible ; l'absence de traduction ne
// l'aurait pas été.

#include "chat/macro_commands.h"

#include <array>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"

#include "chat/chat_view.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// LES ÉMOTICÔNES DONT LE CLIENT REMPLIT SES DIX LIGNES, relevées dans SA table
// de messages (`data\msgstringtable.csv` de `moonlight.grf`) :
//
//     0x220 /!    0x221 /?    0x222 /ho   0x223 /lv   0x224 /lv2  ← SAUTÉ
//     0x225 /swt  0x226 /ic   0x227 /an   0x228 /ag   0x229 /$    0x22A /...
//
// 🔴 LE RANG SAUTÉ N'EST PAS LE CINQUIÈME SPRITE, C'EST `/lv2`, qui est
// l'émoticône 14 chez nous. Déduire « dix rangs consécutifs moins le
// quatrième » donnait `/gawi` à la place de `/swt` — une commande qui n'est pas
// un défaut du client, et un défaut du client manquant. Les deux phrases de
// l'implémentation de référence se contredisent sur ce point ; la table du
// client tranche, et elle dit que les dix valeurs sont simplement les dix
// PREMIÈRES émoticônes.
constexpr std::array<std::uint16_t, ro::ui::kChatMacroCount> kDefaultEmotions = {0, 1, 2, 3, 4,
                                                                                 5, 6, 7, 8, 9};

// Le storage alternatif 5 est celui des CARTES — alias `storagecard` côté
// serveur —, et c'est la seule des cinq qui mérite sa propre phrase.
constexpr int kAltStorageCount = 5;
constexpr int kCardStorage = 5;

// Une commande occupe DEUX entrées de textes : sa forme, et son explication.
constexpr std::size_t kTextsPerCommand = 2;

using ro::core::Printed;

}  // namespace

std::string ChatMacroDefault(std::size_t slot) {
  if (slot >= kDefaultEmotions.size()) {
    return {};
  }
  return std::string(EmotionCommandOf(kDefaultEmotions[slot]));
}

void MacroCommandCatalogue::Refresh() {
  if (language_serial_ == ro::core::LanguageSerial() && !groups_.empty()) {
    return;
  }
  language_serial_ = ro::core::LanguageSerial();

  texts_.clear();
  items_.clear();
  groups_.clear();
  group_labels_.clear();

  std::array<std::size_t, 4> counts{};
  const auto push = [this](const char* text, std::string help) {
    texts_.emplace_back(text);
    texts_.push_back(std::move(help));
  };

  // ── RAMASSAGE ──────────────────────────────────────────────────────────────
  push("@autoloot", Tr("Les items ramassés vont droit dans l'inventory."));
  push("@autolootmvp", Tr("Ramasse aussi les récompenses des MVP."));
  // ⚠ `@autolootpognon` PREND UN PRIX PLANCHER, et non un pourcentage : c'est
  // une commande maison, relevée dans `src/custom/atcommand.inc`.
  push("@autolootpognon 20000", Tr("Ne ramasse que les items valant au moins ce prix."));
  push("@autolootrare", Tr("Ne ramasse que les items rares listés par le serveur."));
  counts[0] = texts_.size() / kTextsPerCommand;

  // ── INFORMATIONS ───────────────────────────────────────────────────────────
  push("@uptime", Tr("Depuis combien de temps le serveur tourne."));
  push("@time", Tr("Date et heure du serveur."));
  push("@who", Tr("Liste les joueurs connectés, avec leur groupe et leur guilde."));
  counts[1] = texts_.size() / kTextsPerCommand - counts[0];

  // ── AUTRE ──────────────────────────────────────────────────────────────────
  // ⚠ PAS SOUS « DÉPLACEMENT », et c'est une correction de l'implémentation de
  // référence : cela ne vaudrait que pour `@load` — `@noks` est une règle de
  // combat et `@refresh` un rafraîchissement d'affichage.
  push("@load", Tr("Téléporte à votre point de sauvegarde."));
  push("@noks", Tr("Bascule la protection contre le vol de kill."));
  push("@refresh", Tr("Resynchronise votre position à l'écran."));
  counts[2] = texts_.size() / kTextsPerCommand - counts[0] - counts[1];

  // ── STORAGE ────────────────────────────────────────────────────────────────
  push("@storage", Tr("Ouvre le storage."));
  push("@storeall", Tr("Envoie tout l'inventory au storage."));
  // ⚠ `@gstorage` EST UN ALIAS DE `@guildstorage`, déclaré dans
  // `conf/atcommands.yml` : c'est la forme courte que les joueurs emploient.
  push("@gstorage", Tr("Ouvre le storage de guilde."));
  // Les cinq alternatifs, par paires « ouvrir / tout envoyer » : c'est ainsi
  // qu'on s'en sert. ⚠ `storage1..5` SONT DES ALIAS DE `storagealt1..5`,
  // déclarés dans `conf/import/atcommands.yml` — le fichier de base ment ici.
  for (int number = 1; number <= kAltStorageCount; ++number) {
    texts_.push_back(Printed("@storage%d", number));
    texts_.push_back(number == kCardStorage
                         ? std::string(Tr("Ouvre le storage alternatif 5, celui des cartes."))
                         : Printed(Tr("Ouvre le storage alternatif %d."), number));
    texts_.push_back(Printed("@storeall %d", number));
    texts_.push_back(Printed(Tr("Envoie tout l'inventory au storage alternatif %d."), number));
  }
  counts[3] = texts_.size() / kTextsPerCommand - counts[0] - counts[1] - counts[2];

  group_labels_.emplace_back(Tr("Ramassage"));
  group_labels_.emplace_back(Tr("Informations"));
  group_labels_.emplace_back(Tr("Autre"));
  group_labels_.emplace_back(Tr("Storage"));

  // ⚠ LES VUES SE POSENT APRÈS, quand les textes ne bougent plus : une vue ne
  // survit pas à la réallocation du vecteur qui la porte.
  items_.reserve(texts_.size() / kTextsPerCommand);
  for (std::size_t rank = 0; rank + 1 < texts_.size(); rank += kTextsPerCommand) {
    items_.push_back(ro::ui::ChatMacroCommand{.text = texts_[rank], .help = texts_[rank + 1]});
  }
  std::size_t at = 0;
  for (std::size_t rank = 0; rank < counts.size(); ++rank) {
    groups_.push_back(ro::ui::ChatMacroCommandGroup{
        .label = group_labels_[rank],
        .items = std::span<const ro::ui::ChatMacroCommand>(items_).subspan(at, counts[rank])});
    at += counts[rank];
  }
}

}  // namespace moonclient
