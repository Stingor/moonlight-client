#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/palette.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_layers.h"
#include "ro/render/sprite_frame.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// À quelle pièce déjà posée une pièce rapportée s'accroche.
//
// Le jeu donne aux parties 3 à 6 — les coiffes — la TÊTE pour référence
// d'ancrage, tandis que la tête, elle, s'accroche au corps. Une coiffe
// accrochée au corps se retrouverait décalée de tout le décalage tête-corps.
enum class AttachTo : std::uint8_t { kBody, kHead };

// Une pièce d'un acteur : son sprite, son animation, et où elle s'accroche.
//
// Les pointeurs ne sont pas possédés : le composeur ne fait que lire, et
// l'appelant garde la main sur le cycle de vie des ressources.
struct ActorPiece {
  const formats::SprSprite* sprite = nullptr;
  const formats::ActAnimation* animation = nullptr;
  // Palette de remplacement. Vide signifie « celle du .spr ». C'est par elle
  // que RO teinte cheveux et vêtements, le sprite ne portant qu'une couleur
  // par défaut.
  const formats::Palette* palette = nullptr;
  // Le numéro de série de cette palette quand elle peut être DÉTRUITE puis
  // remplacée à la même adresse — celle d'un style —, zéro sinon. Voir
  // `SpriteImageKey::palette_serial`.
  std::uint32_t palette_serial = 0;
  // Quelle partie de l'acteur cette pièce représente. C'est elle qui décide de
  // son rang de dessin, lequel dépend de la direction.
  ActorPart part = ActorPart::kBody;
  AttachTo attach = AttachTo::kBody;
  // Cette pièce EST la tête, celle à laquelle les pièces marquées kHead
  // s'accrocheront. Explicite plutôt que déduit de la position dans la liste :
  // la cape aussi s'accroche au corps, et la deviner mènerait à accrocher les
  // chapeaux à la cape.
  bool is_head = false;
  // Une arme d'ambidextre dessinée sous le sprite de son OBJET. Ce sprite est
  // fait pour une arme tenue seule : son attaque 3, celle que joue le corps à
  // deux armes, est vide. La pièce joue alors l'attaque 2, dans la même
  // direction.
  bool single_weapon_sprite = false;
};

// L'identité d'une image de sprite, stable d'un acteur à l'autre.
//
// Deux personnages qui portent la même tête à la même image partagent la même
// texture, et c'est cette clé qui le dit. Les pointeurs identifient les
// ressources, dont l'appelant garantit la durée de vie — les mêmes que ceux
// d'ActorPiece.
struct SpriteImageKey {
  const formats::SprSprite* sprite = nullptr;
  const formats::Palette* palette = nullptr;
  std::int32_t section = 0;
  std::int32_t image_index = -1;
  // 🔴 L'ADRESSE D'UNE PALETTE NE SUFFIT PAS À L'IDENTIFIER. Celle d'un style
  // se détruit quand le style s'oublie, et la suivante peut naître à la même
  // adresse : le cache de textures servirait alors l'image recolorée de la
  // PRÉCÉDENTE. Le numéro de série, jamais réutilisé, les sépare. Zéro pour
  // une palette qui vit aussi longtemps que la garde-robe.
  std::uint32_t palette_serial = 0;

  [[nodiscard]] friend bool operator==(const SpriteImageKey& left,
                                       const SpriteImageKey& right) noexcept {
    return left.sprite == right.sprite && left.palette == right.palette &&
           left.section == right.section && left.image_index == right.image_index &&
           left.palette_serial == right.palette_serial;
  }

  // Ordre total pour servir de clé de cache. std::less sur des pointeurs est
  // le seul ordre défini par la norme ; l'opérateur < brut ne l'est pas entre
  // objets sans lien.
  [[nodiscard]] friend bool operator<(const SpriteImageKey& left,
                                      const SpriteImageKey& right) noexcept {
    const std::less<const void*> before;
    if (left.sprite != right.sprite) {
      return before(left.sprite, right.sprite);
    }
    if (left.palette != right.palette) {
      return before(left.palette, right.palette);
    }
    if (left.section != right.section) {
      return left.section < right.section;
    }
    if (left.image_index != right.image_index) {
      return left.image_index < right.image_index;
    }
    return left.palette_serial < right.palette_serial;
  }
};

// Un acteur composé, prêt à dessiner.
// Vrai si cette clé désigne une image qui EXISTE.
//
// Un calque peut en désigner une qui n'existe pas : une pièce absente de
// l'installation, une image palettisée sans palette, ou — le cas le plus
// courant — une image de largeur ou de hauteur NULLE, dont les fichiers du jeu
// sont pleins. Le renderer saute ces calques-là ; ils ne sont jamais peints.
//
// ⚠ CE QUI N'EST PAS PEINT NE DOIT PAS COMPTER dans ce que l'acteur occupe.
// Un calque vide porte quand même le décalage de son .act : le laisser entrer
// dans l'emprise l'étire jusque-là, sur du vide, et la boîte de clic qui en
// découle vole au sol des cases où l'on voudrait marcher.
//
// Elle répond exactement comme ResolveImage : vrai si et seulement si celle-ci
// rend une image non vide.
[[nodiscard]] bool HasImage(const SpriteImageKey& key);

struct ComposedActor {
  std::vector<SpriteQuad> quads;
  // La provenance de chaque quad, dans le même ordre. Ce ne sont PAS des
  // pixels : les décoder à chaque image d'animation interdirait tout partage
  // entre acteurs. Qui a besoin des pixels appelle ResolveImages.
  std::vector<SpriteImageKey> sources;
  // Le fichier son que cette image DÉCLENCHE, ou vide.
  //
  // Un .act porte une liste de sons et, par image, l'indice de celui qu'elle
  // fait entendre. C'est de là que viennent le bruit d'une épée, le cri d'un
  // monstre, le pas d'un cheval — le jeu ne les déclenche pas depuis son code,
  // il les lit dans l'animation.
  //
  // La vue pointe dans le .act du CORPS : elle vit aussi longtemps que le
  // sprite, et pas davantage.
  std::string_view sound;
  // L'action et l'image d'où vient ce son. Deux coups successifs jouent le même
  // fichier ; sans ce repère, on ne saurait pas qu'il y en a eu deux.
  std::size_t action_index = 0;
  std::size_t frame_index = 0;
  // LA PARTIE DE CHAQUE QUAD, dans le même ordre que `quads`. Un effet peut
  // changer le mélange d'un calque et pas des autres — l'arme seule, ou tout
  // sauf elle. Plus courte que `quads` pour ce qui n'est pas un acteur composé
  // de pièces : un quad sans partie compte alors pour le corps.
  std::vector<ActorPart> parts;
  // LA PLACE DE LA TÊTE à cette image, en pixels de sprite autour des pieds :
  // voir `HeadPlace`. Vide sans tête.
  std::optional<glm::vec2> head_place;
};

// LE CALQUE QUI EST LA TÊTE dans une image du `.act` de tête : le premier est un
// calque vide, le second porte l'image.
inline constexpr std::size_t kHeadLayerRank = 1;

// OÙ LA TÊTE SE POSE : le COIN de son calque de tête — le fichier en donne le
// centre, le client en retranche la demi-image —, plus ce que son point
// d'attache la déplace pour rejoindre celui du corps. C'est ce que le client
// retient pour les effets de chapeau qui suivent la tête, en comparant la place
// de chaque image à celle de la première image de l'attente.
//
// Vide quand l'image n'a pas ce calque, ou que ce calque est vide : le client
// ne la pose alors pas.
[[nodiscard]] std::optional<glm::vec2> HeadPlace(const formats::ActAnimation& body,
                                                 std::size_t body_action, std::size_t body_frame,
                                                 const formats::SprSprite& head_sprite,
                                                 const formats::ActAnimation& head,
                                                 std::size_t head_action, std::size_t head_frame);

// La partie du quad de ce rang : voir `ComposedActor::parts`.
[[nodiscard]] ActorPart PartOfQuad(const ComposedActor& composed, std::size_t index) noexcept;

// UN QUAD À PEINDRE : son rang dans `ComposedActor::quads`, et s'il s'ajoute au
// décor au lieu de le recouvrir.
struct LayerDraw {
  std::size_t quad = 0;
  bool additive = false;
  // La partie dont le quad est un calque.
  ActorPart part = ActorPart::kBody;

  [[nodiscard]] friend bool operator==(const LayerDraw&, const LayerDraw&) = default;
};

// CE QUE L'ON PEINT D'UN ACTEUR COMPOSÉ, dans l'ordre de ses calques, écrit dans
// `out`.
//
// Un calque hors de `shown_parts` ne se peint pas ; un calque de
// `additive_parts` s'ajoute. Et quand `doubles_weapon` est vrai, chaque quad de
// l'ARME se peint une seconde fois, en addition, juste après lui : le client
// soumet l'arme deux fois, la copie en addition, une image sur deux.
void LayerDrawsOf(const ComposedActor& composed, ActorPartSet shown_parts,
                  ActorPartSet additive_parts, bool doubles_weapon, std::vector<LayerDraw>& out);

// Compose un acteur : le corps et ses pièces rapportées.
//
// ⚠ L'ordre de la liste porte les DÉPENDANCES D'ANCRAGE, pas l'ordre de
// dessin : une coiffe doit venir après la tête, dont elle reprend le décalage.
// L'ordre de DESSIN, lui, est recalculé d'après la direction — voir
// actor_layers.h. Les deux sont volontairement séparés : les confondre ferait
// décrocher les coiffes dès que le personnage se retourne.
//
// La direction se déduit de `action_index` : les actions sont rangées par
// groupes de huit, une par direction. La passer en plus serait un second point
// de vérité, donc une occasion de désaccord.
//
// garment_behind met la cape DERRIÈRE tout le reste — ce que les tables du
// client disent pour certaines images, voir GarmentPlacement. Sans table, il la
// dessine devant, et c'est le défaut.
//
// `attachment_frames` donne une image PROPRE à chaque pièce accrochée, dans
// l'ordre où elles sont données. Vide — le cas ordinaire — signifie que toutes
// suivent l'image de l'acteur. Voir AlternateFrame : c'est ainsi qu'une coiffe
// animée vit sur un personnage immobile.
[[nodiscard]] ComposedActor ComposeActor(const ActorPiece& body,
                                         std::span<const ActorPiece> attachments,
                                         std::size_t action_index, std::size_t frame_index,
                                         bool garment_behind = false,
                                         std::span<const std::size_t> attachment_frames = {});

// Vrai pour une pièce dont l'animation peut se JOUER seule : les trois coiffes
// et la cape. Ce sont les quatre parties que le client fait passer par la règle
// ci-dessous.
[[nodiscard]] bool HasOwnAnimation(ActorPart part) noexcept;

// L'image ALTERNATIVE d'une pièce, ou rien si la règle ne s'applique pas.
//
// Une coiffe dont l'action compte un MULTIPLE EXACT des images de sa référence
// — la TÊTE, et non le corps — ne suit pas l'acteur image pour image. Chacune
// des images de la tête lui ouvre un sous-groupe, qu'elle parcourt au fil du
// temps réel et à SA propre cadence :
//
//   image = image de l'acteur × multiple + (tics écoulés / cadence) % multiple
//
// où les tics valent vingt-quatre millisecondes et où une cadence déclarée
// nulle vaut quatre. C'est ce qui fait vivre un chapeau animé sur un personnage
// qui ne bouge pas ; sans cette règle il reste figé sur la première image de
// son sous-groupe.
[[nodiscard]] std::optional<std::size_t> AlternateFrame(const formats::ActAnimation& reference,
                                                        const formats::ActAnimation& piece,
                                                        std::size_t action_index,
                                                        std::size_t frame_index, float elapsed_ms);

// Une paire .spr / .act chargée.
//
// Elle existe pour porter la DURÉE DE VIE des ressources : ActorPiece ne fait
// que pointer, et laisser l'appelant improviser ce stockage l'exposerait à des
// pointeurs pendants dès qu'il déplace son conteneur.
struct LoadedActorSprite {
  formats::SprSprite sprite;
  formats::ActAnimation animation;
};

// Une pièce demandée : son rôle, et où la lire.
//
// Le rôle est DÉCLARÉ plutôt que déduit du rang dans la liste. Il fallait bien
// le devenir : un acteur porte jusqu'à neuf parties, et rien dans un chemin ne
// dit si c'est une cape ou un bouclier.
struct ActorPieceRequest {
  ActorPart part = ActorPart::kBody;
  // Chemin SANS extension : le .spr et le .act portent le même nom, et les
  // séparer inviterait à en désaccorder un.
  std::string base_path;
};

// Lit une demande écrite « rôle=chemin ».
//
// Le vocabulaire est commun à tous les outils du dépôt : deux programmes qui
// nommeraient différemment les mêmes parties finiraient par en composer deux
// acteurs différents. Rôles acceptés : corps, tete, coiffe, cape, arme,
// trainee, bouclier — les formes accentuées « tête » et « traînée » aussi.
//
// Les coiffes se rangent dans l'ordre où elles arrivent : basse, milieu, haut.
// C'est pourquoi la fonction reçoit le nombre déjà lu.
[[nodiscard]] std::optional<ActorPieceRequest> ParseActorPieceRequest(
    std::string_view text, std::size_t headgears_already_read);

// Lit une liste de « rôle=chemin », et la met dans l'ordre des DÉPENDANCES.
//
// Le corps vient d'abord — c'est lui que ComposeActor reçoit à part, et c'est
// sur ses ancres que tout se pose — puis la tête, dont les coiffes reprennent
// le décalage. L'ordre de DESSIN, lui, est recalculé d'après la direction.
//
// Un rôle inconnu ÉCHOUE au lieu d'être ignoré : une faute de frappe ferait
// sinon disparaître une pièce sans un mot, et une pièce absente ne se remarque
// pas sur une silhouette qu'on ne connaît pas par cœur.
[[nodiscard]] core::Result<std::vector<ActorPieceRequest>> ParseActorRequests(
    std::span<const std::string_view> arguments);

// Charge les pièces demandées.
//
// ⚠ Échoue EN BLOC : une seule pièce illisible fait échouer tout l'appel.
//
// Cela convient à des pièces nommées une par une, où le silence serait pire : un
// chemin fautif doit se dire, pas se traduire par un bras manquant sur une
// silhouette qu'on ne connaît pas par cœur.
//
// Cela ne conviendra PAS à un chargement de masse. Le jour où les sprites
// viendront d'une table plutôt que d'un argument, une pièce absente ou abîmée
// ne devra emporter que son porteur, comme le chargement de carte écarte déjà
// un modèle illisible sans renoncer à la carte.
[[nodiscard]] core::Result<std::vector<LoadedActorSprite>> LoadActorSprites(
    vfs::Vfs& vfs, std::span<const ActorPieceRequest> requests);

// Décrit une pièce chargée : son rôle, et où elle s'accroche.
//
// Seules les coiffes s'accrochent à la TÊTE — le jeu leur donne la partie 2
// pour référence ; tout le reste, cape et armes comprises, s'accroche au
// corps. La palette est facultative : nullptr laisse celle du .spr.
[[nodiscard]] ActorPiece DescribeActorPiece(const LoadedActorSprite& loaded, ActorPart part,
                                            const formats::Palette* palette);

// Décrit les pièces chargées : rang de dessin et point d'accrochage.
//
// Une seule description pour tous les programmes : un acteur décrit de deux
// façons différentes finit par se composer de deux façons différentes.
//
// ⚠ L'ordre rendu est celui des DÉPENDANCES d'ancrage — la tête d'abord, puis
// ce qui s'y accroche — et non l'ordre de dessin, que ComposeActor recalcule.
[[nodiscard]] std::vector<ActorPiece> DescribeActorPieces(
    std::span<LoadedActorSprite> loaded, std::span<const ActorPieceRequest> requests);

// L'ALPHA D'UNE IMAGE BGRA DE `.SPR`, RAMENÉ AUX QUATRE BITS DU CLIENT.
//
// 🔴 LE CLIENT NE GARDE QU'UN QUARTET. Il charge la section BGRA dans une
// texture `D3DFMT_A4R4G4B4` et n'en écrit que le quartet de poids fort de
// chaque canal. Un alpha sous SEIZE y vaut donc zéro.
//
// CE QUE ÇA CORRIGE. `data\sprite\이팩트\particle1.spr` — la gerbe d'impact,
// les étincelles d'un warp — porte sur ses 256 texels de bord un noir à alpha
// QUINZE, 238 fois. Gardé sur huit bits et mélangé source par son alpha, ce
// cadre d'un texel assombrit le sol de trente niveaux sur une rangée d'écran,
// et `GL_CLAMP_TO_EDGE` l'étire encore d'un demi-texel : c'est le rectangle gris
// net que le joueur voit autour de chaque particule. Cinquante-quatre des 386
// images BGRA nommées par les patrons de nœuds sont dans ce cas.
//
// ⚠ LA COULEUR N'EST PAS QUANTIFIÉE. Les quatre bits par canal du client sont
// une contrainte de sa carte graphique, pas une règle : les porter ne
// corrigerait rien et coûterait les dégradés. Seul l'alpha décide de ce qui se
// voit.
//
// ⚠ ET CELA NE VIT PAS DANS `formats`. Un décodeur doit rendre le FICHIER,
// comparable à un export de GRFEditor ; c'est le téléversement qui imite la
// carte graphique du client, comme `effect_texture_touchup` pour les `.tga`.
[[nodiscard]] formats::Image WithClientAlphaQuartet(formats::Image image);

// L'image que désigne une clé, palette appliquée.
//
// La palette n'entre en jeu qu'ICI : les images palettisées gardent leurs index
// jusqu'au dernier moment, ce qui permet de recolorer sans relire le fichier.
//
// ⚠ ET L'ALPHA D'UNE IMAGE BGRA S'Y RAMÈNE À QUATRE BITS : voir
// `WithClientAlphaQuartet`.
[[nodiscard]] formats::Image ResolveImage(const SpriteImageKey& key);

// Toutes les images d'un acteur composé, dans l'ordre de ses quads.
//
// Pour les consommateurs qui dessinent sans carte graphique. Un renderer, lui,
// résout par la clé et ne redécode que ce qu'il n'a pas déjà.
[[nodiscard]] std::vector<formats::Image> ResolveImages(const ComposedActor& actor);

// L'INDEX DE PALETTE SOUS UN POINT, en unités de l'acteur composé — l'origine
// étant celle du `.act`, c'est-à-dire les PIEDS.
//
// 🔴 UN INDEX, ET SURTOUT PAS UNE COULEUR. C'est ce qui permet de répondre
// « quelle pièce du costume est-ce là ? » sans jamais rien comparer : deux
// pièces peuvent porter la même teinte après réglage, et une comparaison de
// couleurs les confondrait. L'index, lui, est exact par construction — le
// parseur le conserve précisément pour cela.
//
// ⚠ DU DESSUS VERS LE DESSOUS, comme à l'écran : le premier calque OPAQUE
// répond. Un pixel transparent laisse passer la question au calque suivant,
// sans quoi une manche translucide masquerait le torse.
//
// ⚠ LES CALQUES BGRA SONT SAUTÉS : ils portent leurs couleurs par pixel et
// n'ont AUCUN index — il n'y a rien à désigner dessus.
//
// Rien hors de l'acteur, sur un pixel transparent, ou sur un calque sans
// index.
[[nodiscard]] std::optional<std::uint8_t> PaletteIndexAt(const ComposedActor& actor, glm::vec2 at);

// ⚠ ÉCRASER TOUTES LES COUCHES EN UNE IMAGE se demande à `sprite_vignette.h`,
// qui sait AUSSI la cadrer. Une couche seule ne veut rien dire — une émoticone
// du jeu en superpose plusieurs, dont la première est la bulle VIDE — et un
// écrasement sans cadrage ne veut rien dire non plus : l'échelle prise sur la
// pose affichée change dès qu'on tourne le personnage.

}  // namespace ro::render
