#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>

namespace ro::sim {

// LES COULEURS QU'UN JOUEUR A COMPOSÉES, telles que le serveur les fait
// circuler.
//
// 🔴 CE QUI VOYAGE NE PORTE AUCUNE COULEUR : rien que des DÉCALAGES par rampe,
// que chaque client ré-applique sur le `.spr` qu'il possède déjà. Voir l'ADR
// 0050 et `ro/render/palette_ramps.h`, où vit le calcul — ici il n'y a que ce
// que le fil a dit.
//
// ⚠ `sim` NE SAIT PAS CE QU'EST UNE RAMPE, et c'est voulu : il retient huit
// réglages et trois numéros, et `app` les traduit pour le rendu. La même
// frontière que `sim::Actor` face à `render::ActorLook` — `render` ne dépend
// pas de `sim`, et l'éditeur de liens le dit.

// Le plafond de rampes d'une recette. 🔴 IL DIMENSIONNE LA TRAME : le changer
// casse le protocole, et c'est pourquoi la version du fil existe.
inline constexpr std::size_t kStyleRampCount = 8;

// LA VERSION DU FIL.
//
// 🔴 ELLE SE COMPARE, ELLE NE SE MIGRE PAS : une version peut changer le
// CLASSEMENT des rampes, si bien que le rang trois d'une version ne désigne pas
// la même pièce du costume dans une autre. Réinterpréter une recette d'une
// autre version repeindrait les bottes en couleur de cape, avec l'aplomb d'un
// format valide.
//
// 🔴 ET C'EST LA MÊME POUR LE FIL ET POUR LES CODES ÉCRITS, ce qui n'est pas
// de la coquetterie : c'est ce qui garantit qu'un changement de format
// invalide les DEUX d'un coup, au lieu d'en laisser un servir des recettes mal
// alignées. Le serveur et les autres clients de ce serveur emploient la même.
inline constexpr std::uint8_t kStyleWireVersion = 7;

// ── CE QUE LE SERVEUR ACCEPTE ───────────────────────────────────────────────
//
// 🔴 CES BORNES SONT LES SIENNES, ET ELLES SE LISENT DANS SA CONFIGURATION :
// `conf/import/battle_conf.txt`, et surtout PAS `conf/battle/client.conf`, que
// l'import écrase — celui-ci annonce encore quarante-deux coiffures et huit
// couleurs, valeurs qui n'ont plus cours depuis longtemps.
//
// ⚠ `pc_changelook` BORNE AU LIEU DE REFUSER : un écart ici ne casse rien, il
// donnerait simplement au joueur une coiffure rabotée sans le lui dire. C'est
// pour cela que la fenêtre ne peut pas se contenter de « laisser essayer ».
//
// ⚠ ET LE PREMIER NUMÉRO EST UN, PAS ZÉRO. Zéro n'est pas une palette mais son
// ABSENCE — le jeu garde alors celle du sprite —, si bien que le proposer
// offrirait une teinte que personne ne voit ET décalerait tous les numéros par
// rapport à ceux que le serveur manipule.
inline constexpr int kStyleFirstChoice = 1;
inline constexpr int kHairStyleMax = 80;
inline constexpr int kHairPaletteMax = 251;
inline constexpr int kBodyPaletteMax = 553;

// Le réglage d'UNE rampe : cinq octets sur le fil.
struct StyleAdjust {
  // Relatif : -359..359. Absolu : 0..359.
  std::int16_t hue = 0;
  // Relatif : -100..100. Absolu : 0..100.
  std::int8_t saturation = 0;
  // TOUJOURS relatif : -100..100.
  std::int8_t value = 0;
  bool absolute = false;
};

// La recette d'UN corps : ses réglages, et les trois choix qui l'accompagnent.
struct StyleRecipe {
  std::array<StyleAdjust, kStyleRampCount> ramps{};
  // La palette de vêtement OFFICIELLE sur laquelle la recette s'applique ;
  // moins un vaut « celle que le serveur a assignée au personnage ».
  //
  // 🔴 ELLE FAIT PARTIE DE LA RECETTE parce qu'elle change la BASE, et la base
  // ne voyage pas : chaque client la recalcule. Un joueur qui la choisirait
  // sans que le numéro circule serait vu dans de tout autres couleurs.
  std::int16_t body_palette = -1;
  // Idem pour les cheveux et la coupe ; moins un vaut « ceux du personnage ».
  std::int16_t hair_palette = -1;
  std::int16_t hair_style = -1;
};

// CE QU'UN JOUEUR A COMPOSÉ, corps par corps.
//
// 🔴 UNE RECETTE PAR CORPS, ET CE N'EST PAS UN LUXE. Une recette ne désigne
// ses pièces que par des INDEX de palette, et les index n'ont aucune
// signification commune d'un corps à l'autre — mesuré : un Dragon Knight
// emploie 7-73, un Creator 112-223. Appliquer la même recette au corps du
// moment, quel qu'il soit, donnerait des couleurs plausibles mais AUTRES dès
// qu'on monte sur une monture.
struct StyleVariants {
  // Par clé de corps — le condensé du chemin de son `.spr`, tel que
  // `render::BodySpriteKey` le calcule. C'est le serveur qui l'impose.
  std::map<std::uint32_t, StyleRecipe> by_body;
  // La variante à reprendre pour un corps qui n'a pas la sienne ; zéro si
  // aucune n'est marquée.
  std::uint32_t fallback_body = 0;

  [[nodiscard]] bool empty() const noexcept { return by_body.empty(); }

  // La recette d'un corps : la sienne, sinon celle de repli, sinon rien.
  [[nodiscard]] const StyleRecipe* For(std::uint32_t body_key) const;
};

// CE QUE LE STYLISTE DEMANDE À LA FENÊTRE D'ÉDITION.
//
// ⚠ « BASCULER » N'EST PAS « OUVRIR », et le serveur distingue les trois : un
// PNJ veut pouvoir dire « ouvre » sans risquer de refermer ce que le joueur
// venait d'ouvrir lui-même.
enum class StyleEditorRequest : std::uint8_t { kClose, kOpen, kToggle };

// LES STYLES DE TOUT LE MONDE, par identifiant d'acteur.
//
// ⚠ IL VIT À PART DES ACTEURS, et c'est le fil qui l'impose : un lot arrive à
// l'entrée sur une carte peuplée, et il peut décrire un joueur dont l'acteur
// n'existe pas encore chez nous. Ranger la recette sur l'acteur la perdrait.
class StyleBook {
 public:
  // Ce qu'un acteur porte, ou rien.
  [[nodiscard]] const StyleVariants* Find(std::uint32_t gid) const;

  // 🔴 UN LOT REDÉFINIT INTÉGRALEMENT LES VARIANTES D'UN JOUEUR : le serveur
  // envoie toujours l'ensemble complet, même quand une seule a changé. On vide
  // donc à la PREMIÈRE entrée rencontrée pour un identifiant — et pas avant :
  // un lot tronqué, ou dont toutes les entrées sont d'une autre version,
  // effacerait sinon des variantes valides sans rien mettre à la place.
  void BeginBatchFor(std::uint32_t gid);
  void Remember(std::uint32_t gid, std::uint32_t body_key, const StyleRecipe& recipe,
                bool is_fallback);
  // Retire ce qu'on sait d'un acteur. ⚠ UN JOUEUR SANS AUCUNE VARIANTE NE SE
  // GARDE PAS : une entrée vide n'est pas une apparence, c'est son absence.
  void Forget(std::uint32_t gid);

  // Combien de fois ce qu'on sait d'un joueur a changé : zéro tant qu'aucun
  // lot ne l'a nommé.
  //
  // 🔴 C'EST CE QUI DISTINGUE « PAS ENCORE REÇU » DE « PLUS RIEN ». Les deux
  // rendent `Find` vide ; seul le second doit effacer ce qu'on a gardé ailleurs
  // de ce joueur.
  [[nodiscard]] std::uint32_t RevisionOf(std::uint32_t gid) const;

  [[nodiscard]] const std::map<std::uint32_t, StyleVariants>& everyone() const noexcept {
    return styles_;
  }

  // CE QUE LE STYLISTE A DEMANDÉ, une fois.
  //
  // 🔴 UNE DEMANDE, PAS UN ÉTAT, et elle se CONSOMME : laissée en place, elle
  // rouvrirait la fenêtre à chaque image et le joueur ne pourrait plus la
  // fermer.
  void Ask(StyleEditorRequest asked) { asked_ = asked; }
  [[nodiscard]] std::optional<StyleEditorRequest> TakeRequest() {
    const std::optional<StyleEditorRequest> asked = asked_;
    asked_.reset();
    return asked;
  }

 private:
  std::map<std::uint32_t, StyleVariants> styles_;
  // ⚠ SURVIT À `Forget` : un effacement est un changement comme un autre.
  std::map<std::uint32_t, std::uint32_t> revisions_;
  std::optional<StyleEditorRequest> asked_;
};

}  // namespace ro::sim
