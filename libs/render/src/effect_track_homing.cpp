#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/effect_node.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// COMMENT LA TRAÎNÉE GUIDÉE LIT SES CASES dans le mouvement du sous-type 42.
// Les mêmes que celles du carré teinté, puisque c'est sa figure.

// La vitesse d'avance, en unités par tic. C'est elle que la rupture TRIPLE.
constexpr std::size_t kSlotOfSpeed = 1;
// De combien la tête change de hauteur à chaque tic — dans un sens à l'aller,
// dans l'autre au retour. Voir la note sur les deux signes, plus bas.
constexpr std::size_t kSlotOfRise = 2;
// La case que chaque saut recopie sans qu'on sache ce qu'elle sert : le
// dessinateur du carré ne la lit pas. Elle est là parce que le client l'y met.
constexpr std::size_t kSlotCopiedOnShift = 8;

// La texture, recopiée le long de la traînée.
constexpr std::size_t kMarkOfTexture = 0;
// L'ÉTAT DU VOL, et il en a trois. Voir `PhaseDuVol`.
constexpr std::size_t kMarkOfPhase = 1;
// LE GENRE DE PROJECTILE : celui qui ÉCLATE, et les autres qui se guident.
constexpr std::size_t kMarkOfKind = 2;
constexpr std::uint8_t kKindThatBursts = 1;

// LES TROIS PHASES DU VOL, telles que le client les numérote dans `mark[1]`.
//
// ⚠ CE N'EST PAS UNE ÉCHELLE MAIS TROIS CAS, et l'ordre des numéros ne dit rien
// de l'ordre des phases : on passe de zéro à un PUIS à deux, ou de zéro
// directement à deux quand le projectile éclate au lieu de se guider.
constexpr std::uint8_t kPhaseOfLaunch = 0;
constexpr std::uint8_t kPhaseOfHoming = 1;
constexpr std::uint8_t kPhaseOfFall = 2;

// CE QUE LA CASCADE COÛTE À CHAQUE SAUT, et les trois ne sont PAS égaux.
//
// ⚠ LE PREMIER SAUT COÛTE SIX FOIS LES AUTRES. La tête perd cent cinquante
// d'alpha en passant à la deuxième piste, puis vingt-cinq seulement d'une piste
// à la suivante. Un pas unique — même choisi au milieu — donnerait soit une
// traînée trop présente juste derrière la tête, soit trois crans invisibles.
constexpr std::int16_t kAlphaDropOfFirstShift = 150;
constexpr std::int16_t kAlphaDropOfLaterShifts = 25;

// ⚠ ET CHAQUE CRAN ATTEND SON TIC. La deuxième piste ne reçoit qu'au deuxième
// tic, la troisième au troisième, la quatrième au quatrième : la traînée se
// DÉPLIE au lieu de paraître entière. Les faire sauter ensemble donnerait quatre
// copies superposées au premier tic. Le rang d'une piste est donc aussi le
// dernier tic où elle ne reçoit pas encore.
//
// Et c'est le saut VERS CE RANG-LÀ qui coûte les cent cinquante.
constexpr std::size_t kTrackBehindTheHead = 1;

// TOUS LES COMBIEN LA TRAÎNÉE AVANCE D'UN CRAN : à chaque tic pour celui qui
// éclate, tous les trois pour les autres.
constexpr std::int32_t kTicksBetweenShiftsWhenBursting = 1;
constexpr std::int32_t kTicksBetweenShifts = 3;

// L'ALLUMAGE DE CELUI QUI ÉCLATE : il monte à pleine opacité en dix tics.
constexpr std::int32_t kLastTickOfFadeIn = 10;
constexpr std::int16_t kAlphaRisePerTick = 25;
constexpr std::int16_t kAlphaCeiling = 255;

// L'EXTINCTION, une fois retombé.
constexpr std::int16_t kAlphaFallPerTick = 12;

// La tête tourne sur elle-même, à rebours.
constexpr std::int16_t kSpinDegreesPerTick = -15;

// LE GUIDAGE : de combien le cap se corrige par tic, et à quel écart on le tient
// pour atteint.
constexpr std::int16_t kHomingTurnPerTick = 5;
constexpr std::int16_t kHomingTolerance = 4;

// LA RUPTURE DE CELUI QUI ÉCLATE : sa vitesse triple, et son cap reçoit un écart
// tiré au sort d'un côté ou de l'autre.
constexpr float kSpeedFactorOnBurst = 3.0F;
constexpr std::int32_t kBurstAngleSpread = 45;

[[nodiscard]] float RadiansOfDegrees(std::int32_t degrees) {
  return static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F;
}

[[nodiscard]] std::int16_t WrappedDegrees(std::int32_t degrees) {
  std::int32_t remainder = degrees % kEffectTrackFullTurnDegrees;
  if (remainder < 0) {
    remainder += kEffectTrackFullTurnDegrees;
  }
  return static_cast<std::int16_t>(remainder);
}

// LE CAP DU CLIENT d'un vecteur à nous : sa profondeur est l'opposée de la
// nôtre, et le balayage de sa table arrondit vers le haut.
[[nodiscard]] std::int16_t ClientHeadingOf(float dx, float dz) {
  return static_cast<std::int16_t>(HeadingOfVector(dx, -dz));
}

// UN SAUT DE LA CASCADE : ce que `vers` prend de `depuis`.
//
// ⚠ MOINS DE CHAMPS QUE CHEZ LE SOUS-TYPE 47, et ce n'est pas un oubli : ni
// l'azimut ni les tics ne passent, si bien que chaque cran garde le cap qu'il
// avait en recevant sa position. C'est ce qui courbe la traînée derrière un
// projectile qui vire.
void Shift(EffectTrack& vers, const EffectTrack& from, std::int16_t perte_d_alpha) {
  vers.degrees = from.degrees;
  vers.radius_at_anchor = from.radius_at_anchor;
  vers.value[kSlotCopiedOnShift] = from.value[kSlotCopiedOnShift];
  vers.alpha = std::max<std::int16_t>(static_cast<std::int16_t>(from.alpha - perte_d_alpha), 0);
  vers.anchor = from.anchor;
  vers.mark[kMarkOfTexture] = from.mark[kMarkOfTexture];
}

// LES QUATRE CASES QUE L'OBJET LANCÉ EMPLOIE EN PLUS, et le sens leur vient de
// SON dessinateur (ADR 0017), non de celui du 42.
//
// La deuxième marque y porte le NOMBRE DE PAS du vol, compté par le bâtisseur ;
// la troisième porte le GENRE, qui décide à la fois de la vitesse de rotation et
// de la hauteur de l'arc. La cinquième case de queue porte la hauteur de départ,
// à laquelle l'arc se retranche.
constexpr std::size_t kMarkOfFlightSteps = 1;
constexpr std::size_t kMarkOfThrownKind = 2;
constexpr std::size_t kTailSlotOfBaseHeight = 4;

// LES DEUX GENRES QUE LE CLIENT NOMME, et ce ne sont pas des bornes : il les
// compare à l'égalité. Le genre zéro monte peu, celui qui tourne vite ne monte
// presque pas, et tous les autres montent beaucoup.
constexpr std::uint8_t kThrownKindThatBarelyRises = 0;
constexpr std::uint8_t kThrownKindThatSpinsFast = 15;
constexpr float kArcHeightOfBarelyRising = 10.0F;
constexpr float kArcHeightOfSpinningFast = 1.0F;
constexpr float kArcHeightOfOthers = 30.0F;

// LA ROTATION DE LA TÊTE : quarante-cinq degrés par tic pour le genre qui tourne
// vite, dix pour tous les autres. Et elle va vers l'AVANT, au rebours du 42.
constexpr std::int16_t kThrownSpinWhenFast = 45;
constexpr std::int16_t kThrownSpinOtherwise = 10;

// LA CASCADE AVANCE À CHAQUE TIC IMPAIR.
constexpr std::int32_t kThrownShiftPeriod = 2;
constexpr std::int32_t kThrownShiftPhase = 1;

// ⚠ LE VOL SE JUGE FINI À 1,05 ET NON À 1 : ce cinquième en plus laisse l'objet
// DÉPASSER son but avant de s'éteindre.
constexpr float kFlightOverAt = 1.05F;
constexpr std::int16_t kThrownAlphaFallPerTick = 20;

// LE DEMI-TOUR SUR LEQUEL L'ARC SE PORTE : la fraction du vol accomplie, bornée
// à un, s'y étale — l'objet monte, culmine à mi-chemin et redescend.
constexpr float kArcHalfTurnDegrees = 180.0F;

}  // namespace

void AdvanceThrownItemTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                             glm::vec3 aim_point, std::int16_t billboard_turn_degrees,
                             std::int16_t billboard_tilt_degrees) {
  // ⚠ CES DEUX-LÀ SE POSENT MÊME QUAND LE NŒUD DORT, comme chez le 42 : le
  // client les écrit avant de regarder si sa première piste est allumée.
  // LES ANGLES DE PANNEAU, RANGÉS TELS QUELS : la troupe passe déjà le
  // complément du client (`EffectBillboardTurnDegrees`, `…TiltDegrees`) ; le
  // recalculer ici rendrait à la figure l'angle brut de la caméra.
  tracks[0].style = billboard_turn_degrees;
  tracks[1].style = billboard_tilt_degrees;

  EffectTrack& head = tracks[0];
  if (!head.active) {
    return;
  }
  ++head.ticks;

  // ⚠ LA CASCADE AVANCE À CHAQUE TIC IMPAIR, et de la dernière à la première :
  // l'inverser copierait la tête dans les quatre d'un coup. Chaque cran attend
  // son rang, si bien que la traînée se DÉPLIE.
  if (head.ticks % kThrownShiftPeriod == kThrownShiftPhase) {
    for (std::size_t rang = tracks.size() - 1; rang > 0; --rang) {
      if (head.ticks <= static_cast<std::int32_t>(rang)) {
        continue;
      }
      Shift(tracks[rang], tracks[rang - 1],
            rang == kTrackBehindTheHead ? kAlphaDropOfFirstShift : kAlphaDropOfLaterShifts);
    }
  }

  const bool spins_fast = head.mark[kMarkOfThrownKind] == kThrownKindThatSpinsFast;
  head.degrees =
      WrappedDegrees(head.degrees + (spins_fast ? kThrownSpinWhenFast : kThrownSpinOtherwise));

  // AU PREMIER TIC, LE CAP VISE, et il ne bouge plus : c'est un jet, pas une
  // poursuite. ⚠ C'EST LE CAP DU CLIENT, arrondi par le balayage de sa table :
  // le pas qui le suit l'emploie donc dans son repère, profondeur niée.
  if (head.ticks == 1) {
    head.azimuth_degrees =
        ClientHeadingOf(aim_point.x - head.anchor.x, aim_point.z - head.anchor.z);
  }

  // ⚠ LE NOMBRE DE PAS SE LIT SIGNÉ, comme le client le lit. Un bâtisseur qui
  // n'a pas su le compter laisse zéro, et la fraction part alors à l'infini :
  // le vol se juge fini au premier tic, ce qui est exactement ce qu'il fait.
  const auto step =
      static_cast<std::int32_t>(static_cast<std::int8_t>(head.mark[kMarkOfFlightSteps]));
  const float fraction = static_cast<float>(head.ticks) / static_cast<float>(step);
  if (fraction > kFlightOverAt || step == 0) {
    head.alpha =
        std::max<std::int16_t>(static_cast<std::int16_t>(head.alpha - kThrownAlphaFallPerTick), 0);
    return;
  }

  const float base = head.tail[kTailSlotOfBaseHeight] + head.value[kSlotOfRise];
  head.tail[kTailSlotOfBaseHeight] = base;

  float height = kArcHeightOfOthers;
  if (head.mark[kMarkOfThrownKind] == kThrownKindThatBarelyRises) {
    height = kArcHeightOfBarelyRising;
  } else if (spins_fast) {
    height = kArcHeightOfSpinningFast;
  }
  // ⚠ L'ARC S'AJOUTE À LA BASE : le client le retranche dans son repère, qui
  // DESCEND, et le signe s'inverse dans le nôtre. Le retrancher ici faisait
  // creuser l'objet dans le sol au lieu de le lancer.
  const float avancement = std::min(1.0F, fraction);
  head.anchor.y = base + height * std::sin(RadiansOfDegrees(
                                      static_cast<std::int32_t>(avancement * kArcHalfTurnDegrees)));

  // LE PAS SUIT LE CAP DU CLIENT, et sa profondeur revient à la nôtre par `S`.
  const float speed = head.value[kSlotOfSpeed];
  head.anchor.x += speed * CosOfDegrees(head.azimuth_degrees);
  head.anchor.z -= speed * SinOfDegrees(head.azimuth_degrees);
}

void AdvanceHomingTrailTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              glm::vec3 owner_position, glm::vec3 aim_point,
                              std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees, std::uint32_t& generator) {
  // ⚠ CES DEUX-LÀ SE POSENT MÊME QUAND LE NŒUD DORT, comme chez le sous-type 47 :
  // le client les écrit avant de regarder si sa première piste est allumée.
  // LES ANGLES DE PANNEAU, RANGÉS TELS QUELS : la troupe passe déjà le
  // complément du client (`EffectBillboardTurnDegrees`, `…TiltDegrees`) ; le
  // recalculer ici rendrait à la figure l'angle brut de la caméra.
  tracks[0].style = billboard_turn_degrees;
  tracks[1].style = billboard_tilt_degrees;

  EffectTrack& head = tracks[0];
  if (!head.active) {
    return;
  }
  ++head.ticks;
  // Une piste retardée ne bouge pas tant que son compte n'est pas remonté à un.
  if (head.ticks <= 0) {
    return;
  }

  const bool eclate = head.mark[kMarkOfKind] == kKindThatBursts;
  if (eclate && head.ticks <= kLastTickOfFadeIn) {
    head.alpha = std::min<std::int16_t>(static_cast<std::int16_t>(head.alpha + kAlphaRisePerTick),
                                        kAlphaCeiling);
  }

  const std::int32_t ticks_between_jumps =
      eclate ? kTicksBetweenShiftsWhenBursting : kTicksBetweenShifts;
  if (head.ticks % ticks_between_jumps == 0) {
    // DE LA DERNIÈRE À LA PREMIÈRE : l'inverser copierait la tête dans les
    // quatre d'un coup, et la traînée deviendrait quatre copies superposées.
    for (std::size_t rang = tracks.size() - 1; rang > 0; --rang) {
      if (head.ticks <= static_cast<std::int32_t>(rang)) {
        continue;
      }
      Shift(tracks[rang], tracks[rang - 1],
            rang == kTrackBehindTheHead ? kAlphaDropOfFirstShift : kAlphaDropOfLaterShifts);
    }
  }

  head.degrees = WrappedDegrees(head.degrees + kSpinDegreesPerTick);

  // AU PREMIER TIC, LE CAP VISE. Le client le prend de l'ancre vers ce que le
  // nœud a accumulé de translation : c'est le point que le bâtisseur lui donne
  // pour cible.
  //
  // ⚠ L'AZIMUT DE CE MOUVEMENT EST CELUI DU CLIENT, et il le reste : son virage
  // AJOUTE cinq degrés, sa rupture retranche sur un impair, et sa comparaison
  // brute bute sur le zéro du tour. Seul le pas, en fin de tic, le passe dans
  // notre repère. Un cap à nous les retournerait tous trois.
  if (head.ticks == 1) {
    head.azimuth_degrees =
        ClientHeadingOf(aim_point.x - head.anchor.x, aim_point.z - head.anchor.z);
  }

  // ⚠ LES DEUX SENS DE LA HAUTEUR SE RETOURNENT, et PAS DANS LE MÊME SENS. Le
  // client AJOUTE sa montée à l'aller et la RETRANCHE une fois retombé, dans un
  // repère qui DESCEND : chez nous l'aller retranche et le retour ajoute. Ne
  // retourner qu'un des deux donnerait un projectile qui part bien et remonte à
  // l'arrivée.
  switch (head.mark[kMarkOfPhase]) {
    case kPhaseOfHoming: {
      const std::int16_t target =
          ClientHeadingOf(owner_position.x - head.anchor.x, owner_position.z - head.anchor.z);
      // ⚠ LE CLIENT NE REPLIE PAS LE TOUR ICI, et on ne le replie pas non plus.
      // Il compare deux angles bruts : un cap de cinq degrés et une cible de
      // trois cent cinquante-huit lui paraissent donc LOIN l'un de l'autre,
      // alors qu'ils sont voisins. Le projectile fait un tour de plus avant de
      // se juger arrivé — c'est visible, c'est mesuré, et « corriger » cela
      // donnerait une trajectoire qui n'est pas la sienne.
      const bool arrive = head.azimuth_degrees >= target - kHomingTolerance &&
                          head.azimuth_degrees <= target + kHomingTolerance;
      if (arrive) {
        head.mark[kMarkOfPhase] = kPhaseOfFall;
      } else {
        head.azimuth_degrees = WrappedDegrees(head.azimuth_degrees + kHomingTurnPerTick);
      }
      break;
    }
    case kPhaseOfFall:
      head.alpha =
          std::max<std::int16_t>(static_cast<std::int16_t>(head.alpha - kAlphaFallPerTick), 0);
      head.anchor.y += head.value[kSlotOfRise];
      break;
    default: {
      // LE LANCEMENT dure tant que la distance parcourue n'a pas passé la clé.
      const float travelled = static_cast<float>(head.ticks) * head.value[kSlotOfSpeed];
      if (travelled > head.lift_key) {
        if (eclate) {
          head.mark[kMarkOfPhase] = kPhaseOfFall;
          head.value[kSlotOfSpeed] *= kSpeedFactorOnBurst;
          // ⚠ L'ÉCART SE TIRE D'UN CÔTÉ OU DE L'AUTRE, et le côté se tire aussi.
          // Un écart toujours du même signe ferait éclater toute une volée dans
          // la même direction, ce qui se voit d'un coup d'œil. Le côté se tire
          // AVANT l'écart, et un impair le retranche.
          const bool subtracts_offset = (NextEffectNodeDraw(generator) & 1U) != 0U;
          const std::int32_t offset =
              static_cast<std::int32_t>(NextEffectNodeDraw(generator) % kBurstAngleSpread);
          head.azimuth_degrees =
              WrappedDegrees(head.azimuth_degrees + (subtracts_offset ? -offset : offset));
        } else {
          head.mark[kMarkOfPhase] = kPhaseOfHoming;
        }
      }
      head.anchor.y -= head.value[kSlotOfRise];
      break;
    }
  }

  // LE PAS SUIT LE CAP DU CLIENT : sa profondeur se nie chez nous.
  const float speed = head.value[kSlotOfSpeed];
  const float cap = RadiansOfDegrees(head.azimuth_degrees);
  head.anchor.x += speed * std::cos(cap);
  head.anchor.z -= speed * std::sin(cap);
}

}  // namespace ro::render
