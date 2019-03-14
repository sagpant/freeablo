#include "player.h"
#include "../engine/enginemain.h"
#include "../engine/threadmanager.h"
#include "../fagui/dialogmanager.h"
#include "../fagui/guimanager.h"
#include "../fasavegame/gameloader.h"
#include "actorstats.h"
#include "boost/algorithm/clamp.hpp"
#include "diabloexe/characterstats.h"
#include "equiptarget.h"
#include "itembonus.h"
#include "itemenums.h"
#include "itemmap.h"
#include "missile/missile.h"
#include "playerbehaviour.h"
#include "world.h"
#include <misc/assert.h>
#include <misc/stringops.h>
#include <misc/vec2fix.h>
#include <random/random.h>
#include <string>

namespace FAWorld
{
    const std::string Player::typeId = "player";

    const char* toString(PlayerClass value)
    {
        switch (value)
        {
            case PlayerClass::warrior:
                return "warrior";
            case PlayerClass::rogue:
                return "rogue";
            case PlayerClass::sorcerer:
                return "sorceror";
        }
        return "unknown";
    }

    Player::Player(World& world) : Actor(world)
    {
        // TODO: hack - need to think of some more elegant way of handling Actors in general
        DiabloExe::CharacterStats stats;
        init(stats);
        initCommon();
    }

    Player::Player(World& world, const DiabloExe::CharacterStats& charStats) : Actor(world)
    {
        init(charStats);
        initCommon();
    }

    void Player::initCommon()
    {
        mWorld.registerPlayer(this);
        mInventory.mInventoryChanged.connect([this](EquipTargetType inventoryType, Item const& removed, Item const& added) {
            (void)removed;

            // Update player graphics.
            updateSprites();

            switch (inventoryType)
            {
                case EquipTargetType::body:
                case EquipTargetType::leftHand:
                case EquipTargetType::rightHand:
                    // Update player graphics.
                    updateSprites();
                    break;
                default:
                    break;
            }

            if (!added.isEmpty() && mPlayerInitialised && this == mWorld.getCurrentPlayer())
            {
                // Play inventory place/grab sound.
                switch (inventoryType)
                {
                    case EquipTargetType::cursor:
                        Engine::ThreadManager::get()->playSound("sfx/items/invgrab.wav");
                        break;
                    default:
                        std::string soundPath = added.getInvPlaceSoundPath();
                        Engine::ThreadManager::get()->playSound(soundPath);
                        break;
                }
            }
        });
    }

    void Player::setPlayerClass(PlayerClass playerClass)
    {
        mPlayerClass = playerClass;
        updateSprites();
    }

    int32_t Player::meleeDamageVs(const Actor* /*actor*/) const
    {
        auto bonus = getItemBonus();
        auto dmg = mWorld.mRng->randomInRange(bonus.minAttackDamage, bonus.maxAttackDamage);
        dmg += dmg * getPercentDamageBonus() / 100;
        dmg += getCharacterBaseDamage();
        dmg += getDamageBonus();
        // critical hit for warriors:
        if (mPlayerClass == PlayerClass::warrior && mWorld.mRng->randomInRange(0, 99) < getCharacterLevel())
            dmg *= 2;
        return dmg;
    }

    ItemBonus Player::getItemBonus() const { return mInventory.getTotalItemBonus(); }

    void Player::init(const DiabloExe::CharacterStats& charStats)
    {
        mPlayerStats = {charStats};
        mFaction = Faction::heaven();
        mMoveHandler = MovementHandler(World::getTicksInPeriod("0.1")); // allow players to repath much more often than other actors

        mStats.mAttackDamage = 60;

        mBehaviour.reset(new PlayerBehaviour(this));
    }

    Player::Player(World& world, FASaveGame::GameLoader& loader) : Actor(world, loader)
    {
        mPlayerClass = static_cast<PlayerClass>(loader.load<int32_t>());
        mPlayerStats = {loader};
        mActiveMissileIndex = loader.load<uint32_t>();
        initCommon();
        mPlayerInitialised = true;
    }

    void Player::save(FASaveGame::GameSaver& saver)
    {
        release_assert(mPlayerInitialised);

        Serial::ScopedCategorySaver cat("Player", saver);

        Actor::save(saver);
        saver.save(static_cast<int32_t>(mPlayerClass));
        mPlayerStats.save(saver);
        saver.save(mActiveMissileIndex);
    }

    bool Player::checkHit(Actor* enemy)
    {
        // let's throw some formulas, parameters will be placeholders for now
        auto roll = mWorld.mRng->randomInRange(0, 99);
        auto toHit = mPlayerStats.mDexterity / 2;
        toHit += getArmorPenetration();
        toHit -= enemy->getArmor();
        toHit += getCharacterLevel();
        toHit += 50;
        if (mPlayerClass == PlayerClass::warrior)
            toHit += 20;
        toHit = boost::algorithm::clamp(toHit, 5, 95);
        return roll < toHit;
    }

    Player::~Player() { mWorld.deregisterPlayer(this); }

    char getClassCode(PlayerClass playerClass)
    {
        switch (playerClass)
        {
            case PlayerClass::warrior:
                return 'w';
            case PlayerClass::rogue:
                return 'r';
            case PlayerClass::sorcerer:
                return 's';
        }

        invalid_enum(PlayerClass, playerClass);
    }

    void Player::updateSprites()
    {
        auto classCode = getClassCode(mPlayerClass);

        std::string armour = "l", weapon;
        if (!mInventory.getBody().isEmpty())
        {
            switch (mInventory.getBody().getType())
            {
                case ItemType::heavyArmor:
                {
                    armour = "h";
                    break;
                }

                case ItemType::mediumArmor:
                {
                    armour = "m";
                    break;
                }

                case ItemType::lightArmor:
                default:
                {
                    armour = "l";
                    break;
                }
            }
        }
        if (mInventory.getLeftHand().isEmpty() && mInventory.getRightHand().isEmpty())
        {
            weapon = "n";
        }
        else if ((mInventory.getLeftHand().isEmpty() && !mInventory.getRightHand().isEmpty()) ||
                 (!mInventory.getLeftHand().isEmpty() && mInventory.getRightHand().isEmpty()))
        {
            const Item* hand = nullptr;

            if (!mInventory.getLeftHand().isEmpty())
                hand = &mInventory.getLeftHand();
            else
                hand = &mInventory.getRightHand();
            switch (hand->getType())
            {
                case ItemType::axe:
                {
                    if (hand->getEquipLoc() == ItemEquipType::oneHanded)
                        weapon = "s";
                    else
                        weapon = "a";
                    break;
                }

                case ItemType::mace:
                {
                    weapon = "m";
                    break;
                }

                case ItemType::bow:
                {
                    weapon = "b";
                    break;
                }

                case ItemType::shield:
                {
                    weapon = "u";
                    break;
                }

                case ItemType::sword:
                {
                    weapon = "s";
                    break;
                }

                default:
                {
                    weapon = "n";
                    break;
                }
            }
        }

        else if (!mInventory.getLeftHand().isEmpty() && !mInventory.getRightHand().isEmpty())
        {
            if ((mInventory.getLeftHand().getType() == ItemType::sword && mInventory.getRightHand().getType() == ItemType::shield) ||
                (mInventory.getLeftHand().getType() == ItemType::shield && mInventory.getRightHand().getType() == ItemType::sword))
                weapon = "d";

            else if (mInventory.getLeftHand().getType() == ItemType::bow && mInventory.getRightHand().getType() == ItemType::bow)
                weapon = "b";
            else if (mInventory.getLeftHand().getType() == ItemType::axe && mInventory.getRightHand().getType() == ItemType::axe)
                weapon = "a";

            else if (mInventory.getLeftHand().getType() == ItemType::staff && mInventory.getRightHand().getType() == ItemType::staff)
                weapon = "t";
            else if (mInventory.getLeftHand().getType() == ItemType::mace || mInventory.getRightHand().getType() == ItemType::mace)
                weapon = "h";

            release_assert(!weapon.empty()); // Empty weapon format
        }
        auto weaponCode = weapon;
        auto armourCode = armour;

        auto helper = [&](bool isDie) {
            std::string weapFormat = weaponCode;

            if (isDie)
                weapFormat = "n";

            boost::format fmt("plrgfx/%s/%s%s%s/%s%s%s%s.cl2");
            fmt % toString(mPlayerClass) % classCode % armourCode % weapFormat % classCode % armourCode % weapFormat;
            return fmt;
        };

        auto renderer = FARender::Renderer::get();

        // TODO: Spell animations: lightning "lm", fire "fm", other "qm"
        mAnimation.setAnimation(AnimState::dead, renderer->loadImage((helper(true) % "dt").str()));
        mAnimation.setAnimation(AnimState::attack, renderer->loadImage((helper(false) % "at").str()));
        mAnimation.setAnimation(AnimState::hit, renderer->loadImage((helper(false) % "ht").str()));

        if (getLevel() && getLevel()->isTown())
        {
            mAnimation.setAnimation(AnimState::walk, renderer->loadImage((helper(false) % "wl").str()));
            mAnimation.setAnimation(AnimState::idle, renderer->loadImage((helper(false) % "st").str()));
        }
        else
        {
            mAnimation.setAnimation(AnimState::walk, renderer->loadImage((helper(false) % "aw").str()));
            mAnimation.setAnimation(AnimState::idle, renderer->loadImage((helper(false) % "as").str()));
        }
    }

    bool Player::dropItem(const Misc::Point& clickedPoint)
    {
        auto cursorItem = mInventory.getCursorHeld();
        auto initialDir = (Vec2Fix(clickedPoint.x, clickedPoint.y) - Vec2Fix(getPos().current().x, getPos().current().y)).getDirection();
        auto curPos = getPos().current();
        auto direction = (curPos == clickedPoint) ? Misc::Direction(Misc::Direction8::none) : initialDir;

        if (getLevel()->dropItemClosestEmptyTile(cursorItem, *this, curPos, direction))
        {
            mInventory.setCursorHeld({});
            return true;
        }
        return false;
    }

    bool Player::canTalkTo(Actor* actor)
    {
        if (actor == nullptr)
            return false;

        if (this == actor)
            return false;

        if (!actor->canTalk())
            return false;

        if (isEnemy(actor))
            return false;

        return true;
    }

    void Player::update(bool noclip)
    {
        Actor::update(noclip);

        // handle talking to npcs
        if (mTarget.getType() == Target::Type::Actor)
        {
            Actor* target = mTarget.get<Actor*>();

            if (target && target->getPos().isNear(this->getPos()) && canTalkTo(target))
            {
                if (mWorld.getCurrentPlayer() == this)
                {
                    auto& guiManager = Engine::EngineMain::get()->mGuiManager;
                    guiManager->closeAllPanels();
                    guiManager->mDialogManager.talk(target);
                }
                mTarget.clear();
            }
        }
    }

    void Player::enemyKilled(Actor* enemy)
    {
        if (Monster* monster = dynamic_cast<Monster*>(enemy))
        {
            addExperience(*monster);
            // TODO: intimidate close fallen demons.
            // TODO: notify quests.
            // TODO: if enemy is Diablo game complete.
        }
    }

    void Player::addExperience(Monster& enemy)
    {
        int32_t exp = enemy.getKillExp();
        // Adjust exp based on difference in level between player and monster.
        exp *= 1 + ((float)enemy.getMonsterStats().level - mPlayerStats.mLevel) / 10;
        exp = std::max(0, exp);

        mPlayerStats.mExp = std::min(mPlayerStats.mExp + exp, mPlayerStats.maxExp());
        int32_t newLevel = mPlayerStats.expToLevel(mPlayerStats.mExp);
        // Level up if applicable (it's possible to level up more than once).
        for (int32_t i = mPlayerStats.mLevel; i < newLevel; i++)
            levelUp(newLevel);
    }

    void Player::levelUp(int32_t newLevel)
    {
        mPlayerStats.mLevel = newLevel;

        // Increase HP/Mana.
        switch (mPlayerClass)
        {
            case PlayerClass::warrior:
                mStats.mHp.max += 2;
                mStats.mMana.max += 1;
                break;
            case PlayerClass::rogue:
                mStats.mHp.max += 2;
                mStats.mMana.max += 2;
                break;
            case PlayerClass::sorcerer:
                mStats.mHp.max += 1;
                mStats.mMana.max += 2;
                break;
        }

        // Restore HP/Mana.
        heal();
        restoreMana();
    }

    static const std::vector<MissileId> mImplementedMissiles = {
        MissileId::arrow, MissileId::firebolt, MissileId::firewall, MissileId::manashield, MissileId::farrow, MissileId::larrow};

    void Player::setActiveSpellNumber(int32_t spellNumber)
    {
        (void)spellNumber;
        // Hack for testing, loop through implemented missiles.
        mActiveMissileIndex++;
        if (mActiveMissileIndex >= mImplementedMissiles.size())
            mActiveMissileIndex = 0;
    }

    void Player::castActiveSpell(Misc::Point targetPoint)
    {
        // Hack for testing, loop through implemented missiles.
        auto missileId = mImplementedMissiles[mActiveMissileIndex];
        switch (missileId)
        {
            case MissileId::arrow:
            case MissileId::farrow:
            case MissileId::larrow:
                // Arrow sounds will need to be implemented like Actor::doMeleeHit().
                Engine::ThreadManager::get()->playSound("sfx/misc/bfire.wav");
                break;
            default:
                // Spell sounds will come from DiabloExe::getSpellsDataTable()[spellId].mSoundEffect.
                break;
        }
        activateMissile(missileId, targetPoint);
    }
}
