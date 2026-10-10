// Les mots des commerces. Voir l'en-tête.

#include "ro/vfs/trade_words.h"

#include <span>
#include <tuple>
#include <utility>
#include <variant>

#include "ro/core/i18n.h"
#include "ro/vfs/item_words.h"

#include "message_word.h"

namespace ro::vfs {
namespace {

// Le nom sous lequel un gabarit fautif se journalise.
constexpr std::string_view kTemplateName = "un gabarit des commerces";

// LA MISE EN PAGE DES RÉCAPITULATIFS, qui n'appartient à aucune langue : le
// bandeau autour du titre, le retrait d'une ligne d'objet, et celui, bien plus
// large, qui pousse le zeny vers la droite.
constexpr std::string_view kRecapBannerLead = "======================  ";
constexpr std::string_view kRecapBannerTrail = "  ======================";
constexpr std::string_view kRecapItemIndent = "    ";
constexpr std::string_view kRecapZenyIndent = "                                          ";

// UN ARGUMENT À IMPRIMER : une chaîne ou un entier, et rien d'autre.
using Argument = std::variant<std::string_view, std::int64_t>;

// La lettre de conversion qu'attend un argument.
char ShapeOf(const Argument& argument) {
  return std::holds_alternative<std::string_view>(argument) ? 's' : 'd';
}

// Imprime le gabarit SANS la libc : chaque conversion est remplacée par son
// argument, dans l'ordre. `TradeFormatShape` a déjà vérifié que la suite
// correspond — cette fonction ne s'appelle pas autrement.
std::string Substitute(std::string_view format, std::span<const Argument> arguments) {
  std::string out;
  out.reserve(format.size());
  std::size_t next = 0;
  for (std::size_t at = 0; at < format.size(); ++at) {
    if (format[at] != '%' || at + 1 >= format.size()) {
      out.push_back(format[at]);
      continue;
    }
    const char conversion = format[++at];
    if (conversion == '%') {
      out.push_back('%');
      continue;
    }
    const Argument& argument = arguments[next++];
    if (const auto* text = std::get_if<std::string_view>(&argument)) {
      out.append(*text);
    } else {
      out.append(std::to_string(std::get<std::int64_t>(argument)));
    }
  }
  return out;
}

std::string Format(const ItemWords& words, TradeWord word, std::span<const Argument> arguments) {
  const std::string_view format = TradeText(words, word);
  if (format.empty()) {
    return {};
  }
  std::string expected;
  for (const Argument& argument : arguments) {
    expected.push_back(ShapeOf(argument));
  }
  if (TradeFormatShape(format) != expected) {
    return std::string(NoteMissingWord(kTemplateName));
  }
  return Substitute(format, arguments);
}

}  // namespace

std::string_view TradeText(const ItemWords& words, TradeWord word) {
  const auto rank = static_cast<std::size_t>(word);
  if (rank >= kTradeWordCount) {
    return {};
  }
  return words.trade.texts[rank];
}

std::string TradeFormatShape(std::string_view format) {
  std::string shape;
  for (std::size_t at = 0; at < format.size(); ++at) {
    if (format[at] != '%') {
      continue;
    }
    if (at + 1 >= format.size()) {
      return {};
    }
    const char conversion = format[++at];
    switch (conversion) {
      case '%':
        break;
      case 's':
        shape.push_back('s');
        break;
      case 'd':
      case 'i':
      case 'u':
        shape.push_back('d');
        break;
      default:
        // Largeur, précision, « %n », « %x » : rien de ce qu'on sait imprimer
        // sans la libc. Le gabarit entier est refusé.
        return std::string(1, '?');
    }
  }
  return shape;
}

std::string TradeTextWith(const ItemWords& words, TradeWord word, std::int64_t number) {
  const std::array<Argument, 1> arguments = {number};
  return Format(words, word, arguments);
}

std::string TradeTextWith(const ItemWords& words, TradeWord word, std::string_view text) {
  const std::array<Argument, 1> arguments = {text};
  return Format(words, word, arguments);
}

std::string TradeTextWith(const ItemWords& words, TradeWord word, std::string_view text,
                          std::int64_t number) {
  const std::array<Argument, 2> arguments = {text, number};
  return Format(words, word, arguments);
}

std::string TradeTextWith(const ItemWords& words, TradeWord word, std::string_view first,
                          std::string_view second) {
  const std::array<Argument, 2> arguments = {first, second};
  return Format(words, word, arguments);
}

std::string TradeTextWith(const ItemWords& words, TradeWord word, std::string_view text,
                          std::int64_t first, std::int64_t second) {
  const std::array<Argument, 3> arguments = {text, first, second};
  return Format(words, word, arguments);
}

std::string TradeTextWith(const ItemWords& words, TradeWord word, std::string_view first,
                          std::string_view second, std::string_view third) {
  const std::array<Argument, 3> arguments = {first, second, third};
  return Format(words, word, arguments);
}

void FillTradeWords(TradeWords& words) {
  // DANS L'ORDRE DE `TradeWord` : le rang de l'un est l'indice de l'autre.
  auto texts = std::to_array<std::string>({
      core::Tr("L'échange a été effectué."),
      core::Tr("Vous n'avez pas assez de zeny."),
      core::Tr("Vous dépassez votre limite de poids."),
      core::Tr("L'échange a échoué."),
      core::Tr("Capacité maximale dépassée"),
      core::Tr("Achat impossible : la quantité maximale de cet item est dépassée."),
      core::Tr("Le produit des ventes en vending sera envoyé par RODEX"),
      core::Tr("Échec de l'achat"),
      core::Tr("Impossible d'acheter plus de %d exemplaires du même item à la fois"),
      core::Tr("Impossible d'acheter plus de %d exemplaires du même item à la fois"),
      core::Tr("Veuillez éviter d'acheter 2 fois le même item en une seule fois."),
      core::Tr("Libérez de la place dans l'inventory"),
      core::Tr("Items du shop"),
      core::Tr("Items vendables"),
      core::Tr("Points de [%s] : %d points"),
      core::Tr("Pts d'événement de [%s] : %d pts"),
      core::Tr("L'achat a échoué car le NPC n'existe pas."),
      core::Tr("L'achat a échoué car le Kafra shop ne fonctionne pas correctement."),
      core::Tr("Vous ne pouvez pas acheter d'items pendant un échange."),
      core::Tr("L'achat a échoué car les informations de l'item étaient incorrectes."),
      Edged(" ", core::Tr("Vous n'avez pas assez de points Kafra."), ""),
      core::Tr("Vous pouvez acheter jusqu'à 10 items."),
      core::Tr("Certains items n'ont pas pu être achetés."),
      core::Tr("Ouverture d'une échoppe"),
      core::Tr("Mon shop"),
      core::Tr("Shop du marchand"),
      core::Tr("Historique des ventes"),
      core::Tr("Impossible d'ouvrir le shop."),
      core::Tr("Impossible d'ouvrir un shop à cet endroit"),
      core::Tr("Le total des items à vendre dépasse la quantité de zeny que vous pouvez posséder. "
               "\nVeuillez corriger la quantité et le prix."),
      core::Tr("Au moins un item a son prix fixé à 0 zeny. Voulez-vous continuer ?"),
      core::Tr("Évitez d'ouvrir un salon de chat pendant le vending."),
      core::Tr("%s %d vendu(s)."),
      core::Tr("Les items sont épuisés."),
      core::Tr("%s Achat échoué %s"),
      core::Tr("L'achat de %s a échoué : rupture de stock. Stock actuel : %d."),
      core::Tr("Vous ne pouvez pas utiliser un shop de NPC pendant un échange."),
      core::Tr("Échec de l'achat : informations du shop incorrectes."),
      core::Tr("Aucune information de vente."),
      Edged("", core::Tr("Échec de l'envoi des zeny"), " "),
      core::Tr("Vous pouvez acheter jusqu'à %d exemplaires du même item à la fois."),
      core::Tr("Impossible d'acheter dans un shop privé pendant l'ouverture d'un shop d'achat."),
      core::Tr("Fenêtre de buying store"),
      core::Tr("Limite de zeny d'achat"),
      core::Tr("Liste d'achat :"),
      core::Tr("Items recherchés"),
      core::Tr("Échec de l'ouverture du buying store."),
      core::Tr("Vous dépassez la quantité totale d'items."),
      core::Tr("Vous avez acheté tous les items dans la limite de prix."),
      core::Tr("Vous avez acheté tous les items."),
      core::Tr("Échange impossible : vous n'avez pas assez de zeny."),
      core::Tr("Vous avez vendu %s. Quantité : %d. Total : %dz"),
      core::Tr("%s n'a pas pu être vendu : vous n'avez pas la quantité demandée."),
      core::Tr("Vous pouvez vendre au maximum %d items."),
      core::Tr("L'acheteur n'a pas assez de zeny, réduisez la quantité vendue."),
      core::Tr(
          "Votre manque de zeny ou votre limite de zeny vous empêche d'effectuer cette action."),
      core::Tr("Échec de l'échange."),
      core::Tr("Échange effectué avec succès."),
      core::Tr("L'item a été vendu, il n'y a plus de stock."),
      core::Tr("Items insuffisants pour l'échange."),
      core::Tr("Items à échanger"),
      core::Tr("%s %d u."),
      core::Tr("Les items équipés ne peuvent pas être échangés. Veuillez d'abord les déséquiper."),
      core::Tr("Vous ne pouvez acheter qu'un item à la fois."),
      core::Tr("L'échange a été refusé."),
      core::Tr("Vous êtes trop loin de cette personne pour échanger."),
      core::Tr("Ce personnage n'est pas connecté ou n'existe pas."),
      core::Tr("Cette personne est déjà en cours d'échange."),
      core::Tr("Échange impossible : ce personnage dépasserait sa limite de poids."),
      core::Tr("L'échange a été annulé."),
      core::Tr("L'échange a été effectué."),
      core::Tr("L'échange a échoué."),
      Edged(" ", core::Tr("vous propose un échange."), ""),
      core::Tr("Vous ne pouvez pas échanger plus de 10 types d'items par échange."),
      core::Tr("Impossible d'utiliser, d'équiper ou de déséquiper un item pendant un échange."),
      core::Tr("Rechercher des vendeurs"),
      core::Tr("Vendre"),
      core::Tr("Acheter"),
      core::Tr("Nom du shop"),
      core::Tr("Item"),
      core::Tr("Qté"),
      core::Tr("Prix"),
      core::Tr("Inclure les items similaires ?"),
      core::Tr("Saisissez le nom de la carte ou le préfixe/suffixe."),
      core::Tr("Essais : %d"),
      core::Tr("Voulez-vous ouvrir l'échoppe ?"),
      core::Tr("Trop de résultats trouvés. Veuillez affiner votre recherche."),
      core::Tr("Impossible de poursuivre la recherche."),
      core::Tr("Vous ne pouvez pas encore relancer la recherche."),
      core::Tr("Aucun résultat."),
      core::Tr("L'item sélectionné n'existe pas."),
      core::Tr("Le prix de l'item est trop élevé."),
      core::Tr("Veuillez saisir le nom de l'item."),
      core::Tr("Le nom ou le préfixe/suffixe sélectionné n'existe pas."),
      core::Tr("Préc."),
      core::Tr("Suiv."),
      Edged(kRecapBannerLead, core::Tr("LISTE DE VENTE"), kRecapBannerTrail),
      Edged(kRecapBannerLead, core::Tr("LISTE D'ACHAT"), kRecapBannerTrail),
      Edged(kRecapItemIndent, core::Tr("%s  :  %s  Zeny"), ""),
      core::Tr("%s : %s  Zeny => %s u."),
      core::Tr("Commission : 1 u. = %s Zeny"),
      Edged(kRecapZenyIndent, core::Tr("Zeny actuels : %s Z"), ""),
  });
  // ⚠ UN TEXTE OUBLIÉ NE SE VERRAIT PAS : tous les mots suivants seraient
  // décalés d'un rang. Le compte se vérifie à la compilation.
  static_assert(std::tuple_size_v<decltype(texts)> == kTradeWordCount);
  words.texts = std::move(texts);
}

}  // namespace ro::vfs
