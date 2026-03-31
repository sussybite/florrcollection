#pragma once

#include <Shared/StaticDefinitions.hh>

#include <array>
#include <cstdint>

extern uint32_t const MAX_LEVEL;
extern uint32_t const TPS;

extern float const PETAL_DISABLE_DELAY;
extern float const PLAYER_ACCELERATION;
extern float const DEFAULT_FRICTION;
extern float const SUMMON_RETREAT_RADIUS;
extern float const DIGGER_SPAWN_CHANCE;

extern float const BASE_FLOWER_RADIUS;
extern float const BASE_PETAL_ROTATION_SPEED;
extern float const BASE_FOV;
extern float const BASE_HEALTH;
extern float const BASE_BODY_DAMAGE;

extern std::array<struct PetalData, PetalID::kNumPetals> const PETAL_DATA;
extern std::array<struct MobData, MobID::kNumMobs> const MOB_DATA;

//map extends from (0,0) to (ARENA_WIDTH,ARENA_HEIGHT)
inline std::array const MAP_DATA = std::to_array<struct ZoneDefinition>({
    {
        .left = 0,
        .top = 0,
        .right = 20000,
        .bottom = 15000,
        .density = 1,
        .drop_multiplier = 0.2,
        .spawns = {
            { MobID::kRock, 5000 },
            { MobID::kLadybug, 1000 },
            { MobID::kBee, 1000 },
            { MobID::kBabyAnt, 1000 },
            { MobID::kCentipede, 1000 },
            { MobID::kBoulder, 1000 },
            { MobID::kMassiveLadybug, 1000 },
            { MobID::kSquare, 1000 },
            { MobID::kAntHole, 1000 },
            { MobID::kBeetle, 1000 },
            { MobID::kMassiveBeetle, 1000 },
        },
        .difficulty = 3,
        .color = 0xffae61c7,
        .name = "OHIO"
    }
});

std::array const ANTHOLE_SPAWNS = std::to_array<StaticArray<MobID::T, 3>>({
    {MobID::kBabyAnt},
    {MobID::kWorkerAnt,MobID::kBabyAnt},
    {MobID::kWorkerAnt,MobID::kWorkerAnt},
    {MobID::kSoldierAnt,MobID::kWorkerAnt},
    {MobID::kBabyAnt,MobID::kWorkerAnt,MobID::kSoldierAnt},
    {MobID::kWorkerAnt,MobID::kSoldierAnt},
    {MobID::kSoldierAnt,MobID::kWorkerAnt,MobID::kWorkerAnt},
    {MobID::kSoldierAnt,MobID::kSoldierAnt},
    {MobID::kQueenAnt},
    {MobID::kSoldierAnt,MobID::kSoldierAnt},
    {MobID::kSoldierAnt,MobID::kSoldierAnt,MobID::kSoldierAnt}
});

extern std::array<StaticArray<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> const MOB_DROP_CHANCES;

extern uint32_t score_to_pass_level(uint32_t);
extern uint32_t score_to_level(uint32_t);
extern uint32_t level_to_score(uint32_t);
extern uint32_t loadout_slots_at_level(uint32_t);

extern float hp_at_level(uint32_t);