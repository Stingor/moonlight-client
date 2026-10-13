#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/formats/palette.h"
#include "ro/render/actor_layers.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/vfs/monster_size_effects.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// Un acteur habillé : ses pièces, où sa cape passe, et ce qui en a été composé.
//
// La composition est gardée par animation, par IMAGE et par direction, et ne se
// fait qu'une fois pour chacune. Les huit directions le sont d'un coup : en
// ligne, chacun regarde où il va, et composer par acteur referait le même
// travail autant de fois qu'il y a de gens tournés pareil.
//
// 🔴 UNE COMPOSITION RENDUE N'EST JAMAIS RÉÉCRITE. La tenue est partagée par
// tous les acteurs de même apparence, et la scène garde un POINTEUR vers ce
// qu'elle a reçu : réécrire en place l'image d'une animation donnerait à dix
// Porings qui marchent le pas du dernier composé.
class ActorOutfit {
 public:
  // Sans placement, la cape passe devant : c'est le choix du client sans
  // table, et celui d'une tenue composée hors jeu. Sans ombre — pièce nulle ou
  // échelle nulle — l'acteur n'en porte pas. body_scale grandit ou rétrécit
  // l'acteur ENTIER, en pixels de sprite : un, sauf les bébés — voir
  // BodyScaleFor.
  explicit ActorOutfit(std::vector<ActorPiece> pieces, GarmentPlacement garment = {},
                       ActorPiece shadow = {}, float shadow_scale = 0.0F, float body_scale = 1.0F);

  // Le corps d'abord, puis la tête et ce qui s'y accroche.
  [[nodiscard]] std::span<const ActorPiece> pieces() const noexcept { return pieces_; }

  // L'ombre au sol, à dessiner AVANT l'acteur ; vide s'il n'en porte pas.
  //
  // Elle vit à part de la composition plutôt qu'en tête de ses calques : « le
  // premier calque » veut dire quelque chose — c'est là que passe une cape mise
  // derrière —, et y glisser l'ombre changerait ce sens sans le dire. Elle ne
  // suit d'ailleurs ni l'action ni l'image : le client en dessine toujours la
  // même image, grandie de l'échelle de la classe ET de celle du sprite.
  [[nodiscard]] const ComposedActor& Shadow() const noexcept { return shadow_; }

  // L'acteur composé pour cette animation et cette direction, à cet instant.
  //
  // elapsed_ms se compte depuis le DÉBUT de l'animation. En boucle — le cas
  // ordinaire, celui de l'attente et de la marche —, l'image revient au tour
  // passé ; jouée une fois, elle s'arrête sur la dernière, et un mort reste à
  // terre. Un .act qui ne porte pas l'animation demandée joue l'attente :
  // lire hors de sa table donnerait un acteur invisible, ou pire.
  //
  // held_frame TIENT l'acteur sur une image au lieu de la déduire du temps ;
  // elapsed_ms ne compte alors plus. Les trois images de l'action d'attente
  // d'un personnage — corps, tête et coiffes — ne sont pas une animation : ce
  // sont les trois orientations du regard, et le serveur dit laquelle. Les
  // faire défiler donne un personnage qui dit non sans arrêt.
  [[nodiscard]] const ComposedActor& Composed(std::size_t animation, std::size_t direction,
                                              float elapsed_ms, bool loop = true,
                                              std::optional<std::size_t> held_frame = std::nullopt);

  // L'ORDONNÉE DU PREMIER CALQUE DU CORPS, à la première image de la première
  // action, TELLE QUE LE CLIENT LA TIENT EN MÉMOIRE : celle du fichier moins la
  // demi-hauteur de son image. Vide sans corps, action, image ni calque.
  [[nodiscard]] std::optional<float> BodyFirstLayerY() const;

  // LA PLACE DE REPOS DE SA TÊTE : celle de la première image de l'attente, posée
  // sur le corps de `foot` — la tenue du même acteur à pied, voir `FootJobOf`.
  // Vide sans tête ni corps. Voir `HeadPlace`.
  [[nodiscard]] std::optional<glm::vec2> HeadRestingPlace(const ActorOutfit& foot) const;

  // La durée d'un tour de l'animation, et celle d'une de ses images, en
  // millisecondes ; zéro si le .act du corps ne la porte pas.
  [[nodiscard]] float AnimationDurationMs(std::size_t animation) const;
  [[nodiscard]] float FrameIntervalMs(std::size_t animation) const;
  // Le nombre d'images de l'animation, lu sur le .act du CORPS ; zéro s'il n'y
  // a pas de corps. Une animation qu'il ne porte pas compte celles de
  // l'attente, comme la composition.
  [[nodiscard]] std::size_t FrameCount(std::size_t animation) const;
  // Le nombre d'images d'une action du CORPS, compté comme le client : une
  // action que le .act ne porte pas, ou qui n'a aucune image, en compte une.
  // Rien ne s'y substitue, à la différence de `FrameCount`.
  [[nodiscard]] std::size_t BodyActionFrameCount(std::size_t animation,
                                                 std::size_t direction) const;
  // Le délai qu'une action du CORPS déclare, en pas de vingt-quatre
  // millisecondes ; une action que le .act ne porte pas en a un de
  // `ro::render::kUndeclaredActionDelayUnits`, comme sans corps.
  [[nodiscard]] float BodyActionDelayUnits(std::size_t animation, std::size_t direction) const;
  // LES HORLOGES PROPRES des pièces accrochées qui en ont une pour cette
  // animation : les coiffes et la cape animées, voir `AlternateClockOf`. Vide
  // quand tout suit le corps, ou sans tête qui leur serve de référence. Lues sur
  // la direction zéro, comme la composition.
  [[nodiscard]] std::vector<AlternateClock> OwnClocks(std::size_t animation) const;
  // L'IMAGE DE FRAPPE de l'animation, lue sur le .act du CORPS : la première
  // qui porte le marqueur du coup, sinon l'avant-dernière — voir
  // `ro::render::AttackSoundFrame`. Zéro sans corps.
  [[nodiscard]] std::size_t AttackSoundFrame(std::size_t animation) const;

 private:
  std::vector<ActorPiece> pieces_;
  GarmentPlacement garment_;
  ComposedActor shadow_;
  float body_scale_ = 1.0F;
  // Ce qui désigne une composition dans une animation : l'image du corps, puis
  // celle de chaque pièce accrochée. Une coiffe animée change d'image SANS que
  // le corps bouge : sans la seconde moitié, le cache la figerait sur sa
  // première image.
  using FrameKey = std::pair<std::size_t, std::vector<std::size_t>>;
  // L'ordre des clés, qui se compare aussi à une clé EMPRUNTÉE — l'image du
  // corps et une vue sur celles des pièces. ⚠ C'EST CE QUI ÉPARGNE UNE
  // ALLOCATION PAR ACTEUR ET PAR IMAGE : la clé ne se bâtit qu'à l'insertion.
  // Même ordre que celui de la paire.
  struct FrameKeyOrder {
    using is_transparent = void;
    using Probe = std::pair<std::size_t, std::span<const std::size_t>>;
    [[nodiscard]] static Probe View(const FrameKey& key) noexcept {
      return {key.first, key.second};
    }
    [[nodiscard]] static Probe View(const Probe& probe) noexcept { return probe; }
    template <class Left, class Right>
    [[nodiscard]] bool operator()(const Left& left, const Right& right) const noexcept {
      const Probe one = View(left);
      const Probe other = View(right);
      if (one.first != other.first) {
        return one.first < other.first;
      }
      return std::ranges::lexicographical_compare(one.second, other.second);
    }
  };
  // Les compositions déjà faites, par animation du corps puis par image. Un
  // `std::map` parce que ses nœuds ne bougent pas : une référence rendue reste
  // valide tant que la tenue vit. Le nombre d'entrées est borné par celui des
  // images du .act.
  std::vector<std::map<FrameKey, std::array<ComposedActor, kDirectionCount>, FrameKeyOrder>>
      composed_;
  // L'image de chaque pièce accrochée pour l'appel en cours, gardée d'un
  // appel à l'autre pour ne pas se réallouer.
  std::vector<std::size_t> attachment_frames_;
  // Le .act de la TÊTE : c'est lui qui sert de référence à l'animation propre
  // des coiffes. Nul pour une tenue qui n'en porte pas.
  const formats::ActAnimation* head_animation_ = nullptr;
  // Ce qu'on rend quand il n'y a rien à composer : personne ne dessine.
  ComposedActor nothing_;
};

// L'ordre des apparences, champ par champ. C'est la clé du cache : deux
// acteurs à l'apparence égale partagent tout.
struct ActorLookOrder {
  [[nodiscard]] bool operator()(const ActorLook& left, const ActorLook& right) const noexcept;
};

// La garde-robe : les sprites de tous les acteurs, résolus depuis leur
// apparence par le catalogue et gardés en cache.
//
// Deux acteurs qui se ressemblent partagent leurs pièces ET leur composition :
// dix Porings coûtent un Poring.
//
// Une pièce absente ou abîmée n'emporte que ce qu'elle habille — le corps
// manquant, l'acteur entier, puisque tout s'y ancre ; une coiffe manquante,
// elle seule. Elle se note une fois, pour le journal, et n'est plus
// redemandée : ce que le client n'a pas à la première image, il ne l'aura pas
// à la suivante.
class ActorWardrobe {
 public:
  ActorWardrobe(vfs::Vfs& vfs, SpriteCatalog catalog);

  ActorWardrobe(const ActorWardrobe&) = delete;
  ActorWardrobe& operator=(const ActorWardrobe&) = delete;

  // L'acteur habillé, ou nullptr s'il n'a pas de corps à montrer.
  //
  // Le pointeur reste valable tant que la garde-robe vit : les tenues ne se
  // déplacent pas.
  [[nodiscard]] ActorOutfit* OutfitFor(const ActorLook& look);

  // ── LES STYLES : des couleurs de corps que le joueur a composées ────────
  //
  // Une recette ne dit que des décalages par rampe ; la garde-robe la
  // transforme en palette, parce qu'elle seule tient le sprite du corps et la
  // palette de vêtement sur lesquels elle s'applique. Voir
  // `ro/render/palette_ramps.h`.

  // Enregistre une recette sous cet identifiant, que `ActorLook::style`
  // reprendra. Rend faux si l'identifiant est nul ou DÉJÀ PRIS.
  //
  // 🔴 UNE RECETTE NE SE REMPLACE JAMAIS EN PLACE, et le refus est la règle
  // elle-même. Une tenue déjà composée retient l'ADRESSE de sa palette dans la
  // provenance de chacun de ses calques, et c'est cette provenance qui sert de
  // clé aux textures : repeindre les octets sous le même pointeur ne se
  // verrait pas, puis la couleur basculerait toute seule à la première
  // éviction. Chaque version d'une recette prend donc un identifiant NEUF.
  // LA CLÉ DU CORPS de cette apparence : celle du `.spr` réellement CHARGÉ.
  //
  // 🔴 CELLE DU CHEMIN PRÉVU NE SUFFIRAIT PAS. Une tenue alternative absente de
  // l'installation fait retomber sur le corps ordinaire — voir
  // `BodySpritePaths` —, et une clé prise sur le premier candidat désignerait
  // alors une variante de couleurs que ce client ne montre pas.
  //
  // Zéro quand aucun corps ne se charge : c'est la sentinelle « corps
  // inconnu », qui ne désigne jamais une variante.
  //
  // ⚠ ELLE NE DÉPEND PAS DU STYLE, et c'est ce qui permet de la demander
  // AVANT d'en choisir un : la recette change la palette du corps, jamais quel
  // corps on porte.
  [[nodiscard]] std::uint32_t BodyKeyFor(const ActorLook& look);

  // CE QU'IL Y A À RÉGLER SUR CE CORPS : sa base, et ses rampes.
  //
  // La base est la palette du sprite FUSIONNÉE avec celle du serveur — c'est
  // sur elle que les rampes se détectent, et c'est d'elle que l'éditeur tire
  // la pastille de chaque rampe. Les deux vont ensemble : une rampe désigne
  // des index, et un index ne veut rien dire sans la palette où on le lit.
  //
  // ⚠ SANS STYLE : c'est ce qu'on RÈGLE, pas ce qu'on a déjà réglé. Appliquer
  // la recette avant de détecter ferait détecter les rampes de la palette
  // peinte, donc dériver un peu plus à chaque passage.
  //
  // Rampes vides quand l'apparence n'a pas de corps à montrer.
  struct BodyRamps {
    formats::Palette base;
    std::vector<PaletteRamp> ramps;
    // Les pixels peints du corps, et ceux qu'une rampe couvre.
    //
    // 🔴 L'ÉCART EST LA PART QU'AUCUN RÉGLAGE N'ATTEINT, et il se dit. Au-delà
    // des huit plus grands dégradés, le reste du corps n'a pas de commande et
    // n'en aura pas ; le taire laisse le joueur chercher indéfiniment la pièce
    // qui commande une zone que rien ne commande.
    int pixels_total = 0;
    int pixels_covered = 0;
  };
  [[nodiscard]] BodyRamps BodyRampsFor(const ActorLook& look);

  bool SetStyle(std::uint32_t style, const PaletteRecipe& recipe);

  // UN IDENTIFIANT DE STYLE NEUF, jamais distribué ni réemployé : un identifiant
  // qui revient désignerait, pour une tenue encore en cache, une recette qui
  // n'est plus la sienne.
  //
  // 🔴 LA GARDE-ROBE LES DISTRIBUE, ET PERSONNE D'AUTRE. L'éditeur, l'aperçu
  // d'un style reçu et les styles des joueurs y posent tous les leurs : trois
  // compteurs partis de zéro se rencontreraient, et un joueur porterait la
  // palette de l'éditeur.
  [[nodiscard]] std::uint32_t NewStyleId() noexcept { return ++last_style_id_; }

  // LA MÊME CHOSE, MAIS LA PALETTE TOUTE FAITE.
  //
  // 🔴 POUR UN APERÇU QUI N'EST PAS UNE RECETTE. L'éditeur fait PULSER la pièce
  // que la souris survole : il éclaircit ses index d'un montant fixe, ce
  // qu'aucun réglage de rampe n'exprime — un facteur de luminosité garderait
  // les écarts du dégradé, là où la pulsation doit se voir d'un bloc.
  //
  // ⚠ ELLE NE DÉPEND D'AUCUNE APPARENCE : c'est l'appelant qui a déjà fusionné
  // et peint, donc la même palette vaut pour tous les corps qui portent cet
  // identifiant. Même règle qu'au-dessus — un identifiant ne se réécrit jamais.
  bool SetStylePalette(std::uint32_t style, formats::Palette palette);

  // Oublie une recette, ses palettes dérivées et les tenues qui les portaient.
  //
  // ⚠ ELLE INVALIDE LES `ActorOutfit*` DE CE STYLE, et eux seuls : à appeler
  // entre deux images, jamais au milieu d'une composition.
  void ForgetStyle(std::uint32_t style);

  // ── LES VIGNETTES D'UN CHOIX : UNE SEULE PIÈCE, EN GRAND ────────────────
  //
  // 🔴 UNE SILHOUETTE ENTIÈRE NE RÉPOND PAS À LA QUESTION POSÉE. Une case de
  // grille fait trente pixels : un personnage complet y réduit sa tête à cinq,
  // et l'on choisit alors une coiffure qu'on ne voit pas. Le client lui-même
  // ne montre que la TÊTE dans sa fenêtre de guilde, pour cette raison.
  //
  // ⚠ ELLES SE DEMANDENT PAR APPARENCE, comme une tenue, et vivent aussi
  // longtemps que la garde-robe : une grille en ouvre quelques dizaines, pas
  // des milliers.

  // LA TÊTE SEULE : la coupe de `look.hair`, dans la teinte de
  // `look.hair_palette`. Nulle si l'installation n'a pas ce sprite — et une
  // case vide est la bonne réponse : tous les numéros n'existent pas.
  [[nodiscard]] ActorOutfit* HeadOnly(const ActorLook& look);

  // LE CORPS SEUL, peint de la teinte de vêtement officielle demandée.
  //
  // 🔴 FUSION COMPRISE, et ce n'est pas un détail d'exactitude : une palette
  // officielle ne définit qu'une PARTIE des index, et les corps de troisième
  // et quatrième classe emploient massivement les autres. Montrer le fichier
  // brut donnerait des vignettes à moitié NOIRES — c'est-à-dire l'inverse de
  // ce que le joueur obtiendra s'il choisit cette teinte.
  //
  // ⚠ ELLE IGNORE `look.body_palette` ET `look.style` : c'est `palette`
  // qu'elle montre, et une vignette n'est pas ce qu'on porte.
  [[nodiscard]] ActorOutfit* BodySwatch(const ActorLook& look, std::int16_t palette);

  [[nodiscard]] const SpriteCatalog& catalog() const noexcept { return catalog_; }
  // LA TAILLE ET LES EFFETS QUE L'INSTALLATION DONNE D'OFFICE à certaines
  // créatures : voir `ro/vfs/monster_size_effects.h`. Nulle quand la table
  // manque. La taille entre déjà dans chaque tenue ; les effets, c'est à qui
  // tient le monde de les attacher.
  [[nodiscard]] const vfs::MonsterSizeEffects* monster_size_effects() const noexcept {
    return monster_size_effects_.has_value() ? &*monster_size_effects_ : nullptr;
  }
  // Les chemins qui ont manqué, dans l'ordre où ils ont manqué, chacun une fois.
  [[nodiscard]] const std::vector<std::string>& missing() const noexcept { return missing_; }

 private:
  // Un .spr et un .act, chacun par son chemin sans extension : la cape les
  // cherche à deux endroits, tout le reste au même.
  using SpritePaths = std::pair<std::string, std::string>;

  // Charge une paire .spr/.act, ou la rend du cache. nullptr si elle manque
  // ou ne se lit pas — et alors elle ne sera plus redemandée. L'absence ne
  // se note que si l'appelant le demande : un chemin essayé avant un autre
  // n'en est pas une.
  [[nodiscard]] const LoadedActorSprite* Sprite(const std::string& sprite_path,
                                                const std::string& animation_path,
                                                bool note_absence);
  [[nodiscard]] const formats::Palette* Palette(const std::string& path);
  // Une palette de style et son numéro de série : voir
  // `SpriteImageKey::palette_serial`.
  struct StyledPalette {
    formats::Palette palette;
    std::uint32_t serial = 0;
  };
  // La palette d'un corps STYLÉ, calculée une fois par apparence. Nulle quand
  // le style est inconnu — l'acteur garde alors son apparence native, ce qui
  // vaut mieux qu'un personnage qui disparaît.
  [[nodiscard]] const StyledPalette* StylePalette(const ActorLook& look,
                                                  const LoadedActorSprite& body,
                                                  const formats::Palette* official);
  [[nodiscard]] std::vector<ActorPiece> Dress(const ActorLook& look);
  // Rend à chaque arme d'un ambidextre le sprite de son objet, à la place de
  // la pièce unique du couple. Voir DualWieldSpritePaths.
  void SplitDualWield(const ActorLook& look, std::vector<ActorPiece>& pieces);

  vfs::Vfs& vfs_;
  SpriteCatalog catalog_;
  std::optional<vfs::MonsterSizeEffects> monster_size_effects_;
  // Des std::map : leurs nœuds ne bougent pas, et les pièces pointent dedans.
  std::map<SpritePaths, LoadedActorSprite> sprites_;
  std::map<std::string, formats::Palette> palettes_;
  std::map<ActorLook, ActorOutfit, ActorLookOrder> outfits_;
  std::set<SpritePaths> failed_sprites_;
  std::set<std::string> failed_palettes_;
  // Les recettes connues, et les palettes qu'elles ont produites. ⚠ DES
  // `std::map` COMME LES AUTRES : leurs nœuds ne bougent pas, et les pièces
  // pointent dedans.
  std::map<std::uint32_t, PaletteRecipe> styles_;
  // Les palettes toutes faites, par identifiant. ⚠ CONSULTÉES AVANT les
  // recettes : une palette posée par l'appelant n'a rien à recalculer.
  std::map<std::uint32_t, StyledPalette> raw_styles_;
  std::map<ActorLook, StyledPalette, ActorLookOrder> style_palettes_;
  // Le dernier numéro de série donné à une palette de style. Il ne fait que
  // croître : c'est ce qui le rend sûr là où l'adresse ne l'est pas.
  std::uint32_t last_palette_serial_ = 0;
  // Le dernier identifiant de style distribué — voir `NewStyleId`.
  std::uint32_t last_style_id_ = 0;
  // Les vignettes d'un choix. ⚠ À PART DES TENUES : ce ne sont pas des
  // apparences mais des ÉCHANTILLONS, et les mêler ferait qu'oublier un style
  // emporterait des vignettes qui ne portent aucun style.
  std::map<ActorLook, ActorOutfit, ActorLookOrder> heads_;
  std::map<std::pair<std::string, std::int16_t>, ActorOutfit> swatches_;
  // La fusion d'une teinte officielle sur un corps, gardée ici parce que les
  // pièces des vignettes en retiennent l'ADRESSE.
  std::map<std::pair<std::string, std::int16_t>, formats::Palette> swatch_palettes_;
  std::vector<std::string> missing_;
};

}  // namespace ro::render
