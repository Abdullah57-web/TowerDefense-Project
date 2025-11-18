/* 
	Group Members:
1. Uzair Nadeem (CT-24037)
2. Abdullah (CT-24026)
3. M. Taqi Siddiqui(CT-24027)
4. Ibtissam (CT-24020)
*/

#include "raylib.h"
#include <vector>
#include <list>
#include <queue>
#include <algorithm>
#include <memory>
#include <cmath>
#include <functional>
#include <string>
using namespace std;
//Constant throughout the game
const int SCREEN_WIDTH = 1200;
const int SCREEN_HEIGHT = 800;
const int GROUND_HEIGHT = 100;
const int LANE_Y = SCREEN_HEIGHT / 2;

// Tower
const int TOWER_HP = 3500; 
const int TOWER_DAMAGE = 50;
const float TOWER_ATTACK_RATE = 1.0f;
const float TOWER_RANGE = 300.0f;
//enum means fixed values like here gamestates can only be of three types
enum class GameState {
    START_SCREEN,
    PLAYING,
    GAME_OVER
};

enum class UnitType {
    KNIGHT,
    ARCHER, 
    GIANT,
    WIZARD
};

struct UnitStats {
    string name;
    int cost;
    int hp;
    int damage;
    float speed;
    float attackRate;
    float range;
    bool isRanged;
    Color color;
    int size;
};

struct WaveUnit {
    UnitType type;
    int count;
    
    WaveUnit(UnitType t, int c) : type(t), count(c) {}
};

class Unit {
public:
    UnitType type;
    Vector2 position; //vector2 is used for position (x,y)
    int currentHP;
    int maxHP;
    int damage;
    float speed;
    float attackRate;
    float attackRange;
    bool isRanged;
    float attackTimer;
    bool isPlayer;
    Color color;
    int size;
    bool isAlive;
    Unit* target;
    bool isFrozen;
    float freezeTimer;
    list<Vector2> path; 
    Vector2 currentTargetPos;

    Unit(UnitType unitType, bool player);
    void Update(float deltaTime, list<unique_ptr<Unit>>& allUnits);
    void Draw();
    void FindTargetWithPriority(list<unique_ptr<Unit>>& allUnits);
    void Attack(Unit* targetUnit, list<unique_ptr<Unit>>& allUnits);

private:
    void generatePath();
    void FollowPath(float deltaTime);
    void DrawPath();
    float CalculateDistance(Vector2 a, Vector2 b);
    string getUnitTypeString(UnitType type);
    UnitStats getUnitStats(UnitType type);
};

// to find optimal path
struct PathNode {
    int x;
    float cost;
    bool operator > (const PathNode& other) const { //operator overloading
        return cost > other.cost;
    }
};

// Priority comparison for targeting tower- closest first then low hp
struct TowerTargetPriority {
    bool operator () (const pair<Unit*, float>& a, const pair<Unit*, float>& b) {
        
        if (fabs(a.second - b.second) < 10.0f) {
            return a.first->currentHP > b.first->currentHP; 
        }
        return a.second > b.second; 
    }
};

// Wave management
struct GameWave {
    int waveNumber;
    vector<WaveUnit> waveUnits; 
    float spawnRate;
    float waveCooldown; 
    GameWave* nextWave;
    
    GameWave(int num, const vector<WaveUnit>& units, float rate, float cooldown = 10.0f)  //constructor
        : waveNumber(num), waveUnits(units), spawnRate(rate), waveCooldown(cooldown), nextWave(nullptr) {}
};


Unit::Unit(UnitType unitType, bool player) {
    type = unitType;
    isPlayer = player;
    isAlive = true;
    target = nullptr;
    isFrozen = false;
    freezeTimer = 0.0f;
    UnitStats stats = getUnitStats(unitType);
    maxHP = stats.hp;
    currentHP = maxHP;
    damage = stats.damage;
    speed = stats.speed;
    attackRate = stats.attackRate;
    attackRange = stats.range;
    isRanged = stats.isRanged;
    color = stats.color;
    size = stats.size;
    
    //Initial position of player and enemy tower
    if (isPlayer) {
        position = { 150.0f, LANE_Y };
        currentTargetPos = { (float)SCREEN_WIDTH - 50.0f, LANE_Y }; 
    } else {
        position = { (float)SCREEN_WIDTH - 150.0f, LANE_Y };
        currentTargetPos = { 50.0f, LANE_Y }; 
    }
    
    generatePath();
    attackTimer = 0.0f;
}

void Unit::Update(float deltaTime, list<unique_ptr<Unit>>& allUnits) {
    if (!isAlive) return;
    
    if (isFrozen) {
        freezeTimer -= deltaTime;
        if (freezeTimer <= 0) {
            isFrozen = false;
        }
        return;
    }
    
    if (!target || !target->isAlive) {
        FindTargetWithPriority(allUnits);
    }
    
    if (target && target->isAlive) {
        float distanceToTarget = CalculateDistance(position, target->position);
        
        if (distanceToTarget <= attackRange) {
            // Unit in range - Attack
            attackTimer += deltaTime;
            if (attackTimer >= attackRate) {
                Attack(target, allUnits);
                attackTimer = 0.0f;
            }
        } else {
            FollowPath(deltaTime);
            attackTimer = 0.0f;
        }
    } else {
        FollowPath(deltaTime);
    }
}

void Unit::Draw() {
    if (!isAlive) return;
    
    // Circle shaped unit + Health bar 
    Color drawColor = color;
    if (isFrozen) {
        drawColor = BLUE;
        DrawCircle(position.x, position.y, size + 5, Fade(SKYBLUE, 0.3f));
    }
    
    DrawCircle(position.x, position.y, size, drawColor);
    
    // Units Border Implementation (Player- Blue, Enemy- Red)
    if (isPlayer) {
        DrawCircleLines(position.x, position.y, size + 3, BLUE);
    } else {
        DrawCircleLines(position.x, position.y, size + 3, RED);
    }
    
    // Attack radius for ranged units
    if (isRanged) {
        DrawCircleLines(position.x, position.y, attackRange, Fade(color, 0.3f));
    }
    
    // Health bar
    float healthPercent = (float)currentHP / (float)maxHP;
    DrawRectangle(position.x - size, position.y - size - 15, size * 2, 5, RED);
    DrawRectangle(position.x - size, position.y - size - 15, size * 2 * healthPercent, 5, GREEN);
    
    // unit type indicator
    string typeStr = getUnitTypeString(type);
    DrawText(typeStr.c_str(), position.x - 10, position.y - 8, 12, BLACK);
    
    // freeze indicator
    if (isFrozen) {
        DrawText("FROZEN", position.x - 15, position.y + size + 5, 10, BLUE);
    }
    
    // Target alive so draw target line
    if (target && target->isAlive) {
        DrawLine(position.x, position.y, target->position.x, target->position.y, Fade(RED, 0.5f));
    }
    
    // Draws path
    DrawPath();
}

// Sorting to find priority based targets
void Unit::FindTargetWithPriority(list<unique_ptr<Unit>>& allUnits) {
    vector<pair<Unit*, float>> potentialTargets;
    
    // Search for targets
    for (auto& unit : allUnits) {
        if (unit->isAlive && unit->isPlayer != isPlayer) {
            float distance = CalculateDistance(position, unit->position);
            if (distance <= attackRange * 1.5f) {
                potentialTargets.push_back({unit.get(), distance});
            }
        }
    }
    
    // Sorting enemy targets by priority, attacks closest first then lowest HP
    if (!potentialTargets.empty()) {
        sort(potentialTargets.begin(), potentialTargets.end(),
            [](const auto& a, const auto& b) {
                if (fabs(a.second - b.second) < 10.0f) {
                    return a.first->currentHP < b.first->currentHP;
                }
                return a.second < b.second;
            });
        
        target = potentialTargets[0].first;
    } else {
        target = nullptr;
    }
}

void Unit::Attack(Unit* targetUnit, list<unique_ptr<Unit>>& allUnits) {
    if (!targetUnit || !targetUnit->isAlive) return;
    
    targetUnit->currentHP -= damage;
    
    // Area damage for wizard
    if (type == UnitType::WIZARD) {
        for (auto& unit : allUnits) {
            if (unit->isAlive && unit.get() != targetUnit && unit->isPlayer != isPlayer) {
                float distance = CalculateDistance(unit->position, targetUnit->position);
                if (distance < 60.0f) {
                    unit->currentHP -= damage / 2; //reduces actual damage to half
                }
            }
        }
    }
    
    if (targetUnit->currentHP <= 0) {
        targetUnit->isAlive = false;
        target = nullptr;
    }
}

void Unit::generatePath() {
    path.clear();
    
    if (isPlayer) {
        // Player units movs toward enemy tower
        for (int x = (int)position.x; x < SCREEN_WIDTH - 50; x += 50) {
            path.push_back({(float)x, LANE_Y});
        }
    } else {
        // Enemy units moves toward player tower
        for (int x = (int)position.x; x > 50; x -= 50) {
            path.push_back({(float)x, LANE_Y});
        }
    }
    
    if (!path.empty()) {
        currentTargetPos = path.front();
    }
}

void Unit::FollowPath(float deltaTime) {
    if (path.empty()) return;
    
    Vector2 direction = {
        currentTargetPos.x - position.x,
        currentTargetPos.y - position.y
    };
    // Calculate distance to the target point
    float distance = sqrt(direction.x * direction.x + direction.y * direction.y);
    
    if (distance < 5.0f) {
        path.pop_front();
        if (!path.empty()) {
            currentTargetPos = path.front();
        }
    } else {
        direction.x /= distance;
        direction.y /= distance;
        
        position.x += direction.x * speed * deltaTime;
        position.y += direction.y * speed * deltaTime;
    }
}

void Unit::DrawPath() {
    if (path.empty()) return;
    
    // Draw path lines
    Vector2 prevPos = position;
    for (const auto& point : path) {
        DrawLine(prevPos.x, prevPos.y, point.x, point.y, Fade(BLUE, 0.3f));
        prevPos = point;
    }
    
    // Draw waypoints
    for (const auto& point : path) {
        DrawCircle(point.x, point.y, 3, Fade(GREEN, 0.5f));
    }
}

float Unit::CalculateDistance(Vector2 a, Vector2 b) {
    return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

string Unit::getUnitTypeString(UnitType type) {
    switch(type) {
        case UnitType::KNIGHT: return "K";
        case UnitType::ARCHER: return "A";
        case UnitType::GIANT: return "G";
        case UnitType::WIZARD: return "W";
        default: return "?";
    }
}

UnitStats Unit::getUnitStats(UnitType type) {
    switch(type) {
        case UnitType::KNIGHT:
            return UnitStats{"Knight", 3, 300, 60, 80.0f, 1.2f, 40.0f, false, BLUE, 25};
        case UnitType::ARCHER:
            return UnitStats{"Archer", 3, 150, 40, 60.0f, 1.5f, 150.0f, true, GREEN, 20};
        case UnitType::GIANT:
            return UnitStats{"Giant", 5, 1000, 80, 40.0f, 2.0f, 50.0f, false, GRAY, 35};
        case UnitType::WIZARD:
            return UnitStats{"Wizard", 4, 180, 70, 50.0f, 2.5f, 120.0f, true, PURPLE, 22};
        default:
            return UnitStats{"Unknown", 0, 0, 0, 0.0f, 0.0f, 0.0f, false, WHITE, 0};
    }
}

class Projectile {
public:
    Vector2 startPos;
    Vector2 endPos;
    float progress;
    bool active;
    Color color;

    Projectile(Vector2 start, Vector2 end, Color col) {
        startPos = start;
        endPos = end;
        progress = 0.0f;
        active = true;
        color = col;
    }
    //updates projectile movement
    void Update(float deltaTime) {
        progress += deltaTime * 3.0f;
        if (progress >= 1.0f) {
            active = false;
        }
    }
    // Draws projectile
    void Draw() {
        if (!active) return;
        
        Vector2 currentPos = {
            startPos.x + (endPos.x - startPos.x) * progress,
            startPos.y + (endPos.y - startPos.y) * progress
        };
        
        DrawCircle(currentPos.x, currentPos.y, 4, color);
        DrawCircle(currentPos.x, currentPos.y, 6, Fade(color, 0.5f));
    }
};

class Tower {
public:
    Vector2 position;   //For tower position
    int currentHP;
    int maxHP;
    int damage;
    float attackRate;
    float attackTimer;
    bool isPlayer;
    bool isAlive;
    
    // Priority queue for closest target
    priority_queue<pair<Unit*, float>,vector<pair<Unit*, float>>,TowerTargetPriority> targetQueue;

    Tower(bool player) {
        isPlayer = player;
        maxHP = TOWER_HP;
        currentHP = maxHP;
        damage = TOWER_DAMAGE;
        attackRate = TOWER_ATTACK_RATE;
        attackTimer = 0.0f;
        isAlive = true;
        
        if (isPlayer) {
            position = { 50.0f, LANE_Y };
        } else {
            position = { (float)SCREEN_WIDTH - 50.0f, LANE_Y };
        }
    }

    void Update(float deltaTime, list<unique_ptr<Unit>>& units) {
        if (!isAlive) return;
        attackTimer += deltaTime;
        
        // // Refresh list of potential targets
        UpdateTargetQueue(units);
    }
    // Draw tower 
    void Draw() {
        if (!isAlive) return;
        
        Color towerColor = isPlayer ? BLUE : RED;
        Color darkTowerColor = isPlayer ? DARKBLUE : MAROON;
        Color lightTowerColor = isPlayer ? SKYBLUE : PINK;
        
        DrawRectangle(position.x - 50, position.y - 40, 100, 80, darkTowerColor);
        DrawRectangle(position.x - 45, position.y - 35, 90, 70, towerColor);
        
        DrawRectangle(position.x - 35, position.y - 70, 70, 40, darkTowerColor);
        DrawRectangle(position.x - 30, position.y - 65, 60, 30, towerColor);
        
        DrawRectangle(position.x - 25, position.y - 100, 50, 40, darkTowerColor);
        DrawRectangle(position.x - 20, position.y - 95, 40, 30, lightTowerColor);
        
        if (isPlayer) {
            DrawRectangle(position.x + 25, position.y - 110, 15, 25, BLUE);
            DrawRectangle(position.x + 25, position.y - 115, 20, 5, DARKBLUE);
        } else {
            DrawRectangle(position.x - 40, position.y - 110, 15, 25, RED);
            DrawRectangle(position.x - 45, position.y - 115, 20, 5, MAROON);
        }
        
        DrawRectangle(position.x - 8, position.y - 85, 16, 12, DARKGRAY);
        DrawRectangle(position.x - 5, position.y - 82, 10, 6, YELLOW);
        
        DrawRectangle(position.x - 15, position.y - 15, 30, 35, darkTowerColor);
        DrawRectangle(position.x - 12, position.y - 12, 24, 29, BROWN);
        
        float healthPercent = (float)currentHP / (float)maxHP;
        DrawRectangle(position.x - 50, position.y - 120, 100, 10, RED);
        DrawRectangle(position.x - 50, position.y - 120, 100 * healthPercent, 10, GREEN);
        
        string label = isPlayer ? "Your Tower" : "Enemy Tower";
        DrawText(label.c_str(), position.x - 40, position.y - 135, 12, BLACK);
    }

    bool CanAttack() {
        return attackTimer >= attackRate;
    }

    void ResetAttackTimer() {
        attackTimer = 0.0f;
    }
    // Build priority queue of all targets in range
    void UpdateTargetQueue(list<unique_ptr<Unit>>& units) {
        
        targetQueue = priority_queue<pair<Unit*, float>, vector<pair<Unit*, float>>,TowerTargetPriority>();
        
        for (auto& unit : units) {
            if (unit->isAlive && unit->isPlayer != isPlayer) {
                float distance = CalculateDistance(position, unit->position);
                if (distance < TOWER_RANGE) {
                    targetQueue.push({unit.get(), distance});
                }
            }
        }
    }

    Unit* GetBestTarget() {
        if (!targetQueue.empty()) {
            return targetQueue.top().first;
        }
        return nullptr;
    }

private:
    float CalculateDistance(Vector2 a, Vector2 b) {
        return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    }
};

class Game {
public:
    GameState currentState;
    Tower playerTower;
    Tower enemyTower;
    list<unique_ptr<Unit>> units;
    vector<Projectile> projectiles;
    
    // Freeze ability
    bool freezeAvailable;
    float freezeCooldown;
    const float FREEZE_DURATION = 5.0f;
    const float FREEZE_COOLDOWN = 30.0f;
    int playerElixir;
    const int MAX_ELIXIR = 10;
    float elixirTimer;
    const float ELIXIR_RATE = 2.0f;
    
    // Wave progression using linked list
    GameWave* waveList;
    GameWave* currentWave;
    float waveSpawnTimer;
    int currentUnitTypeIndex;
    int unitsSpawnedForCurrentType;
    bool isBetweenWaves;
    float betweenWavesTimer;
    
    // 2 min timer
    float gameTimer;
    const float GAME_TIME_LIMIT = 120.0f;
    
    bool gameOver;
    string winner;

    Game() : playerTower(true), enemyTower(false) {
        currentState = GameState::START_SCREEN; // Start with start screen
        playerElixir = 5;
        elixirTimer = 0.0f;
        gameOver = false;
        winner = "";
        
        // Initialize freeze ability
        freezeAvailable = true;
        freezeCooldown = 0.0f;
        
        // Initialize game timer
        gameTimer = GAME_TIME_LIMIT;
        
        // Initialize wave progression
        InitializeWaves();
        waveSpawnTimer = 0.0f;
        currentUnitTypeIndex = 0;
        unitsSpawnedForCurrentType = 0;
        isBetweenWaves = false;
        betweenWavesTimer = 0.0f;
    }

    ~Game() {
        // Deletes all waves
        GameWave* current = waveList;
        while (current) {
            GameWave* next = current->nextWave;
            delete current;
            current = next;
        }
    }

    void Update(float deltaTime) {
        if (currentState == GameState::START_SCREEN) {
            if (IsKeyPressed(KEY_ENTER)) {
                currentState = GameState::PLAYING;
            }
            return;
        }
        
        if (currentState == GameState::GAME_OVER) {
            if (IsKeyPressed(KEY_R)) {
                Reset();
                currentState = GameState::PLAYING;
            }
            return;
        }

        if (gameOver) {
            currentState = GameState::GAME_OVER;
            return;
        }

        if (!freezeAvailable) {
            freezeCooldown -= deltaTime;
            if (freezeCooldown <= 0) {
                freezeAvailable = true;
                freezeCooldown = 0.0f;
            }
        }

        gameTimer -= deltaTime;
        if (gameTimer <= 0) {
            gameTimer = 0;
            gameOver = true;
            winner = "Draw";
        }

        // Update elixir
        elixirTimer += deltaTime;
        if (elixirTimer >= ELIXIR_RATE) {
            if (playerElixir < MAX_ELIXIR) playerElixir++;
            elixirTimer = 0.0f;
        }

        playerTower.Update(deltaTime, units);
        enemyTower.Update(deltaTime, units);

        // Update units 
        for (auto it = units.begin(); it != units.end(); ) {
            if ((*it)->isAlive) {
                (*it)->Update(deltaTime, units);
                
                //  Check if unit hits enemy tower
                if ((*it)->isPlayer && (*it)->position.x >= enemyTower.position.x - 60) {
                    enemyTower.currentHP -= (*it)->damage;
                    CreateAttackEffect((*it)->position, enemyTower.position, (*it)->color);
                    if (enemyTower.currentHP <= 0) {
                        enemyTower.currentHP = 0;
                        enemyTower.isAlive = false;
                        gameOver = true;
                        winner = "Player Wins!";
                    }
                } else if (!(*it)->isPlayer && (*it)->position.x <= playerTower.position.x + 60) {
                    playerTower.currentHP -= (*it)->damage;
                    CreateAttackEffect((*it)->position, playerTower.position, (*it)->color);
                    if (playerTower.currentHP <= 0) {
                        playerTower.currentHP = 0;
                        playerTower.isAlive = false;
                        gameOver = true;
                        winner = "Enemy Wins!";
                    }
                }
                ++it;
            } else {
                // Remove dead units from list
                it = units.erase(it);
            }
        }

        // Update projectiles
        for (auto& projectile : projectiles) {
            projectile.Update(deltaTime);
        }

        // Remove dead projectiles
        projectiles.erase(remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& proj) { return !proj.active; }),
            projectiles.end());

        HandleTowerAttacks();

        HandleWaveProgression(deltaTime);
    }

    void Draw() {
        if (currentState == GameState::START_SCREEN) {
            DrawStartScreen();
            return;
        }
        
        if (currentState == GameState::GAME_OVER) {
            DrawGameOverScreen();
            return;
        }
    
        // Draw background
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LIGHTGRAY);
        
        // Draw  lane
        DrawRectangle(0, LANE_Y - 75, SCREEN_WIDTH, 150, DARKGRAY);
        
        // Draw path line
        DrawLine(SCREEN_WIDTH / 2, LANE_Y - 75, SCREEN_WIDTH / 2, LANE_Y + 75, YELLOW);
        
        // Draw ground
        DrawRectangle(0, 0, SCREEN_WIDTH, GROUND_HEIGHT, BROWN);

        // Draw towers
        playerTower.Draw();
        enemyTower.Draw();

        // Draw units
        for (auto& unit : units) {
            unit->Draw();
        }

        // Draw projectiles
        for (auto& projectile : projectiles) {
            projectile.Draw();
        }
        DrawUI();
    }

    void DrawStartScreen() {
        DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, DARKBLUE, BLUE);
        // Title
        DrawText("TOWER DEFENSE", SCREEN_WIDTH/2 - MeasureText("TOWER DEFENSE", 80)/2, 100, 80, YELLOW);
        
        // game description
        DrawText("Defend your tower against enemy waves for 2 minutes!", SCREEN_WIDTH/2 - MeasureText("Defend your tower against enemy waves for 2 minutes!", 30)/2, 220, 30, WHITE);
        
        // Unit info
        int leftColumnX = SCREEN_WIDTH/2 - 400;
        int rightColumnX = SCREEN_WIDTH/2 + 100;
        int startY = 300;
        int lineHeight = 35;
        
        DrawText("UNIT TYPES:", leftColumnX, startY, 28, GREEN);
        DrawText("Knight (Press 1) - Strong melee unit", leftColumnX, startY + lineHeight, 22, WHITE);
        DrawText("Archer (Press 2) - Ranged attacker", leftColumnX, startY + lineHeight * 2, 22, WHITE);
        DrawText("Giant (Press 3) - High HP tank", leftColumnX, startY + lineHeight * 3, 22, WHITE);
        DrawText("Wizard (Press 4) - Area damage dealer", leftColumnX, startY + lineHeight * 4, 22, WHITE);
        
        DrawText("SPECIAL ABILITIES:", rightColumnX, startY, 28, GREEN);
        DrawText("Freeze (Press F) - Freeze enemies for 5s", rightColumnX, startY + lineHeight, 22, WHITE);
        DrawText("30s cooldown", rightColumnX, startY + lineHeight * 2, 22, WHITE);
        
        DrawText("PRESS ENTER TO START", SCREEN_WIDTH/2 - MeasureText("PRESS ENTER TO START", 50)/2, 550, 50, GREEN);
        
        DrawText("Defend your tower and destroy the enemy tower to win!", SCREEN_WIDTH/2 - MeasureText("Defend your tower and destroy the enemy tower to win!", 22)/2, 650, 22, YELLOW);
    }

    void DrawGameOverScreen() {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, {0, 0, 0, 200});
        DrawText(winner.c_str(), SCREEN_WIDTH/2 - MeasureText(winner.c_str(), 60)/2, SCREEN_HEIGHT/2 - 50, 60, WHITE);
        DrawText("Press R to Restart", SCREEN_WIDTH/2 - MeasureText("Press R to Restart", 30)/2, SCREEN_HEIGHT/2 + 40, 30, GREEN);
        DrawText("Press ESC to Exit", SCREEN_WIDTH/2 - MeasureText("Press ESC to Exit", 25)/2, SCREEN_HEIGHT/2 + 90, 25, YELLOW);
    }

    void SpawnUnit(UnitType type) {
        if (currentState != GameState::PLAYING) return;
        
        UnitStats stats = GetUnitStats(type);
        if (playerElixir >= stats.cost) {
            units.push_back(make_unique<Unit>(type, true));// Creates unit
            playerElixir -= stats.cost;// subtracts elixir
        }
    }

    void ActivateFreeze() {
        if (currentState != GameState::PLAYING) return;
        if (!freezeAvailable) return;
        
        // Freeze all enemy units
        for (auto& unit : units) {
            if (!unit->isPlayer && unit->isAlive) {
                unit->isFrozen = true;
                unit->freezeTimer = FREEZE_DURATION;
            }
        }
        
        freezeAvailable = false;
        freezeCooldown = FREEZE_COOLDOWN;
        CreateFreezeEffect();
    }

    void CreateFreezeEffect() {
        for (int i = 0; i < 20; i++) {
            Vector2 startPos = { (float)(GetRandomValue(0, SCREEN_WIDTH)), (float)(GetRandomValue(0, SCREEN_HEIGHT)) };
            Vector2 endPos = { (float)(GetRandomValue(0, SCREEN_WIDTH)), (float)(GetRandomValue(0, SCREEN_HEIGHT)) };
            projectiles.push_back(Projectile(startPos, endPos, SKYBLUE));
        }
    }

    void CreateAttackEffect(Vector2 from, Vector2 to, Color color) {
        projectiles.push_back(Projectile(from, to, color));
    }

    void Reset() {
        units.clear();
        projectiles.clear();
        playerTower = Tower(true);
        enemyTower = Tower(false);
        playerElixir = 5;
        elixirTimer = 0.0f;
        gameOver = false;
        winner = "";
        gameTimer = GAME_TIME_LIMIT;
        
        freezeAvailable = true;
        freezeCooldown = 0.0f;
        
        currentWave = waveList;
        waveSpawnTimer = 0.0f;
        currentUnitTypeIndex = 0;
        unitsSpawnedForCurrentType = 0;
        isBetweenWaves = false;
        betweenWavesTimer = 0.0f;
    }

private:
    void DrawUI() {
        // Elixir bar
        DrawRectangle(10, 10, 200, 20, DARKGRAY);
        DrawRectangle(10, 10, 200 * ((float)playerElixir / MAX_ELIXIR), 20, PURPLE);
        DrawText(TextFormat("Elixir: %d/%d", playerElixir, MAX_ELIXIR), 15, 12, 15, WHITE);

        DrawUnitButtons();

        // Game info panel
        int panelY = 120;
        int panelWidth = 800;
        int panelHeight = 140;
        DrawRectangle(SCREEN_WIDTH/2 - panelWidth/2, panelY, panelWidth, panelHeight, Fade(DARKGRAY, 0.85f));
        DrawRectangleLines(SCREEN_WIDTH/2 - panelWidth/2, panelY, panelWidth, panelHeight, BLACK);
        
        int minutes = (int)gameTimer / 60;
        int seconds = (int)gameTimer % 60;
        Color timerColor = gameTimer < 30.0f ? RED : GREEN;
        DrawText("TIME LEFT", SCREEN_WIDTH/2 - 380, panelY + 25, 26, WHITE);
        DrawText(TextFormat("%02d:%02d", minutes, seconds), SCREEN_WIDTH/2 - 380, panelY + 60, 40, timerColor);
        
        DrawText("FREEZE ABILITY", SCREEN_WIDTH/2 - 120, panelY + 25, 26, WHITE);
        if (freezeAvailable) {
            DrawRectangle(SCREEN_WIDTH/2 - 120, panelY + 60, 240, 40, BLUE);
            DrawText("READY (Press F)", SCREEN_WIDTH/2 - 100, panelY + 70, 22, WHITE);
        } else {
            DrawRectangle(SCREEN_WIDTH/2 - 120, panelY + 60, 240, 40, DARKBLUE);
            DrawText(TextFormat("Cooldown: %.1fs", freezeCooldown), SCREEN_WIDTH/2 - 110, panelY + 70, 20, LIGHTGRAY);
        }
        
        // Wave info
        if (currentWave) {
            DrawText("CURRENT WAVE", SCREEN_WIDTH/2 + 140, panelY + 25, 26, WHITE);
            if (isBetweenWaves) {
                DrawText(TextFormat("Next Wave: %.1fs", betweenWavesTimer), SCREEN_WIDTH/2 + 140, panelY + 60, 22, ORANGE);
                DrawText("Prepare Your Defense!", SCREEN_WIDTH/2 + 140, panelY + 90, 18, YELLOW);
            } else {
                // Wave composition display
                string waveInfo = "Wave " + to_string(currentWave->waveNumber);
                DrawText(waveInfo.c_str(), SCREEN_WIDTH/2 + 140, panelY + 60, 28, YELLOW);
                
                if (currentUnitTypeIndex < currentWave->waveUnits.size()) {
                    string spawnInfo = TextFormat("Spawning: %s %d/%d", 
                    GetUnitTypeString(currentWave->waveUnits[currentUnitTypeIndex].type).c_str(),
                    unitsSpawnedForCurrentType, 
                    currentWave->waveUnits[currentUnitTypeIndex].count);
                    DrawText(spawnInfo.c_str(), SCREEN_WIDTH/2 + 140, panelY + 95, 18, WHITE);
                }
                
                // Show total units in wave
                int totalUnits = 0;
                for (const auto& waveUnit : currentWave->waveUnits) {
                    totalUnits += waveUnit.count;
                }
                DrawText(TextFormat("Total: %d units", totalUnits), SCREEN_WIDTH/2 + 140, panelY + 115, 16, LIGHTGRAY);
            }
        }

        // Instructions
        DrawText("Press 1-4 to spawn units: 1-Knight(3) 2-Archer(3) 3-Giant(5) 4-Wizard(4)", 10, SCREEN_HEIGHT - 30, 20, DARKBLUE);
    }

    void DrawUnitButtons() {
        UnitType types[] = { UnitType::KNIGHT, UnitType::ARCHER, UnitType::GIANT, UnitType::WIZARD };
        const char* names[] = { "Knight (1)", "Archer (2)", "Giant (3)", "Wizard (4)" };
        
        // Unit buttons 
        int startX = SCREEN_WIDTH/2 - 360;
        int buttonWidth = 180;
        int buttonHeight = 70;
        
        for (int i = 0; i < 4; i++) {
            UnitStats stats = GetUnitStats(types[i]);
            Color buttonColor = (playerElixir >= stats.cost) ? GREEN : RED;
            
            int buttonY = 45;
            
            DrawRectangle(startX + i * buttonWidth, buttonY, buttonWidth - 10, buttonHeight, buttonColor);
            DrawRectangleLines(startX + i * buttonWidth, buttonY, buttonWidth - 10, buttonHeight, BLACK);
            
            // Unit info
            DrawText(names[i], startX + i * buttonWidth + 10, buttonY + 5, 16, BLACK);
            DrawText(TextFormat("Cost: %d", stats.cost), startX + i * buttonWidth + 10, buttonY + 25, 14, BLACK);
            DrawText(TextFormat("HP: %d", stats.hp), startX + i * buttonWidth + 10, buttonY + 40, 12, BLACK);
            DrawText(TextFormat("DMG: %d", stats.damage), startX + i * buttonWidth + 90, buttonY + 40, 12, BLACK);
            
            if (stats.isRanged) {
                DrawText("RANGED", startX + i * buttonWidth + 10, buttonY + 55, 10, BLUE);
            } else {
                DrawText("MELEE", startX + i * buttonWidth + 10, buttonY + 55, 10, RED);
            }
        }
    }

    void HandleTowerAttacks() {
        // Player tower attacks with priority targeting
        if (playerTower.CanAttack()) {
            Unit* bestTarget = playerTower.GetBestTarget();
            if (bestTarget) {
                bestTarget->currentHP -= playerTower.damage;
                CreateAttackEffect(playerTower.position, bestTarget->position, BLUE);
                playerTower.ResetAttackTimer();
            }
        }

        // Enemy tower attacks with priority targeting
        if (enemyTower.CanAttack()) {
            Unit* bestTarget = enemyTower.GetBestTarget();
            if (bestTarget) {
                bestTarget->currentHP -= enemyTower.damage;
                CreateAttackEffect(enemyTower.position, bestTarget->position, RED);
                enemyTower.ResetAttackTimer();
            }
        }
    }

    void InitializeWaves() {
        // Wave progression
        vector<WaveUnit> wave1Units = { WaveUnit(UnitType::KNIGHT, 2), WaveUnit(UnitType::WIZARD, 1) };
        vector<WaveUnit> wave2Units = { WaveUnit(UnitType::GIANT, 1), WaveUnit(UnitType::KNIGHT, 1), WaveUnit(UnitType::ARCHER, 1) };
        vector<WaveUnit> wave3Units = { WaveUnit(UnitType::ARCHER, 2), WaveUnit(UnitType::WIZARD, 1) };
        vector<WaveUnit> wave4Units = { WaveUnit(UnitType::GIANT, 1), WaveUnit(UnitType::WIZARD, 2) };
        vector<WaveUnit> wave5Units = { WaveUnit(UnitType::KNIGHT, 2), WaveUnit(UnitType::ARCHER, 1), WaveUnit(UnitType::GIANT, 1) };
        vector<WaveUnit> wave6Units = { WaveUnit(UnitType::WIZARD, 1), WaveUnit(UnitType::ARCHER, 2), WaveUnit(UnitType::KNIGHT, 1) };
        vector<WaveUnit> wave7Units = { WaveUnit(UnitType::GIANT, 1), WaveUnit(UnitType::WIZARD, 1), WaveUnit(UnitType::ARCHER, 2) };
        vector<WaveUnit> wave8Units = { WaveUnit(UnitType::KNIGHT, 2), WaveUnit(UnitType::GIANT, 1), WaveUnit(UnitType::WIZARD, 1) };
        //// Create waves with their units, spawn rate, and cooldown
        waveList = new GameWave(1, wave1Units, 4.0f, 10.0f);
        GameWave* wave2 = new GameWave(2, wave2Units, 3.5f, 10.0f);
        GameWave* wave3 = new GameWave(3, wave3Units, 3.5f, 10.0f);
        GameWave* wave4 = new GameWave(4, wave4Units, 3.5f, 10.0f);
        GameWave* wave5 = new GameWave(5, wave5Units, 3.0f, 10.0f);
        GameWave* wave6 = new GameWave(6, wave6Units, 3.0f, 10.0f);
        GameWave* wave7 = new GameWave(7, wave7Units, 2.5f, 10.0f);
        GameWave* wave8 = new GameWave(8, wave8Units, 2.5f, 10.0f);
        
        waveList->nextWave = wave2;
        wave2->nextWave = wave3;
        wave3->nextWave = wave4;
        wave4->nextWave = wave5;
        wave5->nextWave = wave6;
        wave6->nextWave = wave7;
        wave7->nextWave = wave8;
        
        currentWave = waveList;
    }

    void HandleWaveProgression(float deltaTime) {
        if (!currentWave || gameOver) return;
        
        if (isBetweenWaves) {
            betweenWavesTimer -= deltaTime;
            if (betweenWavesTimer <= 0) {
                isBetweenWaves = false;
                waveSpawnTimer = 0.0f;
                currentUnitTypeIndex = 0;
                unitsSpawnedForCurrentType = 0;
            }
            return;
        }
        
        waveSpawnTimer += deltaTime;
        
        if (currentUnitTypeIndex < currentWave->waveUnits.size()) {
            WaveUnit& currentUnitType = currentWave->waveUnits[currentUnitTypeIndex];
            
            if (waveSpawnTimer >= currentWave->spawnRate && 
                unitsSpawnedForCurrentType < currentUnitType.count) {
                
                units.push_back(make_unique<Unit>(currentUnitType.type, false));
                unitsSpawnedForCurrentType++;
                waveSpawnTimer = 0.0f;
                
                // Check if spawning unit finished in a wave 
                if (unitsSpawnedForCurrentType >= currentUnitType.count) {
                    // Move to next unit type 
                    currentUnitTypeIndex++;
                    unitsSpawnedForCurrentType = 0;
                    waveSpawnTimer = 0.0f;
                }
            }
        } else {
            // Units spawned in the wave
            isBetweenWaves = true;
            betweenWavesTimer = currentWave->waveCooldown;
            
            if (currentWave->nextWave) {
                currentWave = currentWave->nextWave;
            } else {
                currentWave = waveList;
                GameWave* wave = waveList;
                while (wave) {
                    wave->spawnRate = max(2.0f, wave->spawnRate * 0.9f);
                    wave = wave->nextWave;
                }
            }
        }
    }

    string GetUnitTypeString(UnitType type) {
        switch(type) {
            case UnitType::KNIGHT: return "Knight";
            case UnitType::ARCHER: return "Archer";
            case UnitType::GIANT: return "Giant";
            case UnitType::WIZARD: return "Wizard";
            default: return "Unknown";
        }
    }

    UnitStats GetUnitStats(UnitType type) {
        switch(type) {
            case UnitType::KNIGHT:
                return UnitStats{"Knight", 3, 300, 60, 80.0f, 1.2f, 40.0f, false, BLUE, 25};
            case UnitType::ARCHER:
                return UnitStats{"Archer", 3, 150, 40, 60.0f, 1.5f, 150.0f, true, GREEN, 20};
            case UnitType::GIANT:
                return UnitStats{"Giant", 5, 1000, 80, 40.0f, 2.0f, 50.0f, false, GRAY, 35};
            case UnitType::WIZARD:
                return UnitStats{"Wizard", 4, 180, 70, 50.0f, 2.5f, 120.0f, true, PURPLE, 22};
            default:
                return UnitStats{"Unknown", 0, 0, 0, 0.0f, 0.0f, 0.0f, false, WHITE, 0};
        }
    }
};

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Advanced Tower Defense - DSA Project");
    SetTargetFPS(60);
	InitAudioDevice();
    Game game;
	Music backgroundMusic = LoadMusicStream("background_music.ogg");
    
    SetMusicVolume(backgroundMusic, 1.0f);
    
    // For music playing
    PlayMusicStream(backgroundMusic);
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
		UpdateMusicStream(backgroundMusic);
        // keys for Spawning units
        if (game.currentState == GameState::PLAYING) {
            if (IsKeyPressed(KEY_ONE)) game.SpawnUnit(UnitType::KNIGHT);
            if (IsKeyPressed(KEY_TWO)) game.SpawnUnit(UnitType::ARCHER);
            if (IsKeyPressed(KEY_THREE)) game.SpawnUnit(UnitType::GIANT);
            if (IsKeyPressed(KEY_FOUR)) game.SpawnUnit(UnitType::WIZARD);
            
            // Freeze ability
            if (IsKeyPressed(KEY_F)) {
                game.ActivateFreeze();
            }
        }
        // Exit game
        if (IsKeyPressed(KEY_ESCAPE)) {
            break;
        }
        game.Update(deltaTime);
        BeginDrawing();
        game.Draw();
        EndDrawing();
    }
	UnloadMusicStream(backgroundMusic);
    CloseWindow();
    return 0;
}