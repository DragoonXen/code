//
// Created by dragoon on 09.07.2022.
//

#ifndef AI_CUP_22_UTIL_HPP
#define AI_CUP_22_UTIL_HPP

#include <tuple>
#include <optional>
#include "model/Obstacle.hpp"
#include "model/Vec2.hpp"
#include "model/Constants.hpp"

namespace {
    using namespace model;
}

template<typename T>
T sqr(T value) { return value * value; }

inline Vec2 SegmentClosestPoint(Vec2 point, Vec2 segmentStart, Vec2 segmentEnd) {
    Vec2 v = segmentEnd - segmentStart;
    Vec2 w = point - segmentStart;

    double c1 = v.dot(w);
    if ( c1 <= 0 ) {
        return segmentStart;
    }
    double c2 =v.dot(v);
    if (c2 <= c1) {
        return segmentEnd;
    }
    double b = c1 / c2;
    return segmentStart + v * b;
}

inline double SegmentPointSqrDist(Vec2 point, Vec2 segmentStart, Vec2 segmentEnd) {
    return (SegmentClosestPoint(point, segmentStart, segmentEnd) - point).sqrNorm();
}

inline double SegmentPointDist(Vec2 point, Vec2 segmentStart, Vec2 segmentEnd) {
    return (SegmentClosestPoint(point, segmentStart, segmentEnd) - point).norm();
}

template<bool shoot_passable = false>
std::tuple<std::optional<const model::Obstacle *>, Vec2>
GetClosestCollision(Vec2 start, Vec2 finish, const double addRadius, std::optional<int> ignoreId = std::nullopt) {
    const auto& obstacles = Constants::INSTANCE.Get(start);

    std::optional<const Obstacle *> closest_obstacle = std::nullopt;
    Vec2 point;
    double closest_distance = std::numeric_limits<double>::infinity();
    for (const auto &obstacle: obstacles) {
        if (ignoreId && obstacle->id == *ignoreId || (shoot_passable && obstacle->canShootThrough)) {
            continue;
        }
        const double radiusSqr = sqr(obstacle->radius + addRadius);
        auto closest_point = SegmentClosestPoint(obstacle->position, start, finish);
        if ((closest_point - obstacle->position).sqrNorm() > radiusSqr) {
            continue;
        }
        const double curr_distance = (closest_point - start).sqrNorm();
        if (curr_distance < closest_distance) {
            closest_distance = curr_distance;
            closest_obstacle = obstacle;
            point = closest_point;
        }
    }
    return {closest_obstacle, point};
}

inline double CalcResultAim(bool keep, double start, const std::optional<int>& weapon) {
    DRAW(
        if (keep && !weapon) {
            std::cerr << "wrong state" << std::endl;
            exit(0);
        }
    );
    if (keep) {
        return std::min(1., start + Constants::INSTANCE.weapons[*weapon].aimPerTick);
    } else {
        return std::max(0., start - Constants::INSTANCE.weapons[*weapon].aimPerTick);
    }
}

inline double AngleDiff(double first, double second) {
    double angleDiff = first - second;
    if (angleDiff > M_PI) {
        angleDiff -= M_PI * 2.;
    } else if (angleDiff <= -M_PI) {
        angleDiff += M_PI * 2.;
    }
    return angleDiff;
}

inline double IncreaseAngle(double angle, double until) {
    while (angle < until) {
        angle += M_PI * 2.;
    }
    return angle;
}

inline double AddAngle(double angle, double angleToAdd) {
    angle += angleToAdd;
    if (angle > M_PI) {
        angle -= M_PI * 2.;
    }
    return angle;
}

inline double SubstractAngle(double angle, double angleToSubstract) {
    angle -= angleToSubstract;
    if (angle <= -M_PI) {
        angle += M_PI * 2.;
    }
    return angle;
}

// check if angle between from and to counter clockwize
inline bool AngleBetween(double angle, double from, double to) {
    if (to < from) {
        to += M_PI * 2.;
    }
    if (angle < from) {
        angle += M_PI * 2.;
    }
    return angle <= to;
}

inline bool
IsCollide(Vec2 position, Vec2 velocity, Vec2 position2, Vec2 velocity2, const double time,
          const double collisionRadius) {
    velocity2 -= velocity;
    Vec2 position2End = position2 + velocity2 * time;
    return (SegmentClosestPoint(position, position2, position2End) - position).sqrNorm() < sqr(collisionRadius);
}

inline void ApplyDamage(Unit& unit, double incomingDamage, int tick) {
    if (incomingDamage - 1e-6 > unit.shield) {
        incomingDamage -= unit.shield;
        unit.shield = 0;
        unit.healthRegenerationStartTick = tick + Constants::INSTANCE.healthRegenerationDelayTicks;
    }
    unit.health -= incomingDamage;
}

struct ZoneMover {
    Zone zone;

    ZoneMover(const Zone &zone) : zone(zone) {};

    void nextTick() {
        if (zone.currentRadius <= zone.nextRadius) {
            zone.currentRadius -= Constants::INSTANCE.zoneSpeedPerTick;
            return;
        }
        size_t tickToReach = round((zone.currentRadius - zone.nextRadius) / Constants::INSTANCE.zoneSpeedPerTick);
        zone.currentCenter += (zone.nextCenter - zone.currentCenter) * (1. / tickToReach);
        zone.currentRadius -= Constants::INSTANCE.zoneSpeedPerTick;
    }

    inline bool IsTouchingZone(const Unit &unit) {
        return (unit.position - zone.currentCenter).norm() + Constants::INSTANCE.unitRadius > zone.currentRadius;
    }
};

inline bool ShootWhileSpawning(const Unit &fromUnit, const Unit &otherUnit, const double addDistance) {
    return otherUnit.remainingSpawnTime.has_value() &&
           ((otherUnit.position - fromUnit.position).norm() + addDistance) /
           Constants::INSTANCE.weapons[*fromUnit.weapon].projectileSpeed <
           *otherUnit.remainingSpawnTime;
}

inline std::pair<Vec2, Vec2> TangentialPoints(Vec2 from, Vec2 to, const double radius) {
    from = (to + from) * .5;
    const double firstRadiusSqr = (from - to).sqrNorm() + 1e-10;
    to -= from;

    const double a = -2 * to.x;
    const double b = -2 * to.y;
    const double c = sqr(to.x) + sqr(to.y) + firstRadiusSqr - sqr(radius);

    const double x0 = -a * c / (a * a + b * b);
    const double y0 = -b * c / (a * a + b * b);
    // every time there would be two intersection points
    double d = firstRadiusSqr - sqr(c) / (sqr(a) + sqr(b));
    double mult = sqrt(d / (a * a + b * b));
    return {Vec2{x0 + b * mult, y0 - a * mult} + from,
            Vec2{x0 - b * mult, y0 + a * mult} + from};
}

inline void TickRespawnTime(Unit& unit) {
    if (unit.remainingSpawnTime.has_value()) {
        *unit.remainingSpawnTime -= Constants::INSTANCE.tickTime;
        if (*unit.remainingSpawnTime < 1e-5) {
            unit.remainingSpawnTime.reset();
        }
    }
}

inline double CalculateDanger(Vec2 position, const Unit& enemyUnit) {
    double danger = 1.;
    const Vec2 distanceVec = position - enemyUnit.position;
    // angle danger
    const double angleDiff = std::abs(AngleDiff(enemyUnit.direction.toRadians(), distanceVec.toRadians()));
    // normalize from 0 to M_PI * 5 / 6
    const auto diff = std::min(std::max(angleDiff - (M_PI / 6), 0.), M_PI * 2 / 3);
    danger *= 1. - diff / (M_PI * 5 / 6);
    // distance danger
    constexpr double maxDangerDistance = 7.;
    const double distance = distanceVec.norm();
    const double dangerDistanceCheck = maxDangerDistance / std::max(distance, maxDangerDistance);
    danger *= dangerDistanceCheck;
    // weapon danger
    if (enemyUnit.weapon && enemyUnit.ammo[*enemyUnit.weapon] > 0) {
        constexpr double kWeaponDanger[] = {0.34, .6666, 1.};
        danger *= kWeaponDanger[*enemyUnit.weapon];
        if (*enemyUnit.weapon == 1 && distance < 7.) {
            danger *= 1.55;
        }
    } else {
        danger *= 1e-2;
    }
    return danger;
}

inline double EvaluateDanger(Vec2 pos, std::vector<std::vector<std::pair<int, double>>>& dangerMatrix, const model::Game& game) {
    const auto& constants = Constants::INSTANCE;
    Vec2 targetPos(constants.toI(pos.x), constants.toI(pos.y));
    const int posX = constants.toI(pos.x) - constants.minX;
    const int posY = constants.toI(pos.y) - constants.minY;

    auto& value = dangerMatrix[posX][posY];
    if (value.first == game.currentTick) {
        return value.second;
    }
    value.first = game.currentTick;

    // angle, danger, playerId
    std::vector<std::tuple<double, double, int >> unitsDanger;
    double sumDanger = 0.;
    for (auto& unit : game.units) {
        if (unit.playerId == game.myId) {
            continue;
        }
        auto positionDiff = unit.position - targetPos;
        // TODO: think about obstacles. Worth to hide behind non shootable obstacle with reasonable precision
        unitsDanger.emplace_back(positionDiff.toRadians(), CalculateDanger(targetPos, unit),
                                 unit.playerId);
        // distance of 5
        if (positionDiff.sqrNorm() < sqr(7.)) {
            sumDanger += std::get<1>(unitsDanger.back());
        }
    }
    constexpr double kMinAngleDifference = M_PI / 3.6;  // 50 degrees
    constexpr double kMinCoeff = 0.;
    constexpr double kMaxAngleDifference = M_PI * (2. / 3.);  // 120 degrees
    constexpr double kMaxCoeff = 1.;
    constexpr double kDiffPerScore = (kMaxCoeff - kMinCoeff) / (kMaxAngleDifference - kMinAngleDifference);
    for (size_t i = 0; i != unitsDanger.size(); ++i) {
        auto& [angle, danger, playerId] = unitsDanger[i];
        for (size_t j = i + 1; j != unitsDanger.size(); ++j) {
            auto& [angle2, danger2, playerId2] = unitsDanger[j];
            const double angleDiff = std::abs(AngleDiff(angle, angle2));
            if (angleDiff <= kMinAngleDifference) {
                sumDanger += kMinCoeff * std::min(danger, danger2);
            } else if (angleDiff >= kMaxAngleDifference) {
                sumDanger += kMaxCoeff * std::min(danger, danger2);
            } else {
                sumDanger += kDiffPerScore * (angleDiff - kMinAngleDifference) * std::min(danger, danger2);
            }
        }
    }
    value.second = sumDanger;
    return value.second;
}

inline double
EvaluateDangerIncludeObstacles(Vec2 pos, std::vector<std::vector<std::pair<int, double>>> &dangerMatrix,
                               const Game &game) {
    constexpr double maxDistance = 3.;
    constexpr double maxValue = 1.;
    auto danger = EvaluateDanger(pos, dangerMatrix, game);
    const auto &obstacles = Constants::INSTANCE.GetL(pos);

    for (auto obstacle: obstacles) {
        double distance = (obstacle->position - pos).norm() - obstacle->radius - Constants::INSTANCE.unitRadius;
        if (distance >= maxDistance) {
            continue;
        }
        danger += ((maxDistance - distance) / maxDistance) * maxValue;
    }
    DRAW({
             debugInterface->addRect(pos - Vec2(0.5, 0.5), {1., 1.}, debugging::Color(0., 0., 0., 0.5));
             debugInterface->addPlacedText(pos,
                                           to_string_p(danger, 4), {0.5, 0.5},
                                           0.05, debugging::Color(1., 1., 1., 0.9));
         });
    return danger;
}

#ifdef DEBUG_INFO
#define VERIFY(a, b) {if (!(a)){std::cerr << (b) << std::endl; getchar();exit(-1);}}
#else
#define VERIFY(a, b) (1==1)
#endif

#endif //AI_CUP_22_UTIL_HPP
