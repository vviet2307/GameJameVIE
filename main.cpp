#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;
constexpr int TARGET_FPS = 60;

constexpr float FLOOR_Y = 620.0f;
constexpr float STAGE_LEFT = 80.0f;
constexpr float STAGE_RIGHT = 1200.0f;
constexpr float GRAVITY = 1800.0f;

enum class GameState
{
    MENU,
    STAGE_ENFANT_FIGHT1,
    STAGE_ENFANT_FIGHT2,
    STAGE_ENFANT_CUTSCENE,
    STAGE_ADO_FIGHT1,
    STAGE_ADO_FIGHT2,
    STAGE_VINGTAINE_TAFF,
    STAGE_VINGTAINE_CUTSCENE_FIRED,
    STAGE_VINGTAINE_DEPRESSION,
    STAGE_VINGTAINE_FIGHT_SELF,
    ENDING_ASCENSEUR,
    GAME_COMPLETE
};

enum class FighterAnim
{
    IDLE,
    WALK,
    JUMP,
    PUNCH,
    KICK,
    BLOCK,
    HIT,
    KO
};

enum class AttackType
{
    NONE,
    PUNCH,
    KICK
};

struct AnimationClip
{
    int frameCount;
    float frameDuration;
    bool loop;
    int sheetRow;
};

struct AnimationState
{
    FighterAnim current = FighterAnim::IDLE;
    int frame = 0;
    float timer = 0.0f;
    bool finished = false;
};

struct SpriteSheetHook
{
    Texture2D texture{};
    bool loaded = false;
    int frameWidth = 0;
    int frameHeight = 0;
};

struct FighterCommand
{
    int move = 0;
    bool jump = false;
    bool punch = false;
    bool kick = false;
    bool block = false;
};

struct AttackDefinition
{
    int damage;
    float totalDuration;
    float activeStart;
    float activeEnd;
    float range;
    float height;
};

struct Fighter
{
    std::string name;
    Vector2 position{ 0.0f, FLOOR_Y };
    Vector2 velocity{ 0.0f, 0.0f };
    float width = 70.0f;
    float height = 150.0f;
    float moveSpeed = 280.0f;
    float jumpSpeed = 780.0f;
    int hp = 100;
    int maxHp = 100;
    bool facingRight = true;
    bool onGround = true;
    bool blocking = false;
    bool isPlayer = false;
    bool adultLook = false;
    bool beardLook = false;
    Color primaryColor = BLUE;
    Color secondaryColor = SKYBLUE;
    Color outlineColor = BLACK;
    AnimationState animation;
    SpriteSheetHook spriteSheet;
    AttackType currentAttack = AttackType::NONE;
    float attackElapsed = 0.0f;
    float attackCooldown = 0.0f;
    bool attackConnected = false;
    float hitTimer = 0.0f;
    float aiDecisionTimer = 0.0f;
    float aiBlockTimer = 0.0f;
    int aiMoveIntent = 0;
    Rectangle attackBox{ 0.0f, 0.0f, 0.0f, 0.0f };
};

struct FightDefinition
{
    const char* stageTitle;
    const char* stageSubtitle;
    const char* opponentName;
    Color skyColor;
    Color backdropColor;
    Color floorColor;
    Color accentColor;
    Color enemyColor;
};

struct CutsceneDefinition
{
    const char* title;
    const char* subtitle;
    Color backgroundColor;
    Color foregroundColor;
    float duration;
};

struct FightContext
{
    FightDefinition definition{};
    Fighter player{};
    Fighter enemy{};
    bool playerWon = false;
    bool enemyWon = false;
    float endTimer = 0.0f;
    float elapsed = 0.0f;
    float selfAcceptanceTimer = 0.0f;
    bool selfAcceptanceWon = false;
    bool falseVictoryTriggered = false;
    bool selfHintUnlocked = false;
};

struct GameContext
{
    GameState state = GameState::MENU;
    float stateTimer = 0.0f;
    FightContext fight{};
};

struct OptionalTexture
{
    Texture2D texture{};
    bool loaded = false;
};

struct BackgroundLibrary
{
    OptionalTexture enfantFight1;
    OptionalTexture enfantFight2;
    OptionalTexture adoFight1;
    OptionalTexture adoFight2;
    OptionalTexture vingtaineTaff;
    OptionalTexture vingtaineSelf;
};

struct HitEvent
{
    bool connected = false;
    int damage = 0;
    bool blocked = false;
};

const char* ResolveAssetPath(const char* primaryPath, const char* fallbackPath)
{
    if (FileExists(primaryPath))
    {
        return primaryPath;
    }

    if (FileExists(fallbackPath))
    {
        return fallbackPath;
    }

    return nullptr;
}

void LoadOptionalTexture(OptionalTexture& target, const char* primaryPath, const char* fallbackPath)
{
    const char* resolvedPath = ResolveAssetPath(primaryPath, fallbackPath);
    if (resolvedPath == nullptr)
    {
        target.loaded = false;
        return;
    }

    target.texture = LoadTexture(resolvedPath);
    target.loaded = true;
}

void UnloadOptionalTexture(OptionalTexture& target)
{
    if (!target.loaded)
    {
        return;
    }

    UnloadTexture(target.texture);
    target.loaded = false;
}

void LoadBackgroundLibrary(BackgroundLibrary& backgrounds)
{
    LoadOptionalTexture(backgrounds.enfantFight1, "assets/backgrounds/enfant_fight1.png", "build/assets/enfant_fight1.png");
    LoadOptionalTexture(backgrounds.enfantFight2, "assets/backgrounds/enfant_fight2.png", "build/assets/enfant_fight2.png");
    LoadOptionalTexture(backgrounds.adoFight1, "assets/backgrounds/ado_fight1.png", "build/assets/ado_fight1.png");
    LoadOptionalTexture(backgrounds.adoFight2, "assets/backgrounds/ado_fight2.png", "build/assets/ado_fight2.png");
    LoadOptionalTexture(backgrounds.vingtaineTaff, "assets/backgrounds/vingtaine_taff.png", "build/assets/vingtaine_taff.png");
    LoadOptionalTexture(backgrounds.vingtaineSelf, "assets/backgrounds/vingtaine_self.png", "build/assets/vingtaine_self.png");
}

void UnloadBackgroundLibrary(BackgroundLibrary& backgrounds)
{
    UnloadOptionalTexture(backgrounds.enfantFight1);
    UnloadOptionalTexture(backgrounds.enfantFight2);
    UnloadOptionalTexture(backgrounds.adoFight1);
    UnloadOptionalTexture(backgrounds.adoFight2);
    UnloadOptionalTexture(backgrounds.vingtaineTaff);
    UnloadOptionalTexture(backgrounds.vingtaineSelf);
}

const OptionalTexture* GetStageBackground(const BackgroundLibrary& backgrounds, GameState state)
{
    switch (state)
    {
        case GameState::STAGE_ENFANT_FIGHT1:
            return backgrounds.enfantFight1.loaded ? &backgrounds.enfantFight1 : nullptr;

        case GameState::STAGE_ENFANT_FIGHT2:
            return backgrounds.enfantFight2.loaded ? &backgrounds.enfantFight2 : nullptr;

        case GameState::STAGE_ADO_FIGHT1:
            return backgrounds.adoFight1.loaded ? &backgrounds.adoFight1 : nullptr;

        case GameState::STAGE_ADO_FIGHT2:
            return backgrounds.adoFight2.loaded ? &backgrounds.adoFight2 : nullptr;

        case GameState::STAGE_VINGTAINE_TAFF:
            return backgrounds.vingtaineTaff.loaded ? &backgrounds.vingtaineTaff : nullptr;

        case GameState::STAGE_VINGTAINE_FIGHT_SELF:
            return backgrounds.vingtaineSelf.loaded ? &backgrounds.vingtaineSelf : nullptr;

        default:
            return nullptr;
    }
}

AnimationClip GetAnimationClip(FighterAnim anim)
{
    switch (anim)
    {
        case FighterAnim::IDLE: return { 4, 0.16f, true, 0 };
        case FighterAnim::WALK: return { 6, 0.09f, true, 1 };
        case FighterAnim::JUMP: return { 2, 0.12f, false, 2 };
        case FighterAnim::PUNCH: return { 3, 0.08f, false, 3 };
        case FighterAnim::KICK: return { 4, 0.07f, false, 4 };
        case FighterAnim::BLOCK: return { 2, 0.10f, true, 5 };
        case FighterAnim::HIT: return { 2, 0.10f, false, 6 };
        case FighterAnim::KO: return { 3, 0.18f, false, 7 };
    }

    return { 1, 0.1f, true, 0 };
}

AttackDefinition GetAttackDefinition(AttackType attack)
{
    switch (attack)
    {
        case AttackType::PUNCH: return { 10, 0.28f, 0.08f, 0.16f, 46.0f, 46.0f };
        case AttackType::KICK: return { 16, 0.38f, 0.12f, 0.22f, 72.0f, 34.0f };
        case AttackType::NONE: break;
    }

    return { 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
}

bool IsFightState(GameState state)
{
    switch (state)
    {
        case GameState::STAGE_ENFANT_FIGHT1:
        case GameState::STAGE_ENFANT_FIGHT2:
        case GameState::STAGE_ADO_FIGHT1:
        case GameState::STAGE_ADO_FIGHT2:
        case GameState::STAGE_VINGTAINE_TAFF:
        case GameState::STAGE_VINGTAINE_FIGHT_SELF:
            return true;

        default:
            return false;
    }
}

bool IsVingtaineState(GameState state)
{
    switch (state)
    {
        case GameState::STAGE_VINGTAINE_TAFF:
        case GameState::STAGE_VINGTAINE_CUTSCENE_FIRED:
        case GameState::STAGE_VINGTAINE_DEPRESSION:
        case GameState::STAGE_VINGTAINE_FIGHT_SELF:
        case GameState::ENDING_ASCENSEUR:
        case GameState::GAME_COMPLETE:
            return true;

        default:
            return false;
    }
}

bool IsFinalSelfFight(GameState state)
{
    return state == GameState::STAGE_VINGTAINE_FIGHT_SELF;
}

GameState GetNextState(GameState state)
{
    switch (state)
    {
        case GameState::MENU: return GameState::STAGE_ENFANT_FIGHT1;
        case GameState::STAGE_ENFANT_FIGHT1: return GameState::STAGE_ENFANT_FIGHT2;
        case GameState::STAGE_ENFANT_FIGHT2: return GameState::STAGE_ENFANT_CUTSCENE;
        case GameState::STAGE_ENFANT_CUTSCENE: return GameState::STAGE_ADO_FIGHT1;
        case GameState::STAGE_ADO_FIGHT1: return GameState::STAGE_ADO_FIGHT2;
        case GameState::STAGE_ADO_FIGHT2: return GameState::STAGE_VINGTAINE_TAFF;
        case GameState::STAGE_VINGTAINE_TAFF: return GameState::STAGE_VINGTAINE_CUTSCENE_FIRED;
        case GameState::STAGE_VINGTAINE_CUTSCENE_FIRED: return GameState::STAGE_VINGTAINE_DEPRESSION;
        case GameState::STAGE_VINGTAINE_DEPRESSION: return GameState::STAGE_VINGTAINE_FIGHT_SELF;
        case GameState::STAGE_VINGTAINE_FIGHT_SELF: return GameState::ENDING_ASCENSEUR;
        case GameState::ENDING_ASCENSEUR: return GameState::GAME_COMPLETE;
        case GameState::GAME_COMPLETE: return GameState::MENU;
    }

    return GameState::MENU;
}

FightDefinition GetFightDefinition(GameState state)
{
    switch (state)
    {
        case GameState::STAGE_ENFANT_FIGHT1:
            return { "Enfance I", "Le salon devient une arene imaginaire", "SonGoku", { 123, 199, 255, 255 }, { 255, 232, 176, 255 }, { 166, 104, 61, 255 }, { 255, 160, 72, 255 }, { 255, 148, 69, 255 } };

        case GameState::STAGE_ENFANT_FIGHT2:
            return { "Enfance II", "Le heros du quartier surgit des murs", "SpiderMan", { 92, 153, 225, 255 }, { 230, 78, 68, 255 }, { 99, 109, 123, 255 }, { 255, 255, 255, 255 }, { 199, 40, 52, 255 } };

        case GameState::STAGE_ADO_FIGHT1:
            return { "Adolescence I", "Le couloir du college devient ring social", "Enfants", { 176, 208, 255, 255 }, { 119, 161, 230, 255 }, { 154, 120, 92, 255 }, { 249, 215, 87, 255 }, { 87, 96, 186, 255 } };

        case GameState::STAGE_ADO_FIGHT2:
            return { "Adolescence II", "Le salon familial se charge d'injonctions", "Parents", { 255, 194, 136, 255 }, { 207, 149, 103, 255 }, { 130, 84, 55, 255 }, { 255, 232, 198, 255 }, { 122, 73, 63, 255 } };

        case GameState::STAGE_VINGTAINE_TAFF:
            return { "Vingtaine", "Open-space, mails, pression et fatigue", "Boss Bureau", { 205, 213, 224, 255 }, { 120, 132, 156, 255 }, { 84, 88, 98, 255 }, { 64, 201, 158, 255 }, { 57, 68, 110, 255 } };

        case GameState::STAGE_VINGTAINE_FIGHT_SELF:
            return { "Boss Final", "Le vrai duel: soi contre soi", "Le Soi", { 36, 36, 52, 255 }, { 82, 71, 122, 255 }, { 29, 29, 39, 255 }, { 255, 211, 112, 255 }, { 132, 78, 155, 255 } };

        default:
            break;
    }

    return { "Inconnu", "", "Adversaire", DARKBLUE, BLUE, BROWN, GOLD, RED };
}

CutsceneDefinition GetCutsceneDefinition(GameState state)
{
    switch (state)
    {
        case GameState::STAGE_ENFANT_CUTSCENE:
            return { "Oral", "Le monde observe. Il faut parler, tenir debout, improviser.", { 231, 228, 219, 255 }, { 67, 56, 82, 255 }, 4.5f };

        case GameState::STAGE_VINGTAINE_CUTSCENE_FIRED:
            return { "Licencie", "Le badge ne passe plus. Tout s'arrete sans prevenir.", { 204, 54, 54, 255 }, { 255, 236, 214, 255 }, 4.0f };

        case GameState::STAGE_VINGTAINE_DEPRESSION:
            return { "Nuit", "Le lit avale les jours. Le combat se passe a l'interieur.", BLACK, { 210, 210, 225, 255 }, 5.5f };

        case GameState::ENDING_ASCENSEUR:
            return { "Ascenseur", "On remonte, lentement. Pas parfait, mais vivant.", { 18, 18, 24, 255 }, { 244, 236, 199, 255 }, 6.0f };

        default:
            break;
    }

    return { "", "", DARKGRAY, RAYWHITE, 3.0f };
}

Rectangle GetBodyRect(const Fighter& fighter)
{
    return {
        fighter.position.x - fighter.width * 0.5f,
        fighter.position.y - fighter.height,
        fighter.width,
        fighter.height
    };
}

Rectangle GetHurtbox(const Fighter& fighter)
{
    return GetBodyRect(fighter);
}

void SetAnimation(AnimationState& animation, FighterAnim next)
{
    if (animation.current == next)
    {
        return;
    }

    animation.current = next;
    animation.frame = 0;
    animation.timer = 0.0f;
    animation.finished = false;
}

void UpdateAnimation(AnimationState& animation, float dt)
{
    const AnimationClip clip = GetAnimationClip(animation.current);
    if (clip.frameCount <= 1)
    {
        animation.frame = 0;
        animation.finished = true;
        return;
    }

    animation.timer += dt;

    while (animation.timer >= clip.frameDuration)
    {
        animation.timer -= clip.frameDuration;
        animation.frame++;

        if (animation.frame >= clip.frameCount)
        {
            if (clip.loop)
            {
                animation.frame = 0;
            }
            else
            {
                animation.frame = clip.frameCount - 1;
                animation.finished = true;
                break;
            }
        }
    }
}

bool IsAttackActive(const Fighter& fighter)
{
    if (fighter.currentAttack == AttackType::NONE)
    {
        return false;
    }

    const AttackDefinition attack = GetAttackDefinition(fighter.currentAttack);
    return fighter.attackElapsed >= attack.activeStart && fighter.attackElapsed <= attack.activeEnd;
}

Rectangle BuildAttackBox(const Fighter& fighter)
{
    if (!IsAttackActive(fighter))
    {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }

    const AttackDefinition attack = GetAttackDefinition(fighter.currentAttack);
    const Rectangle body = GetBodyRect(fighter);
    const float boxX = fighter.facingRight ? body.x + body.width - 6.0f : body.x - attack.range + 6.0f;
    const float boxY = body.y + body.height * 0.38f;

    return { boxX, boxY, attack.range, attack.height };
}

bool IsFacingOpponent(const Fighter& fighter, const Fighter& opponent)
{
    return fighter.facingRight ? opponent.position.x >= fighter.position.x : opponent.position.x <= fighter.position.x;
}

Fighter CreateFighter(const char* name, bool isPlayer, Color primary, Color secondary)
{
    Fighter fighter{};
    fighter.name = name;
    fighter.isPlayer = isPlayer;
    fighter.maxHp = 100;
    fighter.hp = fighter.maxHp;
    fighter.primaryColor = primary;
    fighter.secondaryColor = secondary;
    fighter.outlineColor = Fade(BLACK, 0.85f);
    fighter.position = isPlayer ? Vector2{ 350.0f, FLOOR_Y } : Vector2{ 930.0f, FLOOR_Y };
    fighter.facingRight = isPlayer;
    fighter.animation.current = FighterAnim::IDLE;
    fighter.animation.frame = 0;
    fighter.animation.timer = 0.0f;
    fighter.onGround = true;
    return fighter;
}

void ApplyLifeLook(Fighter& player, GameState state)
{
    player.adultLook = IsVingtaineState(state);
    player.beardLook = IsVingtaineState(state);

    if (player.adultLook)
    {
        player.width = 82.0f;
        player.height = 170.0f;
        player.moveSpeed = 300.0f;
        player.jumpSpeed = 820.0f;
        player.primaryColor = { 55, 87, 145, 255 };
        player.secondaryColor = { 237, 194, 102, 255 };
    }
    else
    {
        player.width = 70.0f;
        player.height = 150.0f;
        player.moveSpeed = 280.0f;
        player.jumpSpeed = 780.0f;
        player.primaryColor = { 41, 121, 255, 255 };
        player.secondaryColor = { 255, 215, 96, 255 };
    }
}

void SetupFight(FightContext& fight, GameState state)
{
    fight = {};
    fight.definition = GetFightDefinition(state);

    fight.player = CreateFighter("Toi", true, { 41, 121, 255, 255 }, { 255, 215, 96, 255 });
    ApplyLifeLook(fight.player, state);

    fight.enemy = CreateFighter(fight.definition.opponentName, false, fight.definition.enemyColor, fight.definition.accentColor);
    fight.enemy.width = (state == GameState::STAGE_ADO_FIGHT1) ? 64.0f : 78.0f;
    fight.enemy.height = (state == GameState::STAGE_ADO_FIGHT1) ? 142.0f : 156.0f;
    fight.enemy.moveSpeed = (state == GameState::STAGE_VINGTAINE_FIGHT_SELF) ? 305.0f : 255.0f;
    fight.enemy.jumpSpeed = 760.0f;
    fight.enemy.hp = (state == GameState::STAGE_VINGTAINE_FIGHT_SELF) ? 120 : 100;
    fight.enemy.maxHp = fight.enemy.hp;
    fight.enemy.beardLook = (state == GameState::STAGE_VINGTAINE_FIGHT_SELF);
    fight.selfHintUnlocked = !IsFinalSelfFight(state);
}

void EnterState(GameContext& game, GameState nextState)
{
    game.state = nextState;
    game.stateTimer = 0.0f;

    if (IsFightState(nextState))
    {
        SetupFight(game.fight, nextState);
    }
}

FighterCommand ReadPlayerCommand()
{
    FighterCommand command{};
    command.move = static_cast<int>(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - static_cast<int>(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
    command.jump = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_SPACE);
    command.punch = IsKeyPressed(KEY_J);
    command.kick = IsKeyPressed(KEY_K);
    command.block = IsKeyDown(KEY_L) || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    return command;
}

FighterCommand BuildAICommand(Fighter& enemy, const Fighter& player, float dt)
{
    FighterCommand command{};
    enemy.aiDecisionTimer -= dt;
    enemy.aiBlockTimer = std::max(0.0f, enemy.aiBlockTimer - dt);

    const float distance = player.position.x - enemy.position.x;
    const float absDistance = std::fabs(distance);

    if (enemy.aiDecisionTimer <= 0.0f)
    {
        enemy.aiDecisionTimer = static_cast<float>(GetRandomValue(18, 42)) / 60.0f;
        enemy.aiMoveIntent = 0;

        if (absDistance > 110.0f)
        {
            enemy.aiMoveIntent = (distance > 0.0f) ? 1 : -1;
        }

        if (absDistance < 120.0f && enemy.currentAttack == AttackType::NONE && enemy.attackCooldown <= 0.0f && enemy.hitTimer <= 0.0f)
        {
            const int roll = GetRandomValue(0, 99);
            if (roll < 35)
            {
                command.punch = true;
            }
            else if (roll < 65)
            {
                command.kick = true;
            }
            else if (roll < 82)
            {
                enemy.aiBlockTimer = 0.25f + static_cast<float>(GetRandomValue(0, 25)) / 100.0f;
            }
            else if (roll < 90 && enemy.onGround)
            {
                command.jump = true;
            }
        }
    }

    command.move = enemy.aiMoveIntent;
    command.block = enemy.aiBlockTimer > 0.0f && absDistance < 150.0f;

    if (player.currentAttack != AttackType::NONE && IsAttackActive(player) && absDistance < 105.0f)
    {
        command.block = true;
        command.move = 0;
    }

    return command;
}

void StartAttack(Fighter& fighter, AttackType attack)
{
    fighter.currentAttack = attack;
    fighter.attackElapsed = 0.0f;
    fighter.attackConnected = false;
    fighter.attackBox = { 0.0f, 0.0f, 0.0f, 0.0f };
    SetAnimation(fighter.animation, attack == AttackType::PUNCH ? FighterAnim::PUNCH : FighterAnim::KICK);
}

void UpdateFighter(Fighter& fighter, const FighterCommand& command, float dt)
{
    fighter.attackCooldown = std::max(0.0f, fighter.attackCooldown - dt);
    fighter.hitTimer = std::max(0.0f, fighter.hitTimer - dt);

    if (fighter.hp <= 0)
    {
        fighter.blocking = false;
        fighter.currentAttack = AttackType::NONE;
        fighter.velocity.x = 0.0f;
    }
    else if (fighter.hitTimer <= 0.0f)
    {
        if (command.jump && fighter.onGround && fighter.currentAttack == AttackType::NONE)
        {
            fighter.velocity.y = -fighter.jumpSpeed;
            fighter.onGround = false;
        }

        fighter.blocking = command.block && fighter.currentAttack == AttackType::NONE && fighter.onGround;
        fighter.velocity.x = fighter.blocking ? 0.0f : static_cast<float>(command.move) * fighter.moveSpeed;

        if (!fighter.blocking && fighter.currentAttack == AttackType::NONE && fighter.attackCooldown <= 0.0f)
        {
            if (command.punch)
            {
                StartAttack(fighter, AttackType::PUNCH);
                fighter.velocity.x = 0.0f;
            }
            else if (command.kick)
            {
                StartAttack(fighter, AttackType::KICK);
                fighter.velocity.x = 0.0f;
            }
        }
    }
    else
    {
        fighter.blocking = false;
        fighter.velocity.x *= 0.90f;
    }

    fighter.velocity.y += GRAVITY * dt;
    fighter.position.x += fighter.velocity.x * dt;
    fighter.position.y += fighter.velocity.y * dt;

    if (fighter.position.y >= FLOOR_Y)
    {
        fighter.position.y = FLOOR_Y;
        fighter.velocity.y = 0.0f;
        fighter.onGround = true;
    }
    else
    {
        fighter.onGround = false;
    }

    const float halfWidth = fighter.width * 0.5f;
    fighter.position.x = std::clamp(fighter.position.x, STAGE_LEFT + halfWidth, STAGE_RIGHT - halfWidth);

    if (fighter.currentAttack != AttackType::NONE)
    {
        const AttackDefinition attack = GetAttackDefinition(fighter.currentAttack);
        fighter.attackElapsed += dt;

        if (fighter.attackElapsed >= attack.totalDuration)
        {
            fighter.currentAttack = AttackType::NONE;
            fighter.attackCooldown = 0.18f;
            fighter.attackElapsed = 0.0f;
            fighter.attackConnected = false;
            fighter.attackBox = { 0.0f, 0.0f, 0.0f, 0.0f };
        }
        else
        {
            fighter.attackBox = BuildAttackBox(fighter);
        }
    }
    else
    {
        fighter.attackBox = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
}

HitEvent ResolveHit(Fighter& attacker, Fighter& defender)
{
    HitEvent event{};

    if (attacker.hp <= 0 || defender.hp <= 0)
    {
        return event;
    }

    if (attacker.currentAttack == AttackType::NONE || attacker.attackConnected || !IsAttackActive(attacker))
    {
        return event;
    }

    const Rectangle hurtbox = GetHurtbox(defender);
    if (!CheckCollisionRecs(attacker.attackBox, hurtbox))
    {
        return event;
    }

    const AttackDefinition attack = GetAttackDefinition(attacker.currentAttack);
    attacker.attackConnected = true;
    event.connected = true;

    if (defender.blocking && IsFacingOpponent(defender, attacker))
    {
        event.blocked = true;
        event.damage = std::max(1, attack.damage / 4);
        defender.hp = std::max(0, defender.hp - event.damage);
        defender.velocity.x = attacker.facingRight ? 80.0f : -80.0f;
        return event;
    }

    event.damage = attack.damage;
    defender.hp = std::max(0, defender.hp - event.damage);
    defender.hitTimer = 0.22f;
    defender.blocking = false;
    defender.velocity.x = attacker.facingRight ? 190.0f : -190.0f;
    defender.velocity.y = -120.0f;
    return event;
}

void ResolveAnimations(Fighter& fighter)
{
    if (fighter.hp <= 0)
    {
        SetAnimation(fighter.animation, FighterAnim::KO);
    }
    else if (fighter.hitTimer > 0.0f)
    {
        SetAnimation(fighter.animation, FighterAnim::HIT);
    }
    else if (fighter.blocking)
    {
        SetAnimation(fighter.animation, FighterAnim::BLOCK);
    }
    else if (fighter.currentAttack == AttackType::PUNCH)
    {
        SetAnimation(fighter.animation, FighterAnim::PUNCH);
    }
    else if (fighter.currentAttack == AttackType::KICK)
    {
        SetAnimation(fighter.animation, FighterAnim::KICK);
    }
    else if (!fighter.onGround)
    {
        SetAnimation(fighter.animation, FighterAnim::JUMP);
    }
    else if (std::fabs(fighter.velocity.x) > 5.0f)
    {
        SetAnimation(fighter.animation, FighterAnim::WALK);
    }
    else
    {
        SetAnimation(fighter.animation, FighterAnim::IDLE);
    }
}

void UpdateFight(GameContext& game, float dt)
{
    FightContext& fight = game.fight;
    fight.elapsed += dt;
    const bool isFinalSelfFight = IsFinalSelfFight(game.state);

    fight.player.facingRight = fight.enemy.position.x >= fight.player.position.x;
    fight.enemy.facingRight = fight.player.position.x >= fight.enemy.position.x;

    const FighterCommand playerCommand = ReadPlayerCommand();
    FighterCommand aiCommand = BuildAICommand(fight.enemy, fight.player, dt);

    if (isFinalSelfFight && std::fabs(fight.player.position.x - fight.enemy.position.x) < 44.0f)
    {
        aiCommand.move = 0;
        aiCommand.jump = false;
        aiCommand.punch = false;
        aiCommand.kick = false;
        aiCommand.block = false;
    }

    UpdateFighter(fight.player, playerCommand, dt);
    UpdateFighter(fight.enemy, aiCommand, dt);

    const HitEvent playerHit = ResolveHit(fight.player, fight.enemy);
    const HitEvent enemyHit = ResolveHit(fight.enemy, fight.player);

    if (isFinalSelfFight && playerHit.connected)
    {
        const int backlashDamage = std::max(2, playerHit.damage * 2);
        fight.player.hp = std::max(0, fight.player.hp - backlashDamage);
        fight.player.hitTimer = std::max(fight.player.hitTimer, 0.28f);
        fight.player.blocking = false;
        fight.player.velocity.x = (fight.player.position.x < fight.enemy.position.x) ? -240.0f : 240.0f;
        fight.player.velocity.y = -140.0f;
        fight.selfHintUnlocked = true;
    }

    if (isFinalSelfFight && enemyHit.connected)
    {
        fight.selfHintUnlocked = true;
    }

    if (isFinalSelfFight && !fight.playerWon && !fight.enemyWon && fight.player.hp > 0 && fight.enemy.hp > 0)
    {
        const Rectangle playerBody = GetBodyRect(fight.player);
        const Rectangle enemyBody = GetBodyRect(fight.enemy);
        const bool overlappingSelf = CheckCollisionRecs(playerBody, enemyBody);
        const bool standingStill =
            std::fabs(fight.player.velocity.x) < 10.0f &&
            std::fabs(fight.player.velocity.y) < 10.0f &&
            fight.player.onGround &&
            playerCommand.move == 0 &&
            !playerCommand.jump &&
            !playerCommand.punch &&
            !playerCommand.kick &&
            !playerCommand.block &&
            fight.player.currentAttack == AttackType::NONE;

        if (overlappingSelf && standingStill)
        {
            fight.selfAcceptanceTimer += dt;
            fight.selfHintUnlocked = true;

            if (fight.selfAcceptanceTimer >= 4.0f)
            {
                fight.playerWon = true;
                fight.selfAcceptanceWon = true;
                fight.endTimer = 0.0f;
            }
        }
        else
        {
            fight.selfAcceptanceTimer = 0.0f;
        }
    }

    ResolveAnimations(fight.player);
    ResolveAnimations(fight.enemy);
    UpdateAnimation(fight.player.animation, dt);
    UpdateAnimation(fight.enemy.animation, dt);

    if (isFinalSelfFight && !fight.playerWon && !fight.enemyWon && fight.enemy.hp <= 0)
    {
        fight.falseVictoryTriggered = true;
        fight.selfHintUnlocked = true;
        fight.enemyWon = true;
        fight.endTimer = 0.0f;
    }
    else if (!fight.playerWon && fight.enemy.hp <= 0)
    {
        fight.playerWon = true;
        fight.endTimer = 0.0f;
    }

    if (!fight.enemyWon && fight.player.hp <= 0)
    {
        fight.enemyWon = true;
        fight.endTimer = 0.0f;
    }

    if (fight.playerWon || fight.enemyWon)
    {
        fight.endTimer += dt;

        if (fight.playerWon && fight.endTimer >= 1.25f)
        {
            EnterState(game, GetNextState(game.state));
            return;
        }

        if (fight.enemyWon && IsKeyPressed(KEY_R))
        {
            SetupFight(game.fight, game.state);
            return;
        }
    }
}

void DrawStageBackdrop(const FightDefinition& definition, const OptionalTexture* backgroundTexture)
{
    if (backgroundTexture != nullptr && backgroundTexture->loaded)
    {
        DrawTexturePro(
            backgroundTexture->texture,
            Rectangle{ 0.0f, 0.0f, static_cast<float>(backgroundTexture->texture.width), static_cast<float>(backgroundTexture->texture.height) },
            Rectangle{ 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) },
            Vector2{ 0.0f, 0.0f },
            0.0f,
            WHITE
        );
        DrawRectangle(0, static_cast<int>(FLOOR_Y), SCREEN_WIDTH, SCREEN_HEIGHT - static_cast<int>(FLOOR_Y), Fade(definition.floorColor, 0.25f));
        DrawRectangle(0, static_cast<int>(FLOOR_Y - 22.0f), SCREEN_WIDTH, 22, Fade(definition.accentColor, 0.18f));
        return;
    }

    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, definition.skyColor, definition.backdropColor);
    DrawRectangle(0, static_cast<int>(FLOOR_Y), SCREEN_WIDTH, SCREEN_HEIGHT - static_cast<int>(FLOOR_Y), definition.floorColor);
    DrawRectangle(0, static_cast<int>(FLOOR_Y - 22.0f), SCREEN_WIDTH, 22, Fade(definition.accentColor, 0.25f));

    DrawRectangle(70, 160, 240, 170, Fade(WHITE, 0.15f));
    DrawRectangle(970, 120, 180, 200, Fade(BLACK, 0.12f));
    DrawRectangle(360, 90, 560, 240, Fade(definition.accentColor, 0.10f));

    for (int i = 0; i < 9; ++i)
    {
        const int x = 90 + i * 130;
        DrawRectangle(x, static_cast<int>(FLOOR_Y - 70.0f), 80, 70, Fade(BLACK, 0.10f));
        DrawRectangleLines(x, static_cast<int>(FLOOR_Y - 70.0f), 80, 70, Fade(RAYWHITE, 0.18f));
    }
}

void DrawHealthBar(int x, int y, int width, const Fighter& fighter, bool leftAligned)
{
    DrawRectangle(x, y, width, 24, Fade(BLACK, 0.55f));

    const float ratio = static_cast<float>(fighter.hp) / static_cast<float>(fighter.maxHp);
    const int fillWidth = static_cast<int>((width - 6) * ratio);
    const int fillX = leftAligned ? x + 3 : x + width - 3 - fillWidth;
    DrawRectangle(fillX, y + 3, fillWidth, 18, fighter.primaryColor);
    DrawRectangleLines(x, y, width, 24, RAYWHITE);

    DrawText(fighter.name.c_str(), x, y - 26, 22, RAYWHITE);
}

void DrawFighterPlaceholder(const Fighter& fighter)
{
    const Rectangle body = GetBodyRect(fighter);
    const int frame = fighter.animation.frame;
    const bool hitFlash = fighter.hitTimer > 0.0f && static_cast<int>(fighter.hitTimer * 40.0f) % 2 == 0;
    const Color bodyColor = hitFlash ? WHITE : fighter.primaryColor;

    if (fighter.spriteSheet.loaded)
    {
        const AnimationClip clip = GetAnimationClip(fighter.animation.current);
        Rectangle source = {
            static_cast<float>(fighter.animation.frame * fighter.spriteSheet.frameWidth),
            static_cast<float>(clip.sheetRow * fighter.spriteSheet.frameHeight),
            fighter.facingRight ? static_cast<float>(fighter.spriteSheet.frameWidth) : -static_cast<float>(fighter.spriteSheet.frameWidth),
            static_cast<float>(fighter.spriteSheet.frameHeight)
        };

        Rectangle destination = {
            body.x,
            body.y,
            body.width,
            body.height
        };

        // Quand tu brancheras une vraie spritesheet, cette voie est deja prete:
        // remplace simplement le placeholder par DrawTexturePro / DrawTextureRec.
        DrawTexturePro(
            fighter.spriteSheet.texture,
            source,
            destination,
            { 0.0f, 0.0f },
            0.0f,
            WHITE
        );
        return;
    }

    // Placeholder pixel-art: a remplacer plus tard par DrawTextureRec/DrawTexturePro.
    DrawRectangleRounded(body, 0.10f, 4, bodyColor);
    DrawRectangleRoundedLines(body, 0.10f, 4, 2.0f, fighter.outlineColor);

    const float headSize = fighter.width * 0.42f;
    const float headX = body.x + fighter.width * 0.30f;
    const float headY = body.y - headSize * 0.20f;
    DrawRectangle(static_cast<int>(headX), static_cast<int>(headY), static_cast<int>(headSize), static_cast<int>(headSize), fighter.secondaryColor);
    DrawRectangleLines(static_cast<int>(headX), static_cast<int>(headY), static_cast<int>(headSize), static_cast<int>(headSize), fighter.outlineColor);

    const float shoulderY = body.y + fighter.height * 0.28f;
    const float hipY = body.y + fighter.height * 0.68f;
    const float armSwing = (fighter.animation.current == FighterAnim::WALK) ? ((frame % 2 == 0) ? 10.0f : -10.0f) : 0.0f;
    const float legSwing = (fighter.animation.current == FighterAnim::WALK) ? ((frame % 2 == 0) ? -12.0f : 12.0f) : 0.0f;

    float frontArmEndX = fighter.facingRight ? body.x + fighter.width + 12.0f + armSwing : body.x - 12.0f - armSwing;
    float backArmEndX = fighter.facingRight ? body.x + fighter.width - 8.0f - armSwing : body.x + 8.0f + armSwing;
    float frontArmEndY = shoulderY + 26.0f;
    float backArmEndY = shoulderY + 18.0f;

    if (fighter.animation.current == FighterAnim::PUNCH)
    {
        frontArmEndX = fighter.facingRight ? body.x + fighter.width + 32.0f : body.x - 32.0f;
        frontArmEndY = shoulderY + 10.0f;
    }
    else if (fighter.animation.current == FighterAnim::BLOCK)
    {
        frontArmEndX = fighter.facingRight ? body.x + fighter.width + 4.0f : body.x - 4.0f;
        frontArmEndY = body.y + 18.0f;
        backArmEndX = fighter.facingRight ? body.x + fighter.width - 2.0f : body.x + 2.0f;
        backArmEndY = body.y + 36.0f;
    }

    float frontLegEndX = fighter.facingRight ? body.x + fighter.width * 0.72f + legSwing : body.x + fighter.width * 0.28f - legSwing;
    float backLegEndX = fighter.facingRight ? body.x + fighter.width * 0.34f - legSwing : body.x + fighter.width * 0.66f + legSwing;
    float frontLegEndY = body.y + fighter.height + 4.0f;
    float backLegEndY = body.y + fighter.height + 2.0f;

    if (fighter.animation.current == FighterAnim::KICK)
    {
        frontLegEndX = fighter.facingRight ? body.x + fighter.width + 34.0f : body.x - 34.0f;
        frontLegEndY = body.y + fighter.height * 0.62f;
    }

    DrawLineEx({ body.x + fighter.width * 0.50f, shoulderY }, { frontArmEndX, frontArmEndY }, 6.0f, fighter.outlineColor);
    DrawLineEx({ body.x + fighter.width * 0.50f, shoulderY + 4.0f }, { backArmEndX, backArmEndY }, 5.0f, Fade(fighter.outlineColor, 0.75f));
    DrawLineEx({ body.x + fighter.width * 0.48f, hipY }, { frontLegEndX, frontLegEndY }, 7.0f, fighter.outlineColor);
    DrawLineEx({ body.x + fighter.width * 0.52f, hipY }, { backLegEndX, backLegEndY }, 6.0f, Fade(fighter.outlineColor, 0.75f));

    if (fighter.beardLook)
    {
        DrawRectangle(
            static_cast<int>(headX + headSize * 0.20f),
            static_cast<int>(headY + headSize * 0.62f),
            static_cast<int>(headSize * 0.60f),
            static_cast<int>(headSize * 0.22f),
            { 61, 38, 25, 255 }
        );
    }

    if (fighter.currentAttack != AttackType::NONE && IsAttackActive(fighter))
    {
        DrawRectangleLinesEx(fighter.attackBox, 2.0f, Fade(RED, 0.75f));
    }
}

void DrawFightScene(const GameContext& game, const BackgroundLibrary& backgrounds)
{
    const FightContext& fight = game.fight;
    const bool isFinalSelfFight = IsFinalSelfFight(game.state);
    DrawStageBackdrop(fight.definition, GetStageBackground(backgrounds, game.state));

    DrawText(fight.definition.stageTitle, 48, 34, 34, RAYWHITE);
    DrawText(fight.definition.stageSubtitle, 48, 72, 22, Fade(RAYWHITE, 0.85f));

    DrawHealthBar(48, 116, 430, fight.player, true);
    DrawHealthBar(SCREEN_WIDTH - 478, 116, 430, fight.enemy, false);

    DrawFighterPlaceholder(fight.player);
    DrawFighterPlaceholder(fight.enemy);

    DrawLine(static_cast<int>(STAGE_LEFT), static_cast<int>(FLOOR_Y), static_cast<int>(STAGE_RIGHT), static_cast<int>(FLOOR_Y), Fade(RAYWHITE, 0.35f));

    DrawRectangle(40, SCREEN_HEIGHT - 92, 640, 44, Fade(BLACK, 0.40f));
    DrawText("A/D ou Fleches: bouger   SPACE/W: sauter   J: poing   K: pied   L/Shift: parade", 52, SCREEN_HEIGHT - 80, 22, RAYWHITE);

    if (isFinalSelfFight)
    {
        DrawRectangle(SCREEN_WIDTH - 470, SCREEN_HEIGHT - 154, 430, 106, Fade(BLACK, 0.52f));
        DrawRectangleLines(SCREEN_WIDTH - 470, SCREEN_HEIGHT - 154, 430, 106, Fade(RAYWHITE, 0.35f));

        if (fight.selfHintUnlocked)
        {
            DrawText("Retrouver soi-meme est souvent la meilleure solution.", SCREEN_WIDTH - 452, SCREEN_HEIGHT - 144, 20, GOLD);
            DrawText("Parfois, on avance en cessant de frapper.", SCREEN_WIDTH - 452, SCREEN_HEIGHT - 116, 20, RAYWHITE);
        }
        else
        {
            DrawText("Le Soi ne tombe pas comme un boss ordinaire.", SCREEN_WIDTH - 452, SCREEN_HEIGHT - 136, 20, Fade(RAYWHITE, 0.88f));
            DrawText("Les coups reviennent toujours plus fort.", SCREEN_WIDTH - 452, SCREEN_HEIGHT - 108, 20, Fade(RAYWHITE, 0.72f));
        }
    }

    if (fight.playerWon)
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.35f));
        if (isFinalSelfFight && fight.selfAcceptanceWon)
        {
            DrawText("Tu te retrouves", SCREEN_WIDTH / 2 - 170, SCREEN_HEIGHT / 2 - 26, 46, GOLD);
        }
        else
        {
            DrawText("Victoire", SCREEN_WIDTH / 2 - 82, SCREEN_HEIGHT / 2 - 26, 46, GOLD);
        }
    }

    if (fight.enemyWon)
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.45f));
        if (isFinalSelfFight && fight.falseVictoryTriggered)
        {
            DrawText("Tu l'as vaincu, mais tu t'es perdu", SCREEN_WIDTH / 2 - 312, SCREEN_HEIGHT / 2 - 26, 42, RED);
            DrawText("Retrouver soi-meme est souvent la meilleure solution.", SCREEN_WIDTH / 2 - 286, SCREEN_HEIGHT / 2 + 24, 24, GOLD);
            DrawText("Appuie sur R pour recommencer ce stage", SCREEN_WIDTH / 2 - 188, SCREEN_HEIGHT / 2 + 58, 22, RAYWHITE);
        }
        else
        {
            DrawText("Defaite", SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 - 26, 46, RED);
            DrawText("Appuie sur R pour recommencer ce stage", SCREEN_WIDTH / 2 - 188, SCREEN_HEIGHT / 2 + 26, 24, RAYWHITE);
        }
    }
}

void DrawGenericCutscene(const GameContext& game)
{
    const CutsceneDefinition cutscene = GetCutsceneDefinition(game.state);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, cutscene.backgroundColor);
    DrawRectangle(70, 60, SCREEN_WIDTH - 140, SCREEN_HEIGHT - 120, Fade(cutscene.foregroundColor, 0.18f));
    DrawRectangleLines(70, 60, SCREEN_WIDTH - 140, SCREEN_HEIGHT - 120, cutscene.foregroundColor);

    DrawText(cutscene.title, 120, 120, 64, cutscene.foregroundColor);
    DrawText(cutscene.subtitle, 120, 206, 28, cutscene.foregroundColor);
    DrawText("ENTREE pour passer", 120, SCREEN_HEIGHT - 110, 24, Fade(cutscene.foregroundColor, 0.90f));
}

void DrawDepressionCutscene(const GameContext& game)
{
    const CutsceneDefinition cutscene = GetCutsceneDefinition(game.state);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, cutscene.backgroundColor);

    const int darkness = static_cast<int>(std::min(game.stateTimer * 42.0f, 190.0f));
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 0, 0, 0, static_cast<unsigned char>(darkness) });

    const int bedX = SCREEN_WIDTH / 2 - 160;
    const int bedY = SCREEN_HEIGHT / 2 + 20;
    DrawRectangle(bedX, bedY, 320, 46, { 70, 70, 94, 255 });
    DrawRectangle(bedX + 24, bedY - 20, 74, 24, { 187, 187, 205, 255 });
    DrawRectangle(bedX + 12, bedY - 58, 300, 42, { 46, 46, 58, 255 });
    DrawRectangle(bedX + 214, bedY - 84, 70, 26, { 84, 84, 104, 255 });

    DrawText(cutscene.title, 94, 96, 62, cutscene.foregroundColor);
    DrawText(cutscene.subtitle, 94, 176, 28, Fade(cutscene.foregroundColor, 0.88f));
    DrawText("ENTREE pour se relever", 94, SCREEN_HEIGHT - 100, 24, Fade(cutscene.foregroundColor, 0.85f));
}

void DrawEndingAscenseur(const GameContext& game)
{
    const CutsceneDefinition cutscene = GetCutsceneDefinition(game.state);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, cutscene.backgroundColor);

    DrawRectangle(SCREEN_WIDTH / 2 - 120, 80, 240, SCREEN_HEIGHT - 160, { 44, 44, 58, 255 });
    DrawRectangleLines(SCREEN_WIDTH / 2 - 120, 80, 240, SCREEN_HEIGHT - 160, cutscene.foregroundColor);
    DrawLine(SCREEN_WIDTH / 2, 80, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 80, Fade(cutscene.foregroundColor, 0.35f));

    const float normalized = std::min(game.stateTimer / cutscene.duration, 1.0f);
    const float cabinY = 520.0f - normalized * 330.0f;
    DrawRectangle(SCREEN_WIDTH / 2 - 90, static_cast<int>(cabinY), 180, 120, { 192, 160, 88, 255 });
    DrawRectangleLines(SCREEN_WIDTH / 2 - 90, static_cast<int>(cabinY), 180, 120, cutscene.foregroundColor);

    DrawText(cutscene.title, 120, 110, 60, cutscene.foregroundColor);
    DrawText(cutscene.subtitle, 120, 188, 28, Fade(cutscene.foregroundColor, 0.88f));
    DrawText("ENTREE pour conclure", 120, SCREEN_HEIGHT - 100, 24, Fade(cutscene.foregroundColor, 0.85f));
}

void DrawMenu()
{
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, { 48, 57, 94, 255 }, { 17, 20, 34, 255 });
    DrawRectangle(70, 70, SCREEN_WIDTH - 140, SCREEN_HEIGHT - 140, Fade(BLACK, 0.22f));
    DrawRectangleLines(70, 70, SCREEN_WIDTH - 140, SCREEN_HEIGHT - 140, Fade(RAYWHITE, 0.35f));

    DrawText("AU FIL DE LA VIE", 118, 118, 64, RAYWHITE);
    DrawText("Jeu de combat 2D narratif - structure Game Jam Raylib", 122, 194, 28, Fade(RAYWHITE, 0.88f));

    DrawText("Stages:", 122, 280, 34, GOLD);
    DrawText("Enfance  ->  Adolescence  ->  Vingtaine  ->  Face a soi", 122, 328, 30, RAYWHITE);
    DrawText("Chaque victoire fait avancer l'histoire automatiquement.", 122, 372, 28, Fade(RAYWHITE, 0.88f));
    DrawText("Les rectangles sont des placeholders pour spritesheets animees.", 122, 412, 28, Fade(RAYWHITE, 0.88f));

    DrawRectangle(120, 512, 540, 72, Fade(SKYBLUE, 0.15f));
    DrawRectangleLines(120, 512, 540, 72, SKYBLUE);
    DrawText("ENTREE pour commencer", 156, 534, 34, RAYWHITE);

    DrawText("Controles combat: A/D, SPACE, J, K, L", 122, 614, 24, Fade(RAYWHITE, 0.75f));
}

void DrawGameComplete()
{
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, { 245, 220, 170, 255 }, { 144, 109, 208, 255 });
    DrawText("Fin du parcours", 130, 140, 66, WHITE);
    DrawText("Tu as traverse les ages, les projections et le regard de l'autre.", 134, 236, 28, Fade(WHITE, 0.95f));
    DrawText("Le code est pret pour accueillir spritesheets, VFX et vraies illustrations.", 134, 278, 28, Fade(WHITE, 0.92f));
    DrawText("ENTREE pour revenir au menu", 134, 380, 32, WHITE);
}

void UpdateCutsceneState(GameContext& game, float dt)
{
    game.stateTimer += dt;
    const CutsceneDefinition cutscene = GetCutsceneDefinition(game.state);

    if (game.stateTimer >= cutscene.duration || IsKeyPressed(KEY_ENTER))
    {
        EnterState(game, GetNextState(game.state));
    }
}

} // namespace

int main()
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Au fil de la vie - Prototype combat narratif");
    SetTargetFPS(TARGET_FPS);

    GameContext game{};
    BackgroundLibrary backgrounds{};
    LoadBackgroundLibrary(backgrounds);
    EnterState(game, GameState::MENU);

    while (!WindowShouldClose())
    {
        const float dt = GetFrameTime();

        switch (game.state)
        {
            case GameState::MENU:
                game.stateTimer += dt;
                if (IsKeyPressed(KEY_ENTER))
                {
                    EnterState(game, GameState::STAGE_ENFANT_FIGHT1);
                }
                break;

            case GameState::STAGE_ENFANT_FIGHT1:
            case GameState::STAGE_ENFANT_FIGHT2:
            case GameState::STAGE_ADO_FIGHT1:
            case GameState::STAGE_ADO_FIGHT2:
            case GameState::STAGE_VINGTAINE_TAFF:
            case GameState::STAGE_VINGTAINE_FIGHT_SELF:
                game.stateTimer += dt;
                UpdateFight(game, dt);
                break;

            case GameState::STAGE_ENFANT_CUTSCENE:
            case GameState::STAGE_VINGTAINE_CUTSCENE_FIRED:
            case GameState::STAGE_VINGTAINE_DEPRESSION:
            case GameState::ENDING_ASCENSEUR:
                UpdateCutsceneState(game, dt);
                break;

            case GameState::GAME_COMPLETE:
                game.stateTimer += dt;
                if (IsKeyPressed(KEY_ENTER))
                {
                    EnterState(game, GameState::MENU);
                }
                break;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        switch (game.state)
        {
            case GameState::MENU:
                DrawMenu();
                break;

            case GameState::STAGE_ENFANT_FIGHT1:
            case GameState::STAGE_ENFANT_FIGHT2:
            case GameState::STAGE_ADO_FIGHT1:
            case GameState::STAGE_ADO_FIGHT2:
            case GameState::STAGE_VINGTAINE_TAFF:
            case GameState::STAGE_VINGTAINE_FIGHT_SELF:
                DrawFightScene(game, backgrounds);
                break;

            case GameState::STAGE_ENFANT_CUTSCENE:
            case GameState::STAGE_VINGTAINE_CUTSCENE_FIRED:
                DrawGenericCutscene(game);
                break;

            case GameState::STAGE_VINGTAINE_DEPRESSION:
                DrawDepressionCutscene(game);
                break;

            case GameState::ENDING_ASCENSEUR:
                DrawEndingAscenseur(game);
                break;

            case GameState::GAME_COMPLETE:
                DrawGameComplete();
                break;
        }

        EndDrawing();
    }

    UnloadBackgroundLibrary(backgrounds);
    CloseWindow();
    return 0;
}
