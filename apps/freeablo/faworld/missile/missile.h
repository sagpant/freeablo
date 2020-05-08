#pragma once
#include "missileenums.h"
#include "missilegraphic.h"
#include <faworld/actorstats.h>
#include <functional>
#include <misc/misc.h>
#include <vector>

namespace FASaveGame
{
    class GameLoader;
    class GameSaver;
}

namespace FAWorld
{
    class Actor;
}

namespace FAWorld::Missile
{
    class Missile
    {
    public:
        virtual ~Missile() = default;

        Missile(MissileId missileId, Actor& creator, Misc::Point dest);
        Missile(FASaveGame::GameLoader& loader);

        virtual void save(FASaveGame::GameSaver& saver) const;
        virtual void update();
        virtual bool isComplete() const { return mComplete; }
        MissileId getMissileId() const { return mMissileId; }
        const std::vector<std::unique_ptr<MissileGraphic>>& getGraphics() const { return mGraphics; }

        static constexpr bool DEBUG_MISSILES = true;

    protected:
        // Static inner classes for missile attribute composition.
        class Creation
        {
        public:
            Creation() = delete;
            typedef std::function<void(Missile& missile, Misc::Point dest, GameLevel* level)> Method;

            static void singleFrame16Direction(Missile& missile, Misc::Point dest, GameLevel* level);
            static void animated16Direction(Missile& missile, Misc::Point dest, GameLevel* level);
            static void firewall(Missile& missile, Misc::Point dest, GameLevel* level);
            static void basicAnimated(Missile& missile, Misc::Point dest, GameLevel* level);
            static void townPortal(Missile& missile, Misc::Point dest, GameLevel* level);
        };

        class Movement
        {
        public:
            Movement() = delete;
            typedef std::function<void(Missile& missile, MissileGraphic& graphic)> Method;

            static void stationary(Missile& missile, MissileGraphic& graphic);
            static Method linear(FixedPoint speed, FixedPoint maxRange);
            static void hoverOverCreator(Missile& missile, MissileGraphic& graphic);

        private:
            static void linear(Missile& missile, MissileGraphic& graphic, FixedPoint speed, FixedPoint maxRange);
        };

        class ActorEngagement
        {
        public:
            ActorEngagement() = delete;
            typedef std::function<void(Missile& missile, MissileGraphic& graphic, Actor& actor)> Method;

            static void none(Missile& missile, MissileGraphic& graphic, Actor& actor);
            static void damageEnemy(Missile& missile, MissileGraphic&, Actor& actor, int32_t damage);
            static void damageEnemyAndStop(Missile& missile, MissileGraphic& graphic, Actor& actor);
            static void arrowEngagement(Missile& missile, MissileGraphic& graphic, Actor& actor);
            static void townPortal(Missile& missile, MissileGraphic& graphic, Actor& actor);
        };

        // Inner class that holds reference to all missile attributes.
        class Attributes
        {
        public:
            Attributes(Creation::Method creation, Movement::Method movement, ActorEngagement::Method actorEngagement, Tick timeToLive);
            static Attributes fromId(MissileId missileId);

            const Missile::Creation::Method mCreation;
            const Missile::Movement::Method mMovement;
            const Missile::ActorEngagement::Method mActorEngagement;
            const Tick mTimeToLive;
        };

        const DiabloExe::MissileData& missileData() const;
        const FARender::SpriteLoader::SpriteDefinition& getGraphic(int32_t i) const;
        void playImpactSound();

        Actor* mCreator;
        MissileId mMissileId;
        Misc::Point mSrcPoint;
        Missile::Attributes mAttr;
        std::vector<std::unique_ptr<MissileGraphic>> mGraphics;
        bool mComplete = false;

        // These fields are stored at missile creation, to make sure your damage and to-hit are calculated
        // based on your gear / stats when you fired the arrow, not when it hits.
        ToHitChance mToHitRanged;
        IntRange mToHitMinMaxCap;
        int32_t mRangedDamage = 0;
        IntRange mRangedDamageBonusRange;
    };
}
