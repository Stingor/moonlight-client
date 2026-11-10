#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

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

// ── LES TEINTURES DE CALQUES ────────────────────────────────────────────────
//
// À côté de la recette du corps, un joueur teint sa tête, sa peau, ses coiffes
// et sa cape. Chaque teinture se désigne par une CLÉ que tout observateur
// reçoit déjà : la coupe et la couleur de cheveux pour la tête, l'apparence
// affichée pour une coiffe, la robe affichée pour la cape, zéro pour la peau.
// Le calcul vit dans `ro/render/style_set.h` ; ici, ce que le fil dit.

// Le nombre de réglages d'une teinture : un par classe de teinte au profil 2,
// dont le profil 1 n'emploie que les huit premiers.
inline constexpr std::size_t kStyleLayerAdjustCount = 13;

// Le calque, numéroté comme sur le fil.
// `kBodyMore` est la suite du corps : les rampes 9 à 12, sous la clé du corps.
enum class StyleLayerKind : std::uint8_t {
  kHead = 1,
  kHeadgear = 2,
  kGarment = 3,
  kSkin = 4,
  kBodyMore = 5
};

// UNE RETOUCHE : la couleur exacte d'un index de la palette du calque.
struct StyleRetouch {
  std::uint8_t index = 0;
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;

  [[nodiscard]] friend bool operator==(const StyleRetouch&, const StyleRetouch&) = default;
};

// LES RETOUCHES D'UN CALQUE, au plus. C'est ce que l'octet de taille d'une
// entrée permet : 13 octets d'en-tête et de queue, 21 réglages de 5 octets,
// 32 retouches de 4 octets, 246 octets sur 255.
inline constexpr std::size_t kStyleRetouchesMost = 32;

// Ni l'index zéro, le transparent, ni le magenta pur.
[[nodiscard]] bool StyleRetouchAllowed(const StyleRetouch& retouch) noexcept;

// LA TEINTURE D'UN CALQUE.
struct StyleLayerRecipe {
  StyleLayerKind layer = StyleLayerKind::kHead;
  std::uint32_t key = 0;
  // Le profil de détection qui l'a découpée. Un profil que le rendu ne connaît
  // pas laisse le calque dans ses couleurs d'origine.
  std::uint8_t profile = 0;
  std::array<StyleAdjust, kStyleLayerAdjustCount> adjusts{};
  // LE PROFIL MIXTE, propre à moonclient : un réglage par classe de teinte pour
  // ce qu'aucune pièce ne couvre (profils 1 et 3), puis les retouches, triées
  // par index. Un lecteur qui ne les connaît pas les enjambe.
  std::array<StyleAdjust, kStyleLayerAdjustCount> rest{};
  std::vector<StyleRetouch> retouches;
};

// Le plus de teintures qu'un bloc du serveur décrit pour un joueur : sa tête, sa
// peau, ses quatre exemplaires visibles, et la suite de ses quatre corps au
// plus. Au-delà, l'entrée est ignorée.
inline constexpr std::size_t kStyleShownItemsMost = 4;
inline constexpr std::size_t kStyleBodyVariantsMost = 4;
inline constexpr std::size_t kStyleLayersShownMax =
    1 + 1 + kStyleShownItemsMost + kStyleBodyVariantsMost;

// CE QUE LE SERVEUR DIT D'UNE DEMANDE, par le verdict qui suit chaque envoi.
enum class StyleSubject : std::uint8_t { kBody = 0, kLayers = 1, kAtelier = 2 };
enum class StyleVerdictResult : std::uint8_t {
  kAccepted = 0,
  // Le même envoi que ce qui est rangé : rien n'a changé.
  kSame = 1,
  kTooSoon = 2,
  kMalformed = 3,
  kCap = 4,
  kKeyRefused = 5,
  // Une teinture d'objet hors de l'atelier du styliste.
  kNoAtelier = 6,
  // L'objet n'est plus porté à son emplacement.
  kNotWorn = 7,
  // Mort, échange, boutique, storage ou dialogue en cours.
  kBusy = 8,
  kUnavailable = 9,
};
// Le dernier résultat que ce client sait nommer : au-delà, il le lit comme un refus.
inline constexpr std::uint8_t kStyleVerdictLastKnown =
    static_cast<std::uint8_t>(StyleVerdictResult::kUnavailable);

struct StyleVerdict {
  StyleSubject subject = StyleSubject::kBody;
  // Un résultat que ce client ne connaît pas reste dans `raw_result`, et
  // `result` vaut alors `kUnavailable`.
  StyleVerdictResult result = StyleVerdictResult::kAccepted;
  std::uint8_t raw_result = 0;
  // Pour « trop tôt » : quand revenir.
  std::uint16_t wait_ms = 0;
  // Le corps évincé par la validation d'un corps neuf, zéro sinon.
  std::uint32_t evicted_body_key = 0;
  // Le numéro de ce verdict : il avance à chacun, et c'est ce qui dit à un
  // lecteur qu'un verdict est neuf. Zéro : aucun verdict reçu.
  std::uint32_t sequence = 0;
};

// L'ATELIER DU STYLISTE, tel que le serveur l'a annoncé : une teinture d'objet ne
// se valide que pendant qu'il est ouvert. Le serveur en juge seul ; ce client ne
// s'en sert que pour savoir quand proposer « Valider » sur un objet.
struct StyleAtelier {
  bool open = false;
  // L'instant, sur l'horloge du client, où le serveur le fermera.
  std::uint32_t closes_at_ms = 0;

  [[nodiscard]] bool OpenAt(std::uint32_t now_ms) const noexcept {
    return open && static_cast<std::int32_t>(closes_at_ms - now_ms) > 0;
  }
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
  // Retire les variantes de corps d'un acteur ; ses teintures de calques, qui
  // voyagent à part, restent. ⚠ UN JOUEUR SANS AUCUNE VARIANTE NE SE
  // GARDE PAS : une entrée vide n'est pas une apparence, c'est son absence.
  void Forget(std::uint32_t gid);

  // OUBLIE UN JOUEUR QUI N'EST PLUS EN VUE : ses variantes, ses teintures de
  // calques, ET sa révision.
  //
  // 🔴 CE N'EST PAS `Forget`. Un effacement dit « il n'a plus de couleurs » et
  // compte comme un changement ; un départ dit « on ne sait plus », et le livre
  // redevient ce qu'il était avant le premier lot de ce joueur. Le serveur redit
  // ses couleurs quand il réapparaît ou rentre en vue : c'est ce qui borne la
  // mémoire, et ce qui efface les couleurs d'un joueur qui les a supprimées hors
  // de notre vue — le serveur ne dit rien d'un joueur sans style.
  void ForgetDeparted(std::uint32_t gid);
  // TOUS SONT PARTIS SAUF `kept`, à l'entrée sur une carte. Le joueur garde les
  // siennes : le serveur ne redit jamais à un joueur ses propres couleurs.
  void ForgetDepartedExcept(std::uint32_t kept);

  // Le numéro du dernier changement de ce qu'on sait d'un joueur : il change à
  // chaque lot qui le nomme, et à chaque effacement. Zéro tant qu'aucun lot ne
  // l'a nommé, ou depuis son départ.
  //
  // 🔴 C'EST CE QUI DISTINGUE « PAS ENCORE REÇU » DE « PLUS RIEN ». Les deux
  // rendent `Find` vide ; seul le second doit effacer ce qu'on a gardé ailleurs
  // de ce joueur.
  [[nodiscard]] std::uint32_t RevisionOf(std::uint32_t gid) const;

  [[nodiscard]] const std::map<std::uint32_t, StyleVariants>& everyone() const noexcept {
    return styles_;
  }

  // ── LES TEINTURES DE CALQUES, par joueur ──────────────────────────────────

  // Ce qu'un acteur porte, ou rien.
  [[nodiscard]] const std::vector<StyleLayerRecipe>* FindLayers(std::uint32_t gid) const;
  // 🔴 UN BLOC REDÉFINIT TOUTES LES TEINTURES DE CALQUES D'UN JOUEUR, comme un
  // lot de corps : le serveur les envoie toujours ensemble. Une liste vide les
  // efface. Les variantes du corps n'en sont pas touchées : elles voyagent à
  // part.
  void RememberLayers(std::uint32_t gid, std::vector<StyleLayerRecipe> layers);

  // CE QUE LE STYLISTE A DEMANDÉ, une fois.
  //
  // 🔴 UNE DEMANDE, PAS UN ÉTAT, et elle se CONSOMME : laissée en place, elle
  // rouvrirait la fenêtre à chaque image et le joueur ne pourrait plus la
  // fermer.
  //
  // `tab` : l'onglet sur lequel ouvrir, compté depuis un, ou zéro pour garder
  // celui que le joueur avait. Il se consomme avec la demande.
  void Ask(StyleEditorRequest asked, std::uint8_t tab = 0) {
    asked_ = asked;
    asked_tab_ = tab;
  }
  [[nodiscard]] std::optional<StyleEditorRequest> TakeRequest() {
    const std::optional<StyleEditorRequest> asked = asked_;
    asked_.reset();
    return asked;
  }
  // L'onglet de la dernière demande, compté depuis un, ou zéro ; à lire avec
  // elle, et une seule fois.
  [[nodiscard]] std::uint8_t TakeTab() noexcept {
    const std::uint8_t tab = asked_tab_;
    asked_tab_ = 0;
    return tab;
  }

  // ── LES VERDICTS ET L'ATELIER ─────────────────────────────────────────────

  // LE DERNIER VERDICT du serveur, par objet : le corps et les calques partent du
  // même clic, et leurs deux verdicts arrivent dans la même image. Il se lit sans
  // se consommer : son numéro dit s'il est neuf.
  void HearVerdict(StyleVerdict verdict);
  // Celui du corps ou des calques ; son numéro est nul tant qu'aucun n'est venu.
  [[nodiscard]] const StyleVerdict& LastVerdict(StyleSubject subject) const noexcept;
  void SetAtelier(const StyleAtelier& atelier) noexcept { atelier_ = atelier; }
  [[nodiscard]] const StyleAtelier& atelier() const noexcept { return atelier_; }

 private:
  std::map<std::uint32_t, StyleVariants> styles_;
  std::map<std::uint32_t, std::vector<StyleLayerRecipe>> layers_;
  // ⚠ SURVIT À `Forget` : un effacement est un changement comme un autre.
  std::map<std::uint32_t, std::uint32_t> revisions_;
  // ⚠ UN SEUL COMPTEUR POUR TOUS : une révision ne revient jamais, pas même
  // celle d'un joueur parti puis revenu. Comparée à une valeur gardée d'avant
  // son départ, une révision recomptée depuis un dirait sinon « rien n'a
  // changé ».
  std::uint32_t last_revision_ = 0;
  std::optional<StyleEditorRequest> asked_;
  std::uint8_t asked_tab_ = 0;

  StyleVerdict body_verdict_{.subject = StyleSubject::kBody};
  StyleVerdict layers_verdict_{.subject = StyleSubject::kLayers};
  std::uint32_t last_verdict_ = 0;
  StyleAtelier atelier_;
};

}  // namespace ro::sim
