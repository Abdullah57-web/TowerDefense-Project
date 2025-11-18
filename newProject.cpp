#include "raylib.h"
#include <vector>
#include <list>
#include <queue>
#include <algorithm>
#include <memory>
#include <cmath>
#include <functional>
#include <string>

// Game Constants
const int SCREEN_WIDTH = 1200;
const int SCREEN_HEIGHT = 800;
const int GROUND_HEIGHT = 100;
const int LANE_Y = SCREEN_HEIGHT / 2;

// Tower constants - INCREASED HP
const int TOWER_HP = 3500;  // Increased from 2000
const int TOWER_DAMAGE = 50;
const float TOWER_ATTACK_RATE = 1.0f;
const float TOWER_RANGE = 300.0f;

// Game States
enum class GameState {
    START_SCREEN,
    PLAYING,
    GAME_OVER
};

// Unit types
enum class UnitType {
    KNIGHT,
    ARCHER, 
    GIANT,
    WIZARD
};

// Unit stats structure
struct UnitStats {
    std::string name;
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

// Wave unit structure for mixed waves
struct WaveUnit {
    UnitType type;
    int count;
    
    WaveUnit(UnitType t, int c) : type(t), count(c) {}
};

// Unit class declaration
class Unit {
public:
    UnitType type;
    Vector2 position;
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
    
    // Freeze effect
    bool isFrozen;
    float freezeTimer;
    
    // Pathfinding properties
    std::list<Vector2> path;
    Vector2 currentTargetPos;

    Unit(UnitType unitType, bool player);
    void Update(float deltaTime, std::list<std::unique_ptr<Unit>>& allUnits);
    void Draw();
    void FindTargetWithPriority(std::list<std::unique_ptr<Unit>>& allUnits);
    void Attack(Unit* targetUnit, std::list<std::unique_ptr<Unit>>& allUnits);

private:
    void generatePath();
    void FollowPath(float deltaTime);
    void DrawPath();
    float CalculateDistance(Vector2 a, Vector2 b);
    std::string getUnitTypeString(UnitType type);
    UnitStats getUnitStats(UnitType type);
};

// Pathfinding Node for advanced movement
struct PathNode {
    int x;
    float cost;
    bool operator>(const PathNode& other) const {
        return cost > other.cost;
    }
};

// Priority comparison for tower targeting
struct TowerTargetPriority {
    bool operator()(const std::pair<Unit*, float>& a, const std::pair<Unit*, float>& b) {
        // Priority: Closest units first, then low HP units
        if (fabs(a.second - b.second) < 10.0f) {
            return a.first->currentHP > b.first->currentHP; // Lower HP first
        }
        return a.second > b.second; // Closer first
    }
};

// Wave management using linked list (renamed to avoid conflict)
struct GameWave {
    int waveNumber;
    std::vector<WaveUnit> waveUnits; // Mixed units in wave
    float spawnRate;
    float waveCooldown; // Time between waves
    GameWave* nextWave;
    
    GameWave(int num, const std::vector<WaveUnit>& units, float rate, float cooldown = 10.0f) 
        : waveNumber(num), waveUnits(units), spawnRate(rate), waveCooldown(cooldown), nextWave(nullptr) {}
};

// Unit class implementation
Unit::Unit(UnitType unitType, bool player) {
    type = unitType;
    isPlayer = player;
    isAlive = true;
    target = nullptr;
    isFrozen = false;
    freezeTimer = 0.0f;
    
    // Set stats based on unit type
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
    
    // Set initial position
    if (isPlayer) {
        position = { 150.0f, LANE_Y };
        currentTargetPos = { (float)SCREEN_WIDTH - 50.0f, LANE_Y }; // Enemy tower
    } else {
        position = { (float)SCREEN_WIDTH - 150.0f, LANE_Y };
        currentTargetPos = { 50.0f, LANE_Y }; // Player tower
    }
    
    // Generate initial path using A* style (simplified for single lane)
    generatePath();
    
    attackTimer = 0.0f;
}

void Unit::Update(float deltaTime, std::list<std::unique_ptr<Unit>>& allUnits) {
    if (!isAlive) return;
    
    // Handle freeze effect
    if (isFrozen) {
        freezeTimer -= deltaTime;
        if (freezeTimer <= 0) {
            isFrozen = false;
        }
        return; // Frozen units don't move or attack
    }
    
    // Find target using priority-based searching
    if (!target || !target->isAlive) {
        FindTargetWithPriority(allUnits);
    }
    
    // If has target and in range, attack
    if (target && target->isAlive) {
        float distanceToTarget = CalculateDistance(position, target->position);
        
        if (distanceToTarget <= attackRange) {
            // In range, stop moving and attack
            attackTimer += deltaTime;
            if (attackTimer >= attackRate) {
                Attack(target, allUnits);
                attackTimer = 0.0f;
            }
        } else {
            // Move toward target using pathfinding
            FollowPath(deltaTime);
            attackTimer = 0.0f;
        }
    } else {
        // No target, move toward enemy tower using pathfinding
        FollowPath(deltaTime);
    }
}

void Unit::Draw() {
    if (!isAlive) return;
    
    // Draw unit as circle with health bar
    Color drawColor = color;
    if (isFrozen) {
        drawColor = BLUE; // Blue color when frozen
        DrawCircle(position.x, position.y, size + 5, Fade(SKYBLUE, 0.3f)); // Frost aura
    }
    
    DrawCircle(position.x, position.y, size, drawColor);
    
    // Draw border based on player/enemy - NEW FEATURE
    if (isPlayer) {
        DrawCircleLines(position.x, position.y, size + 3, BLUE); // Blue border for player units
    } else {
        DrawCircleLines(position.x, position.y, size + 3, RED); // Red border for enemy units
    }
    
    // Draw range indicator for ranged units
    if (isRanged) {
        DrawCircleLines(position.x, position.y, attackRange, Fade(color, 0.3f));
    }
    
    // Health bar
    float healthPercent = (float)currentHP / (float)maxHP;
    DrawRectangle(position.x - size, position.y - size - 15, size * 2, 5, RED);
    DrawRectangle(position.x - size, position.y - size - 15, size * 2 * healthPercent, 5, GREEN);
    
    // Draw unit type indicator
    std::string typeStr = getUnitTypeString(type);
    DrawText(typeStr.c_str(), position.x - 10, position.y - 8, 12, BLACK);
    
    // Draw freeze indicator
    if (isFrozen) {
        DrawText("FROZEN", position.x - 15, position.y + size + 5, 10, BLUE);
    }
    
    // Draw target line if has target
    if (target && target->isAlive) {
        DrawLine(position.x, position.y, target->position.x, target->position.y, Fade(RED, 0.5f));
    }
    
    // Draw path (for visualization)
    DrawPath();
}

// Priority-based target finding using sorting
void Unit::FindTargetWithPriority(std::list<std::unique_ptr<Unit>>& allUnits) {
    std::vector<std::pair<Unit*, float>> potentialTargets;
    
    // Search for all potential targets
    for (auto& unit : allUnits) {
        if (unit->isAlive && unit->isPlayer != isPlayer) {
            float distance = CalculateDistance(position, unit->position);
            if (distance <= attackRange * 1.5f) {
                potentialTargets.push_back({unit.get(), distance});
            }
        }
    }
    
    // Sort targets by priority: closest first, then lowest HP
    if (!potentialTargets.empty()) {
        std::sort(potentialTargets.begin(), potentialTargets.end(),
            [](const auto& a, const auto& b) {
                if (fabs(a.second - b.second) < 10.0f) {
                    return a.first->currentHP < b.first->currentHP; // Lower HP first
                }
                return a.second < b.second; // Closer first
            });
        
        target = potentialTargets[0].first;
    } else {
        target = nullptr;
    }
}

void Unit::Attack(Unit* targetUnit, std::list<std::unique_ptr<Unit>>& allUnits) {
    if (!targetUnit || !targetUnit->isAlive) return;
    
    targetUnit->currentHP -= damage;
    
    // Area damage for wizard
    if (type == UnitType::WIZARD) {
        for (auto& unit : allUnits) {
            if (unit->isAlive && unit.get() != targetUnit && unit->isPlayer != isPlayer) {
                float distance = CalculateDistance(unit->position, targetUnit->position);
                if (distance < 60.0f) {
                    unit->currentHP -= damage / 2;
                }
            }
        }
    }
    
    if (targetUnit->currentHP <= 0) {
        targetUnit->isAlive = false;
        target = nullptr;
    }
}

// Simplified A* pathfinding for single lane
void Unit::generatePath() {
    path.clear();
    
    if (isPlayer) {
        // Player units move right toward enemy tower
        for (int x = (int)position.x; x < SCREEN_WIDTH - 50; x += 50) {
            path.push_back({(float)x, LANE_Y});
        }
    } else {
        // Enemy units move left toward player tower
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
    
    float distance = sqrt(direction.x * direction.x + direction.y * direction.y);
    
    if (distance < 5.0f) {
        // Reached current waypoint, move to next
        path.pop_front();
        if (!path.empty()) {
            currentTargetPos = path.front();
        }
    } else {
        // Move toward current waypoint
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

std::string Unit::getUnitTypeString(UnitType type) {
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

// Projectile class
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

    void Update(float deltaTime) {
        progress += deltaTime * 3.0f;
        if (progress >= 1.0f) {
            active = false;
        }
    }

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

// Tower class with priority queue targeting
class Tower {
public:
    Vector2 position;
    int currentHP;
    int maxHP;
    int damage;
    float attackRate;
    float attackTimer;
    bool isPlayer;
    bool isAlive;
    
    // Priority queue for target selection
    std::priority_queue<std::pair<Unit*, float>, 
                       std::vector<std::pair<Unit*, float>>,
                       TowerTargetPriority> targetQueue;

    Tower(bool player) {
        isPlayer = player;
        maxHP = TOWER_HP;  // Now uses the increased HP constant
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

    void Update(float deltaTime, std::list<std::unique_ptr<Unit>>& units) {
        if (!isAlive) return;
        attackTimer += deltaTime;
        
        // Update target queue
        UpdateTargetQueue(units);
    }

    void Draw() {
        if (!isAlive) return;
        
        Color towerColor = isPlayer ? BLUE : RED;
        Color darkTowerColor = isPlayer ? DARKBLUE : MAROON;  // Fixed: Use MAROON instead of DARKRED
        Color lightTowerColor = isPlayer ? SKYBLUE : PINK;
        
        // Draw tower base (wider and more detailed)
        DrawRectangle(position.x - 50, position.y - 40, 100, 80, darkTowerColor);
        DrawRectangle(position.x - 45, position.y - 35, 90, 70, towerColor);
        
        // Draw tower middle section
        DrawRectangle(position.x - 35, position.y - 70, 70, 40, darkTowerColor);
        DrawRectangle(position.x - 30, position.y - 65, 60, 30, towerColor);
        
        // Draw tower top section
        DrawRectangle(position.x - 25, position.y - 100, 50, 40, darkTowerColor);
        DrawRectangle(position.x - 20, position.y - 95, 40, 30, lightTowerColor);
        
        // Draw tower flags
        if (isPlayer) {
            // Blue flag for player tower
            DrawRectangle(position.x + 25, position.y - 110, 15, 25, BLUE);
            DrawRectangle(position.x + 25, position.y - 115, 20, 5, DARKBLUE);
        } else {
            // Red flag for enemy tower
            DrawRectangle(position.x - 40, position.y - 110, 15, 25, RED);
            DrawRectangle(position.x - 45, position.y - 115, 20, 5, MAROON);  // Fixed: Use MAROON
        }
        
        // Draw tower windows
        DrawRectangle(position.x - 8, position.y - 85, 16, 12, DARKGRAY);
        DrawRectangle(position.x - 5, position.y - 82, 10, 6, YELLOW);
        
        // Draw tower door
        DrawRectangle(position.x - 15, position.y - 15, 30, 35, darkTowerColor);
        DrawRectangle(position.x - 12, position.y - 12, 24, 29, BROWN);
        
        // Health bar
        float healthPercent = (float)currentHP / (float)maxHP;
        DrawRectangle(position.x - 50, position.y - 120, 100, 10, RED);
        DrawRectangle(position.x - 50, position.y - 120, 100 * healthPercent, 10, GREEN);
        
        std::string label = isPlayer ? "Your Tower" : "Enemy Tower";
        DrawText(label.c_str(), position.x - 40, position.y - 135, 12, BLACK);
    }

    bool CanAttack() {
        return attackTimer >= attackRate;
    }

    void ResetAttackTimer() {
        attackTimer = 0.0f;
    }

    void UpdateTargetQueue(std::list<std::unique_ptr<Unit>>& units) {
        // Clear and rebuild priority queue
        targetQueue = std::priority_queue<std::pair<Unit*, float>, 
                                         std::vector<std::pair<Unit*, float>>,
                                         TowerTargetPriority>();
        
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

// Game class with wave progression using linked list
class Game {
public:
    GameState currentState;
    Tower playerTower;
    Tower enemyTower;
    std::list<std::unique_ptr<Unit>> units;  // Changed to linked list
    std::vector<Projectile> projectiles;
    
    // Freeze ability
    bool freezeAvailable;
    float freezeCooldown;
    const float FREEZE_DURATION = 5.0f;
    const float FREEZE_COOLDOWN = 30.0f; // 30 seconds cooldown
    
    int playerElixir;
    int enemyElixir;
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
    
    // Game timer (2 minutes)
    float gameTimer;
    const float GAME_TIME_LIMIT = 120.0f; // 2 minutes in seconds
    
    bool gameOver;
    std::string winner;

    Game() : playerTower(true), enemyTower(false) {
        currentState = GameState::START_SCREEN; // Start with start screen
        playerElixir = 5;
        enemyElixir = 5;
        elixirTimer = 0.0f;
        gameOver = false;
        winner = "";
        
        // Initialize freeze ability
        freezeAvailable = true;
        freezeCooldown = 0.0f;
        
        // Initialize game timer
        gameTimer = GAME_TIME_LIMIT;
        
        // Initialize wave progression linked list
        InitializeWaves();
        waveSpawnTimer = 0.0f;
        currentUnitTypeIndex = 0;
        unitsSpawnedForCurrentType = 0;
        isBetweenWaves = false;
        betweenWavesTimer = 0.0f;
    }

    ~Game() {
        // Clean up wave linked list
        GameWave* current = waveList;
        while (current) {
            GameWave* next = current->nextWave;
            delete current;
            current = next;
        }
    }

    void Update(float deltaTime) {
        if (currentState == GameState::START_SCREEN) {
            // Check for Enter key to start the game
            if (IsKeyPressed(KEY_ENTER)) {
                currentState = GameState::PLAYING;
            }
            return;
        }
        
        if (currentState == GameState::GAME_OVER) {
            // Check for R key to restart
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

        // Update freeze ability cooldown
        if (!freezeAvailable) {
            freezeCooldown -= deltaTime;
            if (freezeCooldown <= 0) {
                freezeAvailable = true;
                freezeCooldown = 0.0f;
            }
        }

        // Update game timer
        gameTimer -= deltaTime;
        if (gameTimer <= 0) {
            gameTimer = 0;
            // Time's up - Player loses if enemy tower isn't destroyed
            gameOver = true;
            winner = "Draw";
        }

        // Update elixir
        elixirTimer += deltaTime;
        if (elixirTimer >= ELIXIR_RATE) {
            if (playerElixir < MAX_ELIXIR) playerElixir++;
            if (enemyElixir < MAX_ELIXIR) enemyElixir++;
            elixirTimer = 0.0f;
        }

        // Update towers
        playerTower.Update(deltaTime, units);
        enemyTower.Update(deltaTime, units);

        // Update units using linked list iteration
        for (auto it = units.begin(); it != units.end(); ) {
            if ((*it)->isAlive) {
                (*it)->Update(deltaTime, units);
                
                // Check if unit reached enemy tower
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
                // Remove dead units from linked list
                it = units.erase(it);
            }
        }

        // Update projectiles
        for (auto& projectile : projectiles) {
            projectile.Update(deltaTime);
        }

        // Remove inactive projectiles
        projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
            [](const Projectile& proj) { return !proj.active; }),
            projectiles.end());

        // Handle tower attacks with priority targeting
        HandleTowerAttacks();

        // Wave-based enemy spawning using linked list - Enemy tower sends waves
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
        
        // Draw single lane
        DrawRectangle(0, LANE_Y - 75, SCREEN_WIDTH, 150, DARKGRAY);
        
        // Draw center line
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

        // Draw UI
        DrawUI();
    }

    void DrawStartScreen() {
        // Draw background
        DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, DARKBLUE, BLUE);
        
        // Draw title
        DrawText("TOWER DEFENSE", SCREEN_WIDTH/2 - MeasureText("TOWER DEFENSE", 80)/2, 100, 80, YELLOW);
        
        // Draw game description
        DrawText("Defend your tower against enemy waves for 2 minutes!", SCREEN_WIDTH/2 - MeasureText("Defend your tower against enemy waves for 2 minutes!", 30)/2, 220, 30, WHITE);
        
        // Draw unit information - Fixed layout to prevent overlapping
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
        // Removed the border color explanation lines
        
        // Draw start instruction
        DrawText("PRESS ENTER TO START", SCREEN_WIDTH/2 - MeasureText("PRESS ENTER TO START", 50)/2, 550, 50, GREEN);
        
        // Draw footer
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
            units.push_back(std::make_unique<Unit>(type, true));
            playerElixir -= stats.cost;
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
        
        // Start cooldown
        freezeAvailable = false;
        freezeCooldown = FREEZE_COOLDOWN;
        
        // Create freeze visual effect
        CreateFreezeEffect();
    }

    void CreateFreezeEffect() {
        // Create multiple projectiles for visual effect
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
        enemyElixir = 5;
        elixirTimer = 0.0f;
        gameOver = false;
        winner = "";
        gameTimer = GAME_TIME_LIMIT;
        
        // Reset freeze ability
        freezeAvailable = true;
        freezeCooldown = 0.0f;
        
        // Reset wave progression
        currentWave = waveList;
        waveSpawnTimer = 0.0f;
        currentUnitTypeIndex = 0;
        unitsSpawnedForCurrentType = 0;
        isBetweenWaves = false;
        betweenWavesTimer = 0.0f;
    }

private:
    void DrawUI() {
        // Draw only player elixir bar at top (enemy elixir bar removed)
        DrawRectangle(10, 10, 200, 20, DARKGRAY);
        DrawRectangle(10, 10, 200 * ((float)playerElixir / MAX_ELIXIR), 20, PURPLE);
        DrawText(TextFormat("Elixir: %d/%d", playerElixir, MAX_ELIXIR), 15, 12, 15, WHITE);

        // Enemy elixir bar code has been removed

        // Draw unit buttons at top - Made bigger and moved to avoid elixir bars
        DrawUnitButtons();

        // Draw larger game info panel higher up above the game path
        int panelY = 120;
        int panelWidth = 800;
        int panelHeight = 140;
        DrawRectangle(SCREEN_WIDTH/2 - panelWidth/2, panelY, panelWidth, panelHeight, Fade(DARKGRAY, 0.85f));
        DrawRectangleLines(SCREEN_WIDTH/2 - panelWidth/2, panelY, panelWidth, panelHeight, BLACK);
        
        // Draw timer in the left section of the panel
        int minutes = (int)gameTimer / 60;
        int seconds = (int)gameTimer % 60;
        Color timerColor = gameTimer < 30.0f ? RED : GREEN;
        DrawText("TIME LEFT", SCREEN_WIDTH/2 - 380, panelY + 25, 26, WHITE);
        DrawText(TextFormat("%02d:%02d", minutes, seconds), SCREEN_WIDTH/2 - 380, panelY + 60, 40, timerColor);
        
        // Draw freeze ability in the center section of panel
        DrawText("FREEZE ABILITY", SCREEN_WIDTH/2 - 120, panelY + 25, 26, WHITE);
        if (freezeAvailable) {
            DrawRectangle(SCREEN_WIDTH/2 - 120, panelY + 60, 240, 40, BLUE);
            DrawText("READY (Press F)", SCREEN_WIDTH/2 - 100, panelY + 70, 22, WHITE);
        } else {
            DrawRectangle(SCREEN_WIDTH/2 - 120, panelY + 60, 240, 40, DARKBLUE);
            DrawText(TextFormat("Cooldown: %.1fs", freezeCooldown), SCREEN_WIDTH/2 - 110, panelY + 70, 20, LIGHTGRAY);
        }
        
        // Draw wave info in the right section of panel
        if (currentWave) {
            DrawText("CURRENT WAVE", SCREEN_WIDTH/2 + 140, panelY + 25, 26, WHITE);
            if (isBetweenWaves) {
                DrawText(TextFormat("Next Wave: %.1fs", betweenWavesTimer), SCREEN_WIDTH/2 + 140, panelY + 60, 22, ORANGE);
                DrawText("Prepare Your Defense!", SCREEN_WIDTH/2 + 140, panelY + 90, 18, YELLOW);
            } else {
                // Show wave composition
                std::string waveInfo = "Wave " + std::to_string(currentWave->waveNumber);
                DrawText(waveInfo.c_str(), SCREEN_WIDTH/2 + 140, panelY + 60, 28, YELLOW);
                
                // Show current spawning progress
                if (currentUnitTypeIndex < currentWave->waveUnits.size()) {
                    std::string spawnInfo = TextFormat("Spawning: %s %d/%d", 
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

        // Draw instructions at bottom
        DrawText("Press 1-4 to spawn units: 1-Knight(3) 2-Archer(3) 3-Giant(5) 4-Wizard(4)", 10, SCREEN_HEIGHT - 30, 20, DARKBLUE);
    }

    void DrawUnitButtons() {
        UnitType types[] = { UnitType::KNIGHT, UnitType::ARCHER, UnitType::GIANT, UnitType::WIZARD };
        const char* names[] = { "Knight (1)", "Archer (2)", "Giant (3)", "Wizard (4)" };
        
        // Unit buttons at top center - Made bigger and positioned to avoid elixir bars
        int startX = SCREEN_WIDTH/2 - 360;
        int buttonWidth = 180; // Increased width
        int buttonHeight = 70; // Increased height
        
        for (int i = 0; i < 4; i++) {
            UnitStats stats = GetUnitStats(types[i]);
            Color buttonColor = (playerElixir >= stats.cost) ? GREEN : RED;
            
            // Position buttons lower to avoid elixir bars
            int buttonY = 45; // Moved down from 40
            
            DrawRectangle(startX + i * buttonWidth, buttonY, buttonWidth - 10, buttonHeight, buttonColor);
            DrawRectangleLines(startX + i * buttonWidth, buttonY, buttonWidth - 10, buttonHeight, BLACK);
            
            // Draw unit info with HP and damage
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
        // Create balanced wave progression with MAX 4 units per wave
        std::vector<WaveUnit> wave1Units = { WaveUnit(UnitType::KNIGHT, 2), WaveUnit(UnitType::WIZARD, 1) }; // Total: 3
        std::vector<WaveUnit> wave2Units = { WaveUnit(UnitType::GIANT, 1), WaveUnit(UnitType::KNIGHT, 1), WaveUnit(UnitType::ARCHER, 1) }; // Total: 3
        std::vector<WaveUnit> wave3Units = { WaveUnit(UnitType::ARCHER, 2), WaveUnit(UnitType::WIZARD, 1) }; // Total: 3
        std::vector<WaveUnit> wave4Units = { WaveUnit(UnitType::GIANT, 1), WaveUnit(UnitType::WIZARD, 2) }; // Total: 3
        std::vector<WaveUnit> wave5Units = { WaveUnit(UnitType::KNIGHT, 2), WaveUnit(UnitType::ARCHER, 1), WaveUnit(UnitType::GIANT, 1) }; // Total: 4
        std::vector<WaveUnit> wave6Units = { WaveUnit(UnitType::WIZARD, 1), WaveUnit(UnitType::ARCHER, 2), WaveUnit(UnitType::KNIGHT, 1) }; // Total: 4
        std::vector<WaveUnit> wave7Units = { WaveUnit(UnitType::GIANT, 1), WaveUnit(UnitType::WIZARD, 1), WaveUnit(UnitType::ARCHER, 2) }; // Total: 4
        std::vector<WaveUnit> wave8Units = { WaveUnit(UnitType::KNIGHT, 2), WaveUnit(UnitType::GIANT, 1), WaveUnit(UnitType::WIZARD, 1) }; // Total: 4
        
        waveList = new GameWave(1, wave1Units, 4.0f, 10.0f); // Slower spawn rate
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
        
        // Check if we have more unit types to spawn in this wave
        if (currentUnitTypeIndex < currentWave->waveUnits.size()) {
            WaveUnit& currentUnitType = currentWave->waveUnits[currentUnitTypeIndex];
            
            // Enemy waves spawn automatically without elixir cost
            if (waveSpawnTimer >= currentWave->spawnRate && 
                unitsSpawnedForCurrentType < currentUnitType.count) {
                
                // Spawn enemy unit for current wave (no elixir cost for enemy waves)
                units.push_back(std::make_unique<Unit>(currentUnitType.type, false));
                unitsSpawnedForCurrentType++;
                waveSpawnTimer = 0.0f;
                
                // Check if we finished this unit type
                if (unitsSpawnedForCurrentType >= currentUnitType.count) {
                    // Move to next unit type in this wave
                    currentUnitTypeIndex++;
                    unitsSpawnedForCurrentType = 0;
                    waveSpawnTimer = 0.0f;
                }
            }
        } else {
            // All unit types in this wave have been spawned
            // Start cooldown between waves
            isBetweenWaves = true;
            betweenWavesTimer = currentWave->waveCooldown;
            
            // Move to next wave after cooldown
            if (currentWave->nextWave) {
                currentWave = currentWave->nextWave;
            } else {
                // If no more waves, loop back to first wave but with increased difficulty
                currentWave = waveList;
                
                // Increase difficulty for subsequent loops by reducing spawn times slightly
                GameWave* wave = waveList;
                while (wave) {
                    wave->spawnRate = std::max(2.0f, wave->spawnRate * 0.9f); // Faster but not too fast
                    wave = wave->nextWave;
                }
            }
        }
    }

    std::string GetUnitTypeString(UnitType type) {
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
    
    // Set music volume (0.0 to 1.0)
    SetMusicVolume(backgroundMusic, 1.0f);
    
    // Start playing the music
    PlayMusicStream(backgroundMusic);
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
		UpdateMusicStream(backgroundMusic);
        // Handle input
        if (game.currentState == GameState::PLAYING) {
            // Unit spawning
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
