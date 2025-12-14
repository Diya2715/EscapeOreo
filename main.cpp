#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <iostream>
#include <algorithm> // for std::min / std::max
#include <cstdlib>  // ADDED: rand(), srand()
#include <ctime>    // ADDED: time() for srand seed


enum GameState {
    MENU,
    PLAYING,
    PAUSED,
    LEVEL_COMPLETE,
    GAME_OVER
};

// ADDED: which menu screen we are on
enum MenuPage {
    MAIN_MENU,
    MAP_PAGE,
    SETTINGS_PAGE,
    INSTRUCTIONS_PAGE,
    SHOP_PAGE
};

struct Platform {
    sf::RectangleShape shape;
    const sf::Texture* texture;   // NEW
    bool breakable;
    float breakTimer;

    // Colour-based platform (used for rock blocks etc.)
    Platform(float x, float y, float w, float h, sf::Color color, bool canBreak = false)
        : texture(nullptr), breakable(canBreak), breakTimer(0)
    {
        shape.setPosition(x, y);
        shape.setSize(sf::Vector2f(w, h));

        shape.setOutlineThickness(2);
        shape.setOutlineColor(sf::Color(
            0, 0, 0, 120  // or whatever looks good with your texture
        ));

        shape.setFillColor(color);
    }

    // Texture-based platform (used for iceBlock.png etc.)
    Platform(float x, float y, float w, float h, const sf::Texture* tex, bool canBreak = false)
        : texture(tex), breakable(canBreak), breakTimer(0)
    {
        shape.setPosition(x, y);
        shape.setSize(sf::Vector2f(w, h));
        if (texture) {
            shape.setTexture(texture);
        }
    }
};


struct Diamond {
    sf::Sprite sprite;
    const sf::Texture* texture;
    bool collected;
    float animOffset;
    sf::Vector2f basePos;

    Diamond(float x, float y, const sf::Texture* tex)
        : texture(tex), collected(false), animOffset(0.f), basePos(x, y)
    {
        if (texture) {
            sprite.setTexture(*texture);

            // Resize diamond to a nice size (similar to old height)
            sf::FloatRect b = sprite.getLocalBounds();
            float targetHeight = 26.f;        // tweak if you want bigger/smaller
            float scale = targetHeight / b.height;
            sprite.setScale(scale, scale);

            sprite.setPosition(x, y);
        }
    }

    void update() {
        animOffset += 0.05f;
        float yOffset = std::sin(animOffset) * 5;
        sprite.setPosition(basePos.x, basePos.y + yOffset);
    }

    sf::FloatRect getBounds() const {
        return sprite.getGlobalBounds();
    }

    void draw(sf::RenderWindow& window) {
        if (!collected)
            window.draw(sprite);
    }
};




struct Enemy {
    sf::Sprite sprite;
    sf::Vector2f position;
    float speed;
    int direction;
    float minX, maxX;

    // animation
    const std::vector<sf::Texture>* textures;
    int currentFrame;
    float frameTimer;         // counts frames/time between swaps

    Enemy(float x, float y, float spd, float min, float max,
        const std::vector<sf::Texture>* texPtr)
        : position(x, y),
        speed(spd),
        direction(1),
        minX(min),
        maxX(max),
        textures(texPtr),
        currentFrame(0),
        frameTimer(0.f)
    {
        if (textures && !textures->empty()) {
            sprite.setTexture((*textures)[0]);
        }
        sprite.setPosition(position);
    }

    void update() {
        // move horizontally like before
        position.x += direction * speed;
        if (position.x <= minX || position.x >= maxX) {
            direction *= -1;
        }

        // small vertical bob
        position.y += std::sin(position.x * 0.01f) * 0.2f;
        sprite.setPosition(position);

        // flip sprite when changing direction
        if (textures && !textures->empty()) {
            float scaleX = (direction > 0) ? -1.f : 1.f;
            sprite.setScale(scaleX, 1.f);
        }

        // animation: cycle through the 9 images
        frameTimer += 0.15f;          // tweak speed if you want
        if (frameTimer >= 1.f) {      // every ~1 frame here because we use arbitrary units
            frameTimer = 0.f;
            if (textures && !textures->empty()) {
                currentFrame = (currentFrame + 1) % static_cast<int>(textures->size());
                sprite.setTexture((*textures)[currentFrame], true);
            }
        }
    }

    void draw(sf::RenderWindow& window) {
        window.draw(sprite);
    }

    sf::FloatRect getBounds() const {
        return sprite.getGlobalBounds();
    }
};


struct Hammer {
    sf::Sprite sprite;
    const sf::Texture* texture;
    sf::Vector2f position;
    bool collected;

    Hammer(float x, float y, const sf::Texture* tex)
        : texture(tex), position(x, y), collected(false)
    {
        if (texture) {
            sprite.setTexture(*texture);

            // --- Resize so the axe is only slightly bigger than before ---
            sf::FloatRect bounds = sprite.getLocalBounds();
            float targetHeight = 65.f;              // a bit bigger than old ~25px
            float scale = targetHeight / bounds.height;
            sprite.setScale(scale, scale);

            // Position after scaling (x,y is top-left like before)
            sprite.setPosition(position);
        }
    }


    sf::FloatRect getBounds() const {
        return sprite.getGlobalBounds();
    }

    void draw(sf::RenderWindow& window) {
        window.draw(sprite);
    }
};


struct Boulder {
    sf::RectangleShape shape;
    std::vector<sf::RectangleShape> cracks;
    bool broken;

    Boulder(float x, float y) : broken(false) {
        shape.setSize(sf::Vector2f(50, 50));
        shape.setPosition(x, y);
        shape.setFillColor(sf::Color(85, 85, 85));
        shape.setOutlineThickness(3);
        shape.setOutlineColor(sf::Color(51, 51, 51));

        for (int i = 0; i < 3; i++) {
            sf::RectangleShape crack(sf::Vector2f(30, 2));
            crack.setPosition(x + 10, y + 15 + i * 12);
            crack.setFillColor(sf::Color(40, 40, 40));
            cracks.push_back(crack);
        }
    }

    void draw(sf::RenderWindow& window) {
        window.draw(shape);
        for (auto& crack : cracks) {
            window.draw(crack);
        }
    }

    sf::FloatRect getBounds() const {
        return shape.getGlobalBounds();
    }
};

// Kept for future hazards if you want them later
struct FallingRock {
    sf::CircleShape shape;
    sf::Vector2f position;
    sf::Vector2f velocity;
    bool active;
    bool triggered;
    float resetTimer;
    float startY;

    FallingRock(float x, float y) : position(x, y), velocity(0, 0),
        active(false), triggered(false), resetTimer(0), startY(y) {
        shape.setRadius(12);
        shape.setFillColor(sf::Color(100, 100, 100));
        shape.setOutlineThickness(2);
        shape.setOutlineColor(sf::Color(70, 70, 70));
        shape.setPosition(position);
    }

    void update(sf::FloatRect playerBounds) {
        if (!triggered && resetTimer <= 0) {
            if (std::abs(playerBounds.left - position.x) < 40 && playerBounds.top > position.y) {
                triggered = true;
                active = true;
            }
        }

        if (active) {
            velocity.y += 0.5f;
            position.y += velocity.y;
            shape.setPosition(position);

            if (position.y > 650) {
                active = false;
                triggered = false;
                resetTimer = 240;
                position.y = startY;
                velocity.y = 0;
                shape.setPosition(position);
            }
        }

        if (resetTimer > 0) resetTimer--;
    }

    sf::FloatRect getBounds() const {
        return shape.getGlobalBounds();
    }
};

struct Icicle {
    sf::ConvexShape shape;
    sf::Vector2f position;
    sf::Vector2f velocity;
    bool falling;
    float fallTimer;
    float resetTimer;
    float startY;

    Icicle(float x, float y) : position(x, y), velocity(0, 0), falling(false),
        fallTimer(60), resetTimer(0), startY(y) {
        shape.setPointCount(3);
        shape.setPoint(0, sf::Vector2f(0, 0));
        shape.setPoint(1, sf::Vector2f(8, 0));
        shape.setPoint(2, sf::Vector2f(4, 30));
        shape.setPosition(position);
        shape.setFillColor(sf::Color(200, 230, 255));
        shape.setOutlineThickness(1);
        shape.setOutlineColor(sf::Color(150, 200, 255));
    }

    void update(sf::FloatRect playerBounds) {
        if (!falling && resetTimer <= 0) {
            if (std::abs(playerBounds.left - position.x) < 40 && playerBounds.top < position.y) {
                fallTimer--;
                if (fallTimer <= 0) {
                    falling = true;
                }
            }
            else {
                fallTimer = 60;
            }
        }

        if (falling) {
            velocity.y += 0.8f;
            position.y += velocity.y;
            shape.setPosition(position);

            if (position.y > 650) {
                falling = false;
                resetTimer = 300;
                position.y = startY;
                velocity.y = 0;
                fallTimer = 60;
                shape.setPosition(position);
            }
        }

        if (resetTimer > 0) resetTimer--;
    }

    sf::FloatRect getBounds() const {
        return shape.getGlobalBounds();
    }
};

struct LavaPool {
    sf::RectangleShape shape;
    sf::Vector2f position;
    float animOffset;

    LavaPool(float x, float y, float w) : position(x, y), animOffset(0) {
        shape.setSize(sf::Vector2f(w, 30));
        shape.setPosition(position);
        shape.setFillColor(sf::Color(255, 100, 0));
    }

    void update() {
        animOffset += 0.1f;
        sf::Color lavaColor(255, static_cast<sf::Uint8>(100 + std::sin(animOffset) * 50), 0);
        shape.setFillColor(lavaColor);
    }

    sf::FloatRect getBounds() const {
        return shape.getGlobalBounds();
    }
};

class Player {
public:
    // --- animation data ---
    sf::Sprite sprite;
    const std::vector<sf::Texture>* animTextures; // set in setAnimationTextures
    int currentFrame;
    float frameTimer;
    int facingDir;   // 1 = right, -1 = left

    enum AnimState { IDLE, RUNNING, JUMPING };
    AnimState animState;

    float spriteBaseScale; // controls how small the sprite is

    // --- old shape pieces (used only as fallback) ---
    sf::RectangleShape body;
    sf::RectangleShape hat;
    sf::CircleShape head;
    sf::RectangleShape eyeLeft;
    sf::RectangleShape eyeRight;
    sf::RectangleShape mustacheLeft;
    sf::RectangleShape mustacheRight;
    sf::RectangleShape legLeft;
    sf::RectangleShape legRight;

    sf::Vector2f position;
    sf::Vector2f velocity;
    float speed;
    float jumpPower;
    bool grounded;
    bool hasHammer;
    float animTimer;

    Player(float x, float y)
        : animTextures(nullptr),
        currentFrame(0),
        frameTimer(0.f),
        facingDir(1),
        animState(IDLE),
        spriteBaseScale(0.6f),
        position(x, y),
        velocity(0.f, 0.f),
        speed(4.0f),
        jumpPower(-12.0f),
        grounded(false),
        hasHammer(false),
        animTimer(0.f)
    {
        body.setSize(sf::Vector2f(24, 28));
        body.setFillColor(sf::Color::Red);

        head.setRadius(14);
        head.setFillColor(sf::Color(255, 220, 177));

        hat.setSize(sf::Vector2f(28, 8));
        hat.setFillColor(sf::Color::Red);

        eyeLeft.setSize(sf::Vector2f(4, 4));
        eyeLeft.setFillColor(sf::Color::Black);
        eyeRight.setSize(sf::Vector2f(4, 4));
        eyeRight.setFillColor(sf::Color::Black);

        mustacheLeft.setSize(sf::Vector2f(8, 3));
        mustacheLeft.setFillColor(sf::Color(101, 67, 33));
        mustacheRight.setSize(sf::Vector2f(8, 3));
        mustacheRight.setFillColor(sf::Color(101, 67, 33));

        legLeft.setSize(sf::Vector2f(10, 6));
        legLeft.setFillColor(sf::Color(50, 50, 200));
        legRight.setSize(sf::Vector2f(10, 6));
        legRight.setFillColor(sf::Color(50, 50, 200));

        updatePosition();
    }

    void setAnimationTextures(const std::vector<sf::Texture>* texPtr) {
        animTextures = texPtr;
        currentFrame = 0;
        frameTimer = 0.f;

        if (animTextures && !animTextures->empty()) {
            sprite.setTexture((*animTextures)[0]);

            // origin at bottom centre so flipping works nicely
            sf::FloatRect bounds = sprite.getLocalBounds();
            sprite.setOrigin(bounds.width / 2.f, bounds.height);
        }
    }

    void updatePosition() {
        // existing leg wobble (used only in fallback draw)
        animTimer += 0.15f;
        float legOffset = grounded ? std::sin(animTimer) * 2 : 0;

        body.setPosition(position.x + 8, position.y + 18);
        head.setPosition(position.x + 4, position.y - 4);
        hat.setPosition(position.x + 2, position.y - 10);
        eyeLeft.setPosition(position.x + 10, position.y + 4);
        eyeRight.setPosition(position.x + 18, position.y + 4);
        mustacheLeft.setPosition(position.x + 6, position.y + 12);
        mustacheRight.setPosition(position.x + 18, position.y + 12);
        legLeft.setPosition(position.x + 8, position.y + 40 + legOffset);
        legRight.setPosition(position.x + 22, position.y + 40 - legOffset);

        // --- sprite animation ---
        if (!animTextures || animTextures->empty())
            return;

        // Put sprite feet where the old body bottom was
        sprite.setPosition(position.x + 16.f, position.y + 46.f);

        // Decide animation state
        if (!grounded) {
            animState = JUMPING;
        }
        else if (std::fabs(velocity.x) > 0.1f) {
            animState = RUNNING;
        }
        else {
            animState = IDLE;
        }

        // Flip + scale
        float sx = (facingDir > 0 ? 1.f : -1.f) * spriteBaseScale;
        float sy = spriteBaseScale;
        sprite.setScale(sx, sy);

        // Frame ranges: 0 = idle, 1–4 = run, 5 = jump
        int idleFrame = 0;
        int runStart = 1;
        int runEnd = 4;
        int jumpFrame = 5;

        switch (animState) {
        case IDLE:
            if (currentFrame != idleFrame) {
                currentFrame = idleFrame;
                sprite.setTexture((*animTextures)[currentFrame], true);
            }
            break;

        case RUNNING:
            frameTimer += 0.2f;   // animation speed
            if (frameTimer >= 1.f) {
                frameTimer = 0.f;
                if (currentFrame < runStart || currentFrame > runEnd)
                    currentFrame = runStart;
                else
                    currentFrame++;

                if (currentFrame > runEnd)
                    currentFrame = runStart;

                sprite.setTexture((*animTextures)[currentFrame], true);
            }
            break;

        case JUMPING:
            if (currentFrame != jumpFrame) {
                currentFrame = jumpFrame;
                sprite.setTexture((*animTextures)[currentFrame], true);
            }
            break;
        }
    }

    sf::FloatRect getBounds() const {
        return sf::FloatRect(position.x, position.y, 32, 46);
    }

    void draw(sf::RenderWindow& window) {
        if (animTextures && !animTextures->empty()) {
            window.draw(sprite);
        }
        else {
            // fallback if textures missing
            window.draw(legLeft);
            window.draw(legRight);
            window.draw(body);
            window.draw(head);
            window.draw(hat);
            window.draw(eyeLeft);
            window.draw(eyeRight);
            window.draw(mustacheLeft);
            window.draw(mustacheRight);
        }
    }

    void reset(float x, float y) {
        position = sf::Vector2f(x, y);
        velocity = sf::Vector2f(0, 0);
        grounded = false;
        hasHammer = false;

        facingDir = 1;
        animState = IDLE;
        currentFrame = 0;
        frameTimer = 0.f;

        updatePosition();
    }
};


class Game {
private:
    sf::RenderWindow window;
    sf::View view;              // ADDED: for side-scrolling camera
    Player player;

    std::vector<Platform> platforms;
    std::vector<Diamond> diamonds;
    std::vector<Enemy> enemies;
    std::vector<FallingRock> fallingRocks;
    std::vector<Icicle> icicles;
    std::vector<LavaPool> lavaPools;
    Hammer* hammer;
    Boulder* boulder;
    sf::RectangleShape exitDoor;

    GameState state;
    MenuPage menuPage;      // which menu page we are on
    int currentLevel;
    int lives;
    int diamondsCollected;
    int score;

    sf::Font font;
    bool fontLoaded;
    sf::Color bgColor;
    float friction;

    const float GRAVITY = 0.5f;
    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;

    // --- Axe (hammer) texture ---
    sf::Texture axeTexture;
    bool axeLoaded = false;

    sf::Texture diamondTexture;
    bool diamondLoaded = false;

    sf::Texture diamondTexture2;
    bool diamond2Loaded = false;

    sf::Texture iceBlockTexture;
    bool iceBlockLoaded = false;

    sf::Texture seaweedTexture;
    bool seaweedLoaded = false;



    // --- Backgrounds for levels 1–4 ---
    sf::Texture bgTextures[4];
    sf::Sprite  bgSprites[4];
    bool bgLoaded[4] = { false, false, false, false };


    // ADDED: world constants for scrolling
    const float WORLD_WIDTH = 2400.f;
    const float GROUND_Y = 568.f;   // 600 - 32


    // --- MENU UI ---
    sf::RectangleShape startButton;
    sf::RectangleShape instructionsButton;
    sf::RectangleShape mapButton;
    sf::RectangleShape settingsButton;
    sf::RectangleShape shopButton;
    sf::RectangleShape backButton;
    sf::RectangleShape menuPanel;

    // --- Level 1 background image ---
    sf::Texture bgTexture1;
    sf::Sprite  bgSprite1;
    bool bg1Loaded = false;
    // --- Door texture ---
    sf::Texture doorTexture;
    bool doorLoaded = false;


    // --- Bat enemy textures ---
    std::vector<sf::Texture> batTextures;
    bool batsLoaded = false;

    // --- Player animation textures ---
    std::vector<sf::Texture> playerTextures;
    bool playerAnimLoaded = false;

    //Settings variables 
    int musicVolume = 100;     // 0..100
    bool musicMuted = false;   // true = muted

   // SETTINGS BUTTONS (ADDED)
    sf::RectangleShape volDownButton;   // "-" button
    sf::RectangleShape volUpButton;     // "+" button
    sf::RectangleShape muteButton;      // "MUTE/UNMUTE" toggle




    // ADDED: Three new variables for menu animations
    float menuAnimTime;      // Tracks time for smooth animations (increments each frame)
    float titleBounce;       // Controls title vertical bouncing motion
    float glowPulse;         // Controls glow intensity pulsing (alpha channel)

    
    // ADDED: Structure to define each floating particle
    struct MenuParticle {
        sf::CircleShape shape;      // The visual particle
        sf::Vector2f velocity;      // Movement speed and direction
        float lifetime;             // How long it's been alive
        float maxLifetime;          // When to respawn it
        float rotation;             // Current rotation angle
        float rotationSpeed;        // How fast it spins
    };

    // ADDED: Vector storing all 80 particles
    std::vector<MenuParticle> menuParticles;

  
    // ADDED: Structure to define each floating diamond
    struct FloatingDiamond {
        sf::CircleShape shape;      // Diamond shape (4-point circle)
        sf::Vector2f position;      // Current position
        float angle;                // Rotation and movement angle
        float speed;                // Movement speed
        float bobOffset;            // Offset for bobbing motion
    };

    // ADDED: Vector storing all 12 diamonds
    std::vector<FloatingDiamond> floatingDiamonds;

 
// ADDED: Complete function to create all animated menu elements
    void initMenuParticles() {
        menuParticles.clear();

        // Create 80 floating particles with random properties
        for (int i = 0; i < 80; i++) {
            MenuParticle p;

            float radius = 1.f + static_cast<float>(rand() % 4);
            p.shape.setRadius(radius);

            // Center origin so rotation looks nice
            p.shape.setOrigin(radius, radius);

            // Randomize colors: gold, blue, pink, or green
            int colorType = rand() % 4;
            if (colorType == 0) {
                p.shape.setFillColor(sf::Color(255, 215, 0, 80 + rand() % 120));  // Gold
            }
            else if (colorType == 1) {
                p.shape.setFillColor(sf::Color(100, 200, 255, 60 + rand() % 100));  // Blue
            }
            else if (colorType == 2) {
                p.shape.setFillColor(sf::Color(255, 100, 150, 60 + rand() % 100));  // Pink
            }
            else {
                p.shape.setFillColor(sf::Color(150, 255, 150, 60 + rand() % 100));  // Green
            }

            // Random starting position
            p.shape.setPosition(
                static_cast<float>(rand() % WINDOW_WIDTH),
                static_cast<float>(rand() % WINDOW_HEIGHT)
            );

            // Random velocity (mostly upward movement)
            p.velocity = sf::Vector2f(
                -0.5f + static_cast<float>(rand() % 100) / 100.f,  // Horizontal drift
                -0.3f - static_cast<float>(rand() % 150) / 100.f   // Upward movement
            );

            p.maxLifetime = 150.f + static_cast<float>(rand() % 180);
            p.lifetime = static_cast<float>(rand() % 150);
            p.rotation = 0.f;
            p.rotationSpeed = -2.f + static_cast<float>(rand() % 400) / 100.f;

            menuParticles.push_back(p);
        }

        // Create 12 floating diamond shapes
        floatingDiamonds.clear();
        for (int i = 0; i < 12; i++) {
            FloatingDiamond d;

            d.shape.setRadius(6.f);
            d.shape.setPointCount(4);  // 4 points makes a diamond
            d.shape.setFillColor(sf::Color(255, 215, 0, 150));  // Semi-transparent gold
            d.shape.setOutlineThickness(1.f);
            d.shape.setOutlineColor(sf::Color(200, 180, 0, 200));

            d.position = sf::Vector2f(
                static_cast<float>(rand() % WINDOW_WIDTH),
                static_cast<float>(rand() % WINDOW_HEIGHT)
            );

            d.angle = static_cast<float>(rand() % 360);
            d.speed = 0.3f + static_cast<float>(rand() % 50) / 100.f;
            d.bobOffset = static_cast<float>(rand() % 100) / 10.f;

            d.shape.setOrigin(6.f, 6.f);  // Center origin for rotation
            d.shape.setPosition(d.position);

            floatingDiamonds.push_back(d);
        }
    }

    // ============================================================
    // ✨ ENHANCEMENT #5: Update Menu Animation Function
    // ============================================================
    // ADDED: Function that updates all animations every frame
    void updateMenuAnimation() {
        menuAnimTime += 0.016f;  // ~60 FPS timing (simple fixed-step)

        // Calculate bouncing and glowing effects using sine waves
        titleBounce = std::sin(menuAnimTime * 2.f) * 5.f;          // Bounces 5 pixels
        glowPulse = 150.f + std::sin(menuAnimTime * 3.f) * 50.f;   // Pulses between ~100-200

        // Update each particle
        for (auto& p : menuParticles) {
            p.shape.move(p.velocity);     // Move particle
            p.lifetime++;
            p.rotation += p.rotationSpeed;  // Spin particle
            p.shape.setRotation(p.rotation);

            // Respawn particle if it lived too long
            if (p.lifetime > p.maxLifetime) {
                p.lifetime = 0.f;
                p.shape.setPosition(
                    static_cast<float>(rand() % WINDOW_WIDTH),
                    static_cast<float>(WINDOW_HEIGHT + 20)  // Start from bottom
                );
            }

            // Wrap particle if it goes off top of screen
            if (p.shape.getPosition().y < -20) {
                p.shape.setPosition(
                    static_cast<float>(rand() % WINDOW_WIDTH),
                    static_cast<float>(WINDOW_HEIGHT + 20)
                );
                p.lifetime = 0.f;
            }

            // Fade particle based on lifetime
            float alpha = 255.f * (1.f - p.lifetime / p.maxLifetime);
            sf::Color c = p.shape.getFillColor();
            c.a = static_cast<sf::Uint8>(std::max(0.f, std::min(alpha, 200.f)));
            p.shape.setFillColor(c);
        }

        // Update each floating diamond
        for (auto& d : floatingDiamonds) {
            d.angle += d.speed;

            // Bobbing motion using sine wave
            d.position.y += std::sin(d.angle + d.bobOffset) * 0.5f;
            d.position.x += std::cos(d.angle * 0.5f) * 0.3f;

            // Wrap diamond if it goes off screen
            if (d.position.y < -20 || d.position.y > WINDOW_HEIGHT + 20 ||
                d.position.x < -20 || d.position.x > WINDOW_WIDTH + 20) {
                d.position = sf::Vector2f(
                    static_cast<float>(rand() % WINDOW_WIDTH),
                    static_cast<float>(rand() % WINDOW_HEIGHT)
                );
            }

            d.shape.setPosition(d.position);
            d.shape.setRotation(d.angle * 10.f);  // Spin the diamond
        }
    }





    void buildCommonLevelLayout() {
        platforms.clear();
        diamonds.clear();
        enemies.clear();
        fallingRocks.clear();
        icicles.clear();
        lavaPools.clear();
        delete hammer;
        delete boulder;
        hammer = nullptr;
        boulder = nullptr;


        // Different fallback colours for backgrounds if texture fails
        if (currentLevel == 2) {
            bgColor = sf::Color(10, 20, 40);   // colder for ice level
        }
        else {
            bgColor = sf::Color(20, 10, 30);   // deep cave purple
        }
        friction = 0.85f;

        float blockSize = 32.f;

        // Helper: place a 32×32 block on a grid (uses ice texture on level 2)
        auto addBlock = [&](int gx, int gy, sf::Color color = sf::Color(60, 40, 40)) {
            float x = gx * blockSize;
            float y = gy * blockSize;

            if (currentLevel == 2 && iceBlockLoaded) {
                platforms.emplace_back(x, y, blockSize, blockSize, &iceBlockTexture);
            }
            else {
                platforms.emplace_back(x, y, blockSize, blockSize, color);
            }
            };

        // Helper: main platforms at arbitrary world positions
        auto addMainPlatform = [&](float x, float y, sf::Color color = sf::Color(90, 70, 70)) {
            if (currentLevel == 2 && iceBlockLoaded) {
                platforms.emplace_back(x, y, blockSize, blockSize, &iceBlockTexture);
            }
            else {
                platforms.emplace_back(x, y, blockSize, blockSize, color);
            }
            };

        // Helper: path tiles, with optional vertical offset in tiles (for harder paths)
        auto addPathTile = [&](int gx, int heightOffset, sf::Color color = sf::Color(80, 55, 55)) {
            float basePathY = GROUND_Y - blockSize;    // default path height
            float x = gx * blockSize;
            float y = basePathY - heightOffset * blockSize;

            if (currentLevel == 2 && iceBlockLoaded) {
                platforms.emplace_back(x, y, blockSize, blockSize, &iceBlockTexture);
            }
            else {
                platforms.emplace_back(x, y, blockSize, blockSize, color);
            }
            };

        // Helper: icicle that is visually attached under an ice block
        auto addIcicleWithBlock = [&](int gx, int gy) {
            // Make sure there is an ice block here
            addBlock(gx, gy);

            // Icicle hangs from the bottom centre of that block
            float x = gx * blockSize + (blockSize / 2.f) - 4.f; // 8px wide base => offset by 4
            float y = (gy + 1) * blockSize;                     // just under the block
            icicles.emplace_back(x, y);
            };



        // --- Ground: continuous strip of small square blocks ---
        for (int gx = 0; gx < static_cast<int>(WORLD_WIDTH / blockSize); ++gx) {
            float x = gx * blockSize;

            if (currentLevel == 2 && iceBlockLoaded) {
                platforms.emplace_back(x, GROUND_Y, blockSize, blockSize, &iceBlockTexture);
            }
            else {
                platforms.emplace_back(x, GROUND_Y, blockSize, blockSize, sf::Color(60, 40, 40));
            }
        }


        // --- Diamond textures ---
        if (diamondTexture.loadFromFile("tiles/diamond.png")) {
            diamondLoaded = true;
        }

        if (diamondTexture2.loadFromFile("tiles/diamond2.png")) {
            diamond2Loaded = true;
        }
        else {
            std::cout << "Failed to load tiles/diamond2.png\n";
        }

        const sf::Texture* diamondTexToUse =
            (currentLevel == 2 && diamond2Loaded) ? &diamondTexture2 : &diamondTexture;

        // ----------------------------------------------------
        // DECORATIVE CAVE CEILING (top of screen)
        // ----------------------------------------------------
        {
            sf::Color ceilingColor1(45, 30, 60);
            sf::Color ceilingColor2(55, 35, 70);

            // Row 0 (very top)
            for (int gx = 0; gx < static_cast<int>(WORLD_WIDTH / blockSize); ++gx) {
                addBlock(gx, 0, ceilingColor1);
            }

            // Row 1 (just under the top, with some gaps for variety)
            for (int gx = 0; gx < static_cast<int>(WORLD_WIDTH / blockSize); ++gx) {
                if (gx % 4 == 1) continue;   // gaps for rocky look
                addBlock(gx, 1, ceilingColor2);
            }
        }

        // Two main platform heights (easy to reach)
        float h1 = GROUND_Y - 60.f;   // low platforms
        float h2 = GROUND_Y - 120.f;  // slightly higher
        float h3 = GROUND_Y - 180.f;  // optional higher

        // ----------------------------------------------------
// CAVE PATH / PLATFORMS (different layouts per level)
// ----------------------------------------------------
        sf::Color pathColor(80, 55, 55);

        if (currentLevel == 1) {
            // Original stepped mid path
            for (int gx = 1; gx <= 6; ++gx)  addBlock(gx, 16, pathColor);
            for (int gx = 7; gx <= 12; ++gx) addBlock(gx, 15, pathColor);
            for (int gx = 13; gx <= 18; ++gx) addBlock(gx, 14, pathColor);
            for (int gx = 19; gx <= 22; ++gx) addBlock(gx, 15, pathColor);
            for (int gx = 23; gx <= 26; ++gx) addBlock(gx, 16, pathColor);

            // Middle platforms
            addMainPlatform(1080.f, h1);
            addMainPlatform(1230.f, h2);

            // Right platforms
            addMainPlatform(1564.f, h2);
            addMainPlatform(1740.f, h1);
            addMainPlatform(1900.f, h2);
            addMainPlatform(2060.f, h1);

            // High bonus
            addMainPlatform(700.f, h3, sf::Color(110, 80, 90));
            addMainPlatform(1600.f, h3, sf::Color(110, 80, 90));
        }
        else if (currentLevel == 2) {
            // LEVEL 2: different, trickier ice layout

            // Left: small staggered steps
            addMainPlatform(400.f, h1);     // low
            addMainPlatform(520.f, h2);     // higher
            addMainPlatform(640.f, h1);     // back down

            // Mid: vertical challenge
            addMainPlatform(950.f, h2);
            addMainPlatform(1030.f, h3);    // quite high
            addMainPlatform(1150.f, h2);

            // Right: spaced platforms toward the door
            addMainPlatform(1500.f, h2);
            addMainPlatform(1650.f, h3);
            addMainPlatform(1820.f, h2);
            addMainPlatform(1980.f, h1);

            // One high bonus ledge (different from level 1)
            addMainPlatform(1350.f, h3, sf::Color(110, 80, 90));
        }


        // -----------------------------------------------------------------
        // Extra decorative blocks (do NOT block main path)
        // -----------------------------------------------------------------
        for (int c = 0; c < 25; ++c) addBlock(c, 0);
        for (int c = 3; c <= 7; ++c) addBlock(c, 1);
        for (int c = 12; c <= 17; ++c) addBlock(c, 1);
        for (int c = 20; c <= 23; ++c) addBlock(c, 1);

        // Stalactites (will be more "icy" on level 2 thanks to ice texture)
        addBlock(5, 2); addBlock(5, 3);
        addBlock(14, 2); addBlock(14, 3);
        addBlock(21, 2); addBlock(21, 3);

        for (int c = 25; c < 50; ++c) addBlock(c, 0);
        for (int c = 28; c <= 32; ++c) addBlock(c, 1);
        for (int c = 40; c <= 44; ++c) addBlock(c, 1);

        for (int c = 10; c <= 13; ++c) addBlock(c, 8);
        for (int c = 35; c <= 38; ++c) addBlock(c, 9);

        // ----------------------------------------------------
// BOTTOM PATH (walkway towards the door)
// - Level 1: continuous, easy
// - Level 2: stepped, with small gaps & height changes (harder)
// ----------------------------------------------------
        {
            if (currentLevel == 1) {
                float pathY = GROUND_Y - blockSize;
                float pathEndX = WORLD_WIDTH - blockSize;
                for (float x = 0.f; x <= pathEndX; x += blockSize) {
                    // normal rock path
                    platforms.emplace_back(
                        x, pathY,
                        blockSize, blockSize,
                        pathColor
                    );
                }
            }
            else if (currentLevel == 2) {
                // Use grid columns and height offsets for a trickier path

                // Segment 1: start flat
                for (int gx = 0; gx <= 8; ++gx) {
                    addPathTile(gx, 0);
                }

                // Segment 2: one tile higher
                for (int gx = 9; gx <= 13; ++gx) {
                    addPathTile(gx, 1);
                }

                // Small gap at 14 (no tile)

                // Segment 3: back to base height
                for (int gx = 15; gx <= 20; ++gx) {
                    addPathTile(gx, 0);
                }

                // Segment 4: two tiles higher (harder jump section)
                for (int gx = 21; gx <= 24; ++gx) {
                    addPathTile(gx, 2);
                }

                // Gap at 25

                // Segment 5: slightly raised
                for (int gx = 26; gx <= 32; ++gx) {
                    addPathTile(gx, 1);
                }

                // Final run toward door at base height
                for (int gx = 33; gx <= 70; ++gx) {
                    addPathTile(gx, 0);
                }
            }
        }   // <-- END OF PATH SECTION

        // --------------------------------------------------
        // EXTRA END-OF-LEVEL ICE FIX (fills last columns)
        // --------------------------------------------------
        if (currentLevel == 2 && iceBlockLoaded) {
            float yPath = GROUND_Y - blockSize;  // path height
            float yGround = GROUND_Y;              // true ground

            // Cover the last 3 columns with ice on BOTH rows
            for (int i = 1; i <= 3; ++i) {
                float x = WORLD_WIDTH - i * blockSize;

                // path row
                platforms.emplace_back(x, yPath, blockSize, blockSize, &iceBlockTexture);
                // ground row
                platforms.emplace_back(x, yGround, blockSize, blockSize, &iceBlockTexture);
            }
        }



        // ----------------------------------------------------
        // RIGHT-HAND CAVE WALL (ceiling-to-floor at level end)
        // ----------------------------------------------------
        {
            int wallCol = static_cast<int>((WORLD_WIDTH - 32.f) / 32.f); // 74
            sf::Color wallColor(60, 40, 40);

            for (int gy = 0; gy <= 16; ++gy) {
                addBlock(wallCol, gy, wallColor);
            }
        }


        // ---------------- DIAMONDS (same layout, different texture on L2) ---------------
        diamonds.emplace_back(320.f + 4.f, h2 - 40.f, diamondTexToUse);
        diamonds.emplace_back(600.f + 4.f, h2 - 40.f, diamondTexToUse);
        diamonds.emplace_back(930.f + 4.f, h2 - 40.f, diamondTexToUse);
        diamonds.emplace_back(1230.f + 4.f, h2 - 40.f, diamondTexToUse);
        diamonds.emplace_back(1420.f + 4.f, h1 - 40.f, diamondTexToUse);
        diamonds.emplace_back(1580.f + 4.f, h2 - 40.f, diamondTexToUse);
        diamonds.emplace_back(1900.f + 4.f, h2 - 40.f, diamondTexToUse);
        diamonds.emplace_back(2060.f + 4.f, h1 - 40.f, diamondTexToUse);

        // Bonus diamonds
        diamonds.emplace_back(700.f + 4.f, h3 - 40.f, diamondTexToUse);
        diamonds.emplace_back(1600.f + 4.f, h3 - 40.f, diamondTexToUse);

        // ---------------- ENEMIES: Level 2 = more + faster ----------------
        if (currentLevel == 1) {
            enemies.emplace_back(550.f, h2 - 40.f, 1.0f, 480.f, 720.f, &batTextures);
            enemies.emplace_back(1150.f, h2 - 40.f, 1.2f, 1080.f, 1350.f, &batTextures);
            enemies.emplace_back(1850.f, h2 - 80.f, 1.0f, 1780.f, 2100.f, &batTextures);
        }
        else if (currentLevel == 2) {
            // Left section bat, patrolling above the staggered platforms
            enemies.emplace_back(520.f, h2 - 50.f, 1.5f, 380.f, 680.f, &batTextures);

            // Mid vertical challenge bat over the high platform
            enemies.emplace_back(1030.f, h3 - 50.f, 1.6f, 940.f, 1180.f, &batTextures);

            // Right section bats over the last platforms
            enemies.emplace_back(1600.f, h2 - 40.f, 1.7f, 1480.f, 1760.f, &batTextures);
            enemies.emplace_back(1900.f, h2 - 60.f, 1.8f, 1820.f, 2140.f, &batTextures);
        }

        // ---------------- ICICLES: more, and all attached to ice blocks ----
        if (currentLevel == 2) {
            // These coordinates are grid-based (gx, gy)
            addIcicleWithBlock(6, 2);
            addIcicleWithBlock(12, 3);
            addIcicleWithBlock(18, 3);
            addIcicleWithBlock(24, 2);
            addIcicleWithBlock(30, 3);
            addIcicleWithBlock(36, 3);
            addIcicleWithBlock(42, 2);
        }

        // ---------------- HAMMER POSITION: higher on Level 2 ---------------
        float hammerX, hammerY;
        if (currentLevel == 2) {
            // Put hammer on a high platform so player MUST do trickier jumps
            hammerX = 1600.f;
            hammerY = h3 - 30.f;
        }
        else {
            hammerX = 1100.f;
            hammerY = h2 - 30.f;
        }

        if (axeLoaded)
            hammer = new Hammer(hammerX, hammerY, &axeTexture);
        else
            hammer = nullptr;

        // ---------------- EXIT DOOR at far right --------------------------
        float doorWidth = 40.f;
        float doorHeight = 70.f;

        float doorX = WORLD_WIDTH - 72.f;                 // right next to the wall
        float doorY = (GROUND_Y - doorHeight) - 32.f;     // sits level with the path

        boulder = nullptr;  // no boulder now

        if (doorLoaded) {
            exitDoor.setSize(sf::Vector2f(doorWidth, doorHeight));
            exitDoor.setTexture(&doorTexture);
            exitDoor.setTextureRect(sf::IntRect(
                0, 0,
                doorTexture.getSize().x,
                doorTexture.getSize().y
            ));
        }
        else {
            exitDoor.setSize(sf::Vector2f(doorWidth, doorHeight));
            exitDoor.setFillColor(sf::Color(255, 215, 0));
        }
        exitDoor.setPosition(doorX, doorY);

        // ----------------------------------------------------
// LEVEL 3: special layout – blocks only top & bottom
// ----------------------------------------------------
        if (currentLevel == 3) {
            // Remove whatever platforms were added earlier
            platforms.clear();

            float blockSize = 32.f;
            int cols = static_cast<int>(WORLD_WIDTH / blockSize);
            int groundRow = static_cast<int>(GROUND_Y / blockSize);

            auto addColumnBlock = [&](int gx, int gy) {
                float x = gx * blockSize;
                float y = gy * blockSize;

                if (seaweedLoaded) {
                    platforms.emplace_back(x, y, blockSize, blockSize, &seaweedTexture);
                }
                else {
                    platforms.emplace_back(x, y, blockSize, blockSize, sf::Color(60, 40, 40));
                }
                };

            // --------- Bottom "seaweed floor" varying heights ----------
            for (int gx = 0; gx < cols; ++gx) {
                int heightBlocks;

                // pattern of heights: 4,3,2,1,3,2,1,...
                switch (gx % 7) {
                case 0:
                case 1: heightBlocks = 4; break;
                case 2:
                case 3: heightBlocks = 3; break;
                case 4:
                case 5: heightBlocks = 2; break;
                default: heightBlocks = 1; break;
                }

                for (int i = 0; i < heightBlocks; ++i) {
                    int gy = groundRow - i;
                    addColumnBlock(gx, gy);
                }
            }

            // --------- Top "seaweed ceiling" varying heights ----------
            for (int gx = 0; gx < cols; ++gx) {
                int heightBlocks;

                // different pattern so top ≠ bottom
                if (gx % 5 == 0 || gx % 5 == 3)
                    heightBlocks = 3;
                else
                    heightBlocks = 2;

                for (int gy = 0; gy < heightBlocks; ++gy) {
                    addColumnBlock(gx, gy);
                }
            }
        }




        // Player start at far left, slightly above ground
        player.reset(50.f, GROUND_Y - 60.f);
    }


public:
    Game() :
        window(sf::VideoMode(800, 600), "Oreo Escape - Cave Adventure"),
        view(sf::FloatRect(0.f, 0.f, 800.f, 600.f)),
        player(100, 300),
        state(MENU),
        menuPage(MAIN_MENU),
        currentLevel(1),
        lives(3),
        diamondsCollected(0),
        score(0),
        hammer(nullptr),
        boulder(nullptr),
        fontLoaded(false),
        friction(0.85f),
        menuAnimTime(0.f),
        titleBounce(0.f),
        glowPulse(150.f) {

        window.setFramerateLimit(60);

        // Load 4 level backgrounds
        for (int i = 0; i < 4; i++) {
            std::string filename = "tiles/background" + std::to_string(i + 1) + ".png";
            if (bgTextures[i].loadFromFile(filename)) {
                bgLoaded[i] = true;
                bgSprites[i].setTexture(bgTextures[i]);

                // Scale each background to fill the screen
                sf::Vector2u texSize = bgTextures[i].getSize();
                float scaleX = WINDOW_WIDTH / static_cast<float>(texSize.x);
                float scaleY = WINDOW_HEIGHT / static_cast<float>(texSize.y);
                bgSprites[i].setScale(scaleX, scaleY);
                bgSprites[i].setPosition(0.f, 0.f);
            }
            else {
                std::cout << "Failed to load " << filename << "\n";
            }
        }

        fontLoaded = font.loadFromFile("arial.ttf") ||
            font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf") ||
            font.loadFromFile("C:/Windows/Fonts/arial.ttf");


        // --- Load Level 1 background image ---
        if (bgTexture1.loadFromFile("tiles/background1.png")) {
            bg1Loaded = true;
            bgSprite1.setTexture(bgTexture1);

            sf::Vector2u texSize = bgTexture1.getSize();
            float scaleX = WINDOW_WIDTH / static_cast<float>(texSize.x);
            float scaleY = WINDOW_HEIGHT / static_cast<float>(texSize.y);
            bgSprite1.setScale(scaleX, scaleY);
            bgSprite1.setPosition(0.f, 0.f);
        }
        else {
            std::cout << "Failed to load tiles/background1.png\n";
        }

        // --- Load bat animation frames ---
        for (int i = 1; i <= 9; ++i) {
            sf::Texture tex;
            std::string fileName = "tiles/bat" + std::to_string(i) + ".png";
            if (!tex.loadFromFile(fileName)) {
                std::cout << "Failed to load " << fileName << "\n";
                break;
            }
            batTextures.push_back(tex);
        }

        batsLoaded = (batTextures.size() == 9);

        // --- Load player animation frames ---
        for (int i = 1; i <= 6; ++i) {
            sf::Texture tex;
            std::string fileName = "tiles/character" + std::to_string(i) + ".png";
            if (!tex.loadFromFile(fileName)) {
                std::cout << "Failed to load " << fileName << "\n";
                break;
            }
            playerTextures.push_back(tex);
        }

        // --- Load axe image for hammer pickup ---
        if (axeTexture.loadFromFile("tiles/axe.png")) {   // adjust path if needed
            axeLoaded = true;
        }
        else {
            std::cout << "Failed to load tiles/axe.png\n";
        }

        playerAnimLoaded = (playerTextures.size() == 6);
        if (playerAnimLoaded) {
            player.setAnimationTextures(&playerTextures);
        }

        // -- - Load door image-- -
        if (doorTexture.loadFromFile("tiles/door.png")) {
            doorLoaded = true;
        }
        else {
            std::cout << "Failed to load tiles/door.png\n";
        }

        if (iceBlockTexture.loadFromFile("tiles/iceBlock.png")) {
            iceBlockLoaded = true;
        }
        if (seaweedTexture.loadFromFile("tiles/seaweed.png")) {
            seaweedLoaded = true;
        }
        else {
            std::cout << "Failed to load tiles/seaweed.png\n";
        }





        view.setCenter(400.f, 300.f);

        menuPanel.setSize(sf::Vector2f(360.f, 360.f));
        menuPanel.setPosition(220.f, 160.f);
        menuPanel.setFillColor(sf::Color(0, 0, 0, 180));
        menuPanel.setOutlineThickness(3.f);
        menuPanel.setOutlineColor(sf::Color(255, 215, 0));

        mapButton.setSize(sf::Vector2f(260, 40));
        mapButton.setPosition(270, 190);
        mapButton.setFillColor(sf::Color(60, 90, 140));
        mapButton.setOutlineThickness(2.f);
        mapButton.setOutlineColor(sf::Color(20, 30, 60));

        settingsButton.setSize(sf::Vector2f(260, 40));
        settingsButton.setPosition(270, 240);
        settingsButton.setFillColor(sf::Color(60, 90, 140));
        settingsButton.setOutlineThickness(2.f);
        settingsButton.setOutlineColor(sf::Color(20, 30, 60));

        instructionsButton.setSize(sf::Vector2f(260, 40));
        instructionsButton.setPosition(270, 290);
        instructionsButton.setFillColor(sf::Color(60, 90, 140));
        instructionsButton.setOutlineThickness(2.f);
        instructionsButton.setOutlineColor(sf::Color(20, 30, 60));

        shopButton.setSize(sf::Vector2f(260, 40));
        shopButton.setPosition(270, 340);
        shopButton.setFillColor(sf::Color(60, 90, 140));
        shopButton.setOutlineThickness(2.f);
        shopButton.setOutlineColor(sf::Color(20, 30, 60));

        startButton.setSize(sf::Vector2f(260, 50));
        startButton.setPosition(270, 400);
        startButton.setFillColor(sf::Color(255, 180, 0));
        startButton.setOutlineThickness(2.f);
        startButton.setOutlineColor(sf::Color(130, 90, 0));

        backButton.setSize(sf::Vector2f(120.f, 35.f));
        backButton.setPosition(40.f, 520.f);
        backButton.setFillColor(sf::Color(80, 80, 80));
        backButton.setOutlineThickness(2.f);
        backButton.setOutlineColor(sf::Color(200, 200, 200));


        // Small - button
        volDownButton.setSize(sf::Vector2f(200.f, 340.f));
        volDownButton.setPosition(200.f, 430.f);
        volDownButton.setFillColor(sf::Color(40, 40, 60));
        volDownButton.setOutlineThickness(3.f);
        volDownButton.setOutlineColor(sf::Color(255, 215, 0));

        // Small + button
        volUpButton.setSize(sf::Vector2f(50.f, 35.f));
        volUpButton.setPosition(260.f, 430.f);
        volUpButton.setFillColor(sf::Color(40, 40, 60));
        volUpButton.setOutlineThickness(3.f);
        volUpButton.setOutlineColor(sf::Color(255, 215, 0));

        // Mute toggle button
        muteButton.setSize(sf::Vector2f(160.f, 35.f));
        muteButton.setPosition(330.f, 430.f);
        muteButton.setFillColor(sf::Color(40, 40, 60));
        muteButton.setOutlineThickness(3.f);
        muteButton.setOutlineColor(sf::Color(255, 215, 0));

        std::srand(static_cast<unsigned>(std::time(nullptr)));
        initMenuParticles();
    }

    ~Game() {
        delete hammer;
        delete boulder;
    }

    void loadLevel(int level) {
        currentLevel = level;
        buildCommonLevelLayout();     // same layout for all 4 levels for now
    }



    void handleInput() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (state == MENU) {
                window.setView(window.getDefaultView());

                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                    sf::Vector2f mousePosF(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));

                    if (menuPage == MAIN_MENU) {
                        if (startButton.getGlobalBounds().contains(mousePosF)) {
                            lives = 3;
                            score = 0;
                            diamondsCollected = 0;
                            loadLevel(1);
                            state = PLAYING;
                        }
                        else if (mapButton.getGlobalBounds().contains(mousePosF)) {
                            menuPage = MAP_PAGE;
                        }
                        else if (settingsButton.getGlobalBounds().contains(mousePosF)) {
                            menuPage = SETTINGS_PAGE;
                        }
                        else if (instructionsButton.getGlobalBounds().contains(mousePosF)) {
                            menuPage = INSTRUCTIONS_PAGE;
                        }
                        else if (shopButton.getGlobalBounds().contains(mousePosF)) {
                            menuPage = SHOP_PAGE;
                        }
                    }
                    else {
                        // BACK works on all sub-pages
                        if (backButton.getGlobalBounds().contains(mousePosF)) {
                            menuPage = MAIN_MENU;
                        }

                        // ================================
                        // SETTINGS PAGE BUTTON CLICKS (ADDED)
                        // ================================
                        if (menuPage == SETTINGS_PAGE) {

                            // "-" decrease volume
                            if (volDownButton.getGlobalBounds().contains(mousePosF)) {
                                musicVolume = std::max(0, musicVolume - 10);
                                if (musicVolume == 0) musicMuted = true;   // optional behavior
                                else musicMuted = false;

                                // If later you add sf::Music music;
                                // music.setVolume(musicMuted ? 0.f : (float)musicVolume);
                            }

                            // "+" increase volume
                            if (volUpButton.getGlobalBounds().contains(mousePosF)) {
                                musicVolume = std::min(100, musicVolume + 10);
                                if (musicVolume > 0) musicMuted = false;

                                // music.setVolume(musicMuted ? 0.f : (float)musicVolume);
                            }

                            // Mute toggle
                            if (muteButton.getGlobalBounds().contains(mousePosF)) {
                                musicMuted = !musicMuted;

                                // music.setVolume(musicMuted ? 0.f : (float)musicVolume);
                            }
                        }
                    }



                }

                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter) {
                    lives = 3;
                    score = 0;
                    diamondsCollected = 0;
                    loadLevel(1);
                    state = PLAYING;
                }
            }
            else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) {
                    state = (state == PLAYING) ? PAUSED : (state == PAUSED) ? PLAYING : state;
                }
                if (event.key.code == sf::Keyboard::R && state == PLAYING) {
                    loadLevel(currentLevel);
                }
                if (event.key.code == sf::Keyboard::Enter) {
                    if (state == LEVEL_COMPLETE) {
                        if (currentLevel < 4) {
                            loadLevel(currentLevel + 1);
                            state = PLAYING;
                        }
                        else {
                            state = MENU;
                            menuPage = MAIN_MENU;
                        }
                    }
                    else if (state == GAME_OVER) {
                        state = MENU;
                        menuPage = MAIN_MENU;
                    }
                }
            }
        }
    }

    void update() {
        if (state != PLAYING) return;

        // ------- INPUT: only move when keys are pressed (fix drifting) -------
        player.velocity.x = 0.f;   // reset each frame

        // movement input...
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
            player.velocity.x -= player.speed;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
            player.velocity.x += player.speed;
        }

        // NEW: update facing direction
        if (player.velocity.x > 0.f)  player.facingDir = 1;
        if (player.velocity.x < 0.f)  player.facingDir = -1;

        // NEW: jump input
        if ((sf::Keyboard::isKeyPressed(sf::Keyboard::Space) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Up) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::W)) && player.grounded) {

            player.velocity.y = player.jumpPower; // negative value = go up
            player.grounded = false;
        }


        // ------- PHYSICS: horizontal then vertical with collision --------
        // Horizontal move
        player.position.x += player.velocity.x;
        player.updatePosition();

        for (auto& platform : platforms) {
            sf::FloatRect playerBounds = player.getBounds();
            sf::FloatRect platformBounds = platform.shape.getGlobalBounds();

            if (playerBounds.intersects(platformBounds)) {
                if (player.velocity.x > 0) {
                    player.position.x = platformBounds.left - playerBounds.width;
                }
                else if (player.velocity.x < 0) {
                    player.position.x = platformBounds.left + platformBounds.width;
                }
                player.updatePosition();
            }
        }

        // Vertical move
        player.velocity.y += GRAVITY;
        float vyBefore = player.velocity.y;

        player.position.y += player.velocity.y;

        player.updatePosition();

        player.grounded = false;
        for (auto& platform : platforms) {
            sf::FloatRect playerBounds = player.getBounds();
            sf::FloatRect platformBounds = platform.shape.getGlobalBounds();

            if (playerBounds.intersects(platformBounds)) {
                if (vyBefore > 0) { // falling down onto platform
                    player.position.y = platformBounds.top - playerBounds.height;
                    player.velocity.y = 0;
                    player.grounded = true;
                    player.updatePosition();

                    if (platform.breakable) {
                        platform.breakTimer += 1;
                        if (platform.breakTimer > 120) {
                            platform.shape.setFillColor(sf::Color(168, 216, 234, 150));
                        }
                    }
                }
                else if (vyBefore < 0) { // hitting head
                    player.position.y = platformBounds.top + platformBounds.height;
                    player.velocity.y = 0;
                    player.updatePosition();
                }
            }
        }

        // World bounds (for scrolling world)
        if (player.position.x < 0) player.position.x = 0;
        if (player.position.x + 32.f > WORLD_WIDTH) player.position.x = WORLD_WIDTH - 32.f;

        if (player.position.y > WINDOW_HEIGHT + 200.f) {
            lives--;
            if (lives <= 0) {
                state = GAME_OVER;
            }
            else {
                loadLevel(currentLevel);
            }
            return;
        }

        // Collectables
        for (auto& diamond : diamonds) {
            diamond.update();
            if (!diamond.collected && player.getBounds().intersects(diamond.getBounds())) {
                diamond.collected = true;
                diamondsCollected++;
                score += 50;
            }
        }



        // Hammer pickup: just collect it, show in HUD, and allow door use
        if (hammer && !hammer->collected && player.getBounds().intersects(hammer->getBounds())) {
            hammer->collected = true;   // hammer disappears (render checks !collected)
            player.hasHammer = true;    // HUD now shows "Hammer: YES"
            score += 75;
        }



        // Enemies
        for (auto& enemy : enemies) {
            enemy.update();
            if (player.getBounds().intersects(enemy.getBounds())) {
                lives--;
                if (lives <= 0) {
                    state = GAME_OVER;
                }
                else {
                    loadLevel(currentLevel);
                }
                return;
            }
        }

        // Hazards (if you add them later)
        for (auto& rock : fallingRocks) {
            rock.update(player.getBounds());
            if (rock.active && player.getBounds().intersects(rock.getBounds())) {
                lives--;
                if (lives <= 0) {
                    state = GAME_OVER;
                }
                else {
                    loadLevel(currentLevel);
                }
                return;
            }
        }

        for (auto& icicle : icicles) {
            icicle.update(player.getBounds());
            if (icicle.falling && player.getBounds().intersects(icicle.getBounds())) {
                lives--;
                if (lives <= 0) {
                    state = GAME_OVER;
                }
                else {
                    loadLevel(currentLevel);
                }
                return;
            }
        }

        for (auto& lava : lavaPools) {
            lava.update();
            if (player.getBounds().intersects(lava.getBounds())) {
                lives--;
                if (lives <= 0) {
                    state = GAME_OVER;
                }
                else {
                    loadLevel(currentLevel);
                }
                return;
            }
        }

        // Exit condition: player just needs the hammer and to touch the door
        if (player.hasHammer &&
            player.getBounds().intersects(exitDoor.getGlobalBounds())) {

            state = LEVEL_COMPLETE;
        }


        // Camera follow
        float camX = player.position.x + 16.f;
        camX = std::max(400.f, std::min(camX, WORLD_WIDTH - 400.f));
        view.setCenter(camX, 300.f);
    }

    void drawMenu() {
        window.setView(window.getDefaultView());

      
        //  Animated Gradient Background + Particles
        
        sf::RectangleShape bgGradient(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
        bgGradient.setFillColor(sf::Color(15, 10, 35));  // Deep purple
        window.draw(bgGradient);

        // ADDED: Update and draw all animated particles/diamonds every frame
        updateMenuAnimation();
        for (auto& p : menuParticles) window.draw(p.shape);
        for (auto& d : floatingDiamonds) window.draw(d.shape);

        
        // Bouncing Title with Glow Effect
        
        if (fontLoaded) {
            // ADDED: Glowing shadow layer (behind main title)
            sf::Text titleGlow;
            titleGlow.setFont(font);
            titleGlow.setString("OREO ESCAPE");
            titleGlow.setCharacterSize(72);  // Bigger than before!
            titleGlow.setFillColor(sf::Color(255, 215, 0, static_cast<sf::Uint8>(glowPulse)));
            titleGlow.setOutlineThickness(8.f);  // Thick glow
            titleGlow.setOutlineColor(sf::Color(255, 150, 0, static_cast<sf::Uint8>(glowPulse * 0.5f)));
            titleGlow.setPosition(175 + titleBounce * 0.5f, 35 + titleBounce);  // Bounces!
            window.draw(titleGlow);

            // ADDED: Main title on top
            sf::Text title;
            title.setFont(font);
            title.setString("OREO ESCAPE");
            title.setCharacterSize(72);
            title.setFillColor(sf::Color(255, 235, 100));  // Bright gold
            title.setOutlineThickness(4.f);
            title.setOutlineColor(sf::Color(180, 100, 0));
            title.setPosition(180, 40 + titleBounce);  // Bounces
            window.draw(title);

            // SETTINGS UI (ADDED)
// ================================
            if (menuPage == SETTINGS_PAGE) {
                window.draw(volDownButton);
                window.draw(volUpButton);
                window.draw(muteButton);

                if (fontLoaded) {
                    sf::Text t;
                    t.setFont(font);
                    t.setCharacterSize(18);
                    t.setFillColor(sf::Color::White);

                    // "-" label
                    t.setString("-");
                    t.setPosition(volDownButton.getPosition().x + 18.f, volDownButton.getPosition().y + 5.f);
                    window.draw(t);

                    // "+" label
                    t.setString("+");
                    t.setPosition(volUpButton.getPosition().x + 16.f, volUpButton.getPosition().y + 5.f);
                    window.draw(t);

                    // Mute label
                    t.setString(musicMuted ? "UNMUTE" : "MUTE");
                    t.setPosition(muteButton.getPosition().x + 35.f, muteButton.getPosition().y + 5.f);
                    window.draw(t);

                    // Volume display text
                    sf::Text volText;
                    volText.setFont(font);
                    volText.setCharacterSize(18);
                    volText.setFillColor(sf::Color(255, 235, 150));
                    volText.setString("Music Volume: " + std::to_string(musicMuted ? 0 : musicVolume));
                    volText.setPosition(200.f, 300.f);
                    window.draw(volText);
                }
            }


            // Subtitle (keeps your page-specific strings)
            sf::Text subtitle;
            subtitle.setFont(font);
            subtitle.setCharacterSize(22);
            subtitle.setFillColor(sf::Color(210, 210, 210));
            subtitle.setPosition(245, 125);

            if (menuPage == MAIN_MENU) {
                subtitle.setString("Cave Adventure Platformer");
            }
            else if (menuPage == MAP_PAGE) {
                subtitle.setString("Map of the Cave");
            }
            else if (menuPage == SETTINGS_PAGE) {
                subtitle.setString("Settings");
            }
            else if (menuPage == INSTRUCTIONS_PAGE) {
                subtitle.setString("How to Play");
            }
            else if (menuPage == SHOP_PAGE) {
                subtitle.setString("Shop");
            }

            window.draw(subtitle);
        }

      
        // MAIN MENU PAGE
      
        if (menuPage == MAIN_MENU) {
            window.draw(menuPanel);

            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            sf::Vector2f mousePosF(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));

         
            // Interactive Button Hover Effects
         
            auto styleButton = [&](sf::RectangleShape& button, const std::string& label,
                bool primary, float yPos) {
                    bool hovered = button.getGlobalBounds().contains(mousePosF);

                    // Always reset to default state first (fixes disappearing/offset bugs)
                    button.setOrigin(0, 0);
                    button.setPosition(270.f, yPos);
                    button.setScale(1.f, 1.f);

                    if (hovered) {
                        // Brighter colors when hovering
                        if (primary) {
                            button.setFillColor(sf::Color(255, 200, 20));
                            button.setOutlineColor(sf::Color(255, 220, 80));
                        }
                        else {
                            button.setFillColor(sf::Color(70, 110, 180));
                            button.setOutlineColor(sf::Color(150, 190, 255));
                        }
                        button.setOutlineThickness(4.f);  // Thicker when hovering
                    }
                    else {
                        // Normal colors
                        if (primary) {
                            button.setFillColor(sf::Color(255, 180, 0));
                            button.setOutlineColor(sf::Color(200, 140, 0));
                        }
                        else {
                            button.setFillColor(sf::Color(50, 80, 130));
                            button.setOutlineColor(sf::Color(100, 150, 220));
                        }
                        button.setOutlineThickness(3.f);
                    }

                    window.draw(button);

                    // Draw label text on top of button
                    if (fontLoaded) {
                        sf::Text t;
                        t.setFont(font);
                        t.setString(label);
                        t.setCharacterSize(primary ? 22 : 20);
                        t.setFillColor(primary ? sf::Color::Black : sf::Color::White);

                        // Simple centering
                        sf::FloatRect tb = t.getLocalBounds();
                        sf::Vector2f bp = button.getPosition();
                        sf::Vector2f bs = button.getSize();
                        t.setPosition(
                            bp.x + (bs.x - tb.width) / 2.f - tb.left,
                            bp.y + (bs.y - tb.height) / 2.f - tb.top - 2.f
                        );

                        window.draw(t);
                    }
                };

            // ADDED: Call with Y positions to fix button placement
            styleButton(mapButton, "MAP", false, 175);
            styleButton(settingsButton, "SETTINGS", false, 230);
            styleButton(instructionsButton, "INSTRUCTIONS", false, 285);
            styleButton(shopButton, "SHOP", false, 340);
            styleButton(startButton, "START ADVENTURE", true, 410);

            // ============================================================
            // ✨ ENHANCEMENT #6D: Pulsing Hint Text
            // ============================================================
            if (fontLoaded) {
                sf::Text hint;
                hint.setFont(font);
                hint.setString("Press ENTER or click START to begin");
                hint.setCharacterSize(18);
                hint.setFillColor(sf::Color(180, 200, 255, 200));
                hint.setPosition(220, 550);

                // Pulsing fade effect using sine wave
                float pulse = 200.f + std::sin(menuAnimTime * 4.f) * 55.f;
                sf::Color hintColor = hint.getFillColor();
                hintColor.a = static_cast<sf::Uint8>(pulse);
                hint.setFillColor(hintColor);

                window.draw(hint);
            }
        }
        // OTHER PAGES (keeps your existing panels/content)
        else {
            sf::RectangleShape panel(sf::Vector2f(500, 320));
            panel.setPosition(150, 170);
            panel.setFillColor(sf::Color(0, 0, 0, 220));
            panel.setOutlineThickness(2.f);
            panel.setOutlineColor(sf::Color(200, 200, 200));
            window.draw(panel);

            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            sf::Vector2f mousePosF(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
            bool backHovered = backButton.getGlobalBounds().contains(mousePosF);

            if (backHovered) {
                sf::Color c = backButton.getFillColor();
                c.r = std::min(255, c.r + 40);
                c.g = std::min(255, c.g + 40);
                c.b = std::min(255, c.b + 40);
                backButton.setFillColor(c);
                backButton.setOutlineThickness(3.f);
            }
            else {
                backButton.setFillColor(sf::Color(80, 80, 80));
                backButton.setOutlineThickness(2.f);
            }

            window.draw(backButton);

            if (fontLoaded) {
                sf::Text backText;
                backText.setFont(font);
                backText.setString("BACK");
                backText.setCharacterSize(18);
                backText.setFillColor(sf::Color::White);
                backText.setPosition(backButton.getPosition().x + 30.f, backButton.getPosition().y + 5.f);
                window.draw(backText);

                sf::Text content;
                content.setFont(font);
                content.setCharacterSize(14);
                content.setFillColor(sf::Color::White);
                content.setLineSpacing(1.3f);
                content.setPosition(170, 190);

                if (menuPage == INSTRUCTIONS_PAGE) {
                    content.setString(
                        "HOW TO PLAY\n"
                        "--------------------------------------------------------------\n"
                        "Arrow Keys / A, D  : Move left/right\n"
                        "Space / W / Up     : Jump\n"
                        "E                  : Use Hammer on Boulder\n"
                        "ESC                : Pause game\n"
                        "R                  : Restart current level\n\n"
                        "Goal:\n"
                        "- Collect diamonds\n"
                        "- Avoid enemies and hazards\n"
                        "- Break the boulder with your hammer\n"
                        "- Reach the glowing exit door!"
                    );
                }
                else if (menuPage == MAP_PAGE) {
                    content.setString(
                        "MAP OF THE CAVE\n"
                        "--------------------------------------------------------------\n"
                        "Level 1 : Diamond Mine\n"
                        "Level 2 : Diamond Mine\n"
                        "Level 3 : Diamond Mine\n"
                        "Level 4 : Diamond Mine\n\n"
                        "(For now all levels share\n"
                        " the same layout while the\n"
                        " mechanics are being tested.)"
                    );
                }
                else if (menuPage == SETTINGS_PAGE) {
                    content.setString(
                        "SETTINGS\n"
                        "---------------------------------------------------------------\n"
                        "(Placeholder - to be implemented)\n\n"
                        "- Music Volume\n"
                        "- Sound Effects Volume\n"
                        "- Visual settings (e.g. brightness)\n\n"
                        "These options will be connected to\n"
                        "real audio and display systems later."
                    );
                }
                else if (menuPage == SHOP_PAGE) {
                    content.setString(
                        "SHOP\n"
                        "---------------------------------------------------------------\n"
                        "(Placeholder - to be implemented)\n\n"
                        "Use collected diamonds to buy:\n"
                        "- Temporary power-ups\n"
                        "- Extra lives\n"
                        "- Cosmetic outfits for Oreo\n\n"
                        "Shop items and prices will be added\n"
                        "in a future update."
                    );
                }

                window.draw(content);
            }
        }
    }


    void drawHUD() {
        // Extra height so all lines fit comfortably
        float hudHeight = player.hasHammer ? 170.f : 150.f;

        sf::RectangleShape hudFrame(sf::Vector2f(230.f, hudHeight));
        hudFrame.setPosition(12.f, 12.f);
        hudFrame.setFillColor(sf::Color(25, 15, 25, 230));
        hudFrame.setOutlineThickness(3.f);
        hudFrame.setOutlineColor(sf::Color(255, 215, 120, 230));
        window.draw(hudFrame);

        sf::RectangleShape inner(sf::Vector2f(220.f, hudHeight - 10.f));
        inner.setPosition(17.f, 17.f);
        inner.setFillColor(sf::Color(40, 24, 40, 220));
        window.draw(inner);

        if (fontLoaded) {
            // Title
            sf::Text title;
            title.setFont(font);
            title.setCharacterSize(16);
            title.setFillColor(sf::Color(255, 215, 0));
            title.setString("CAVE STATUS");
            title.setPosition(25.f, 24.f);
            window.draw(title);

            // Main stats
            sf::Text text;
            text.setFont(font);
            text.setCharacterSize(18);
            text.setFillColor(sf::Color(255, 245, 220));
            text.setLineSpacing(1.3f);
            text.setPosition(25.f, 50.f);   // moved further down

            std::stringstream ss;
            ss << "Level: " << currentLevel << " / 4\n";
            ss << "Lives: " << lives << "\n";
            ss << "Diamonds: " << diamondsCollected << "\n";
            ss << "Score: " << score;
            if (player.hasHammer) {
                ss << "\nHammer: READY";
            }

            text.setString(ss.str());
            window.draw(text);
        }
    }


    void drawPauseMenu() {
        sf::RectangleShape overlay(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
        overlay.setFillColor(sf::Color(0, 0, 0, 200));
        window.draw(overlay);

        if (fontLoaded) {
            sf::Text text;
            text.setFont(font);
            text.setString("PAUSED\n\nESC - Resume\nR - Restart Level");
            text.setCharacterSize(40);
            text.setFillColor(sf::Color::White);
            text.setPosition(280, 220);
            window.draw(text);
        }
    }

    void drawGameOver() {
        sf::RectangleShape overlay(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
        overlay.setFillColor(sf::Color(0, 0, 0, 200));
        window.draw(overlay);

        if (fontLoaded) {
            sf::Text text;
            text.setFont(font);
            std::stringstream ss;
            ss << "GAME OVER\n\n";
            ss << "Final Score: " << score << "\n";
            ss << "Diamonds: " << diamondsCollected << "\n\n";
            ss << "Press ENTER to Menu";
            text.setString(ss.str());
            text.setCharacterSize(36);
            text.setFillColor(sf::Color::Red);
            text.setPosition(220, 180);
            window.draw(text);
        }
    }

    void drawLevelComplete() {
        sf::RectangleShape overlay(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
        overlay.setFillColor(sf::Color(0, 0, 0, 200));
        window.draw(overlay);

        if (fontLoaded) {
            sf::Text text;
            text.setFont(font);
            std::stringstream ss;
            ss << "LEVEL COMPLETE!\n\n";
            ss << "Score: " << score << "\n";
            ss << "Diamonds: " << diamondsCollected << "\n\n";
            if (currentLevel < 4) {
                ss << "Press ENTER for\nNext Level";
            }
            else {
                ss << "YOU WIN!\nAll Levels Complete!\n\n";
                ss << "Press ENTER for Menu";
            }
            text.setString(ss.str());
            text.setCharacterSize(36);
            text.setFillColor(sf::Color::Yellow);
            text.setPosition(220, 150);
            window.draw(text);
        }
    }

    void render() {
        if (state == MENU) {
            // --- MENU SCREEN ---
            window.setView(window.getDefaultView());
            window.clear(sf::Color(30, 30, 50));  // menu background colour
            drawMenu();
        }
        else {
            // --- GAMEPLAY ---
            window.clear();

            // 1) Draw background in screen space (full window)
            // Draw correct background for each level
            window.setView(window.getDefaultView());
            int bgIndex = currentLevel - 1; // 0–3

            if (bgIndex >= 0 && bgIndex < 4 && bgLoaded[bgIndex]) {
                window.draw(bgSprites[bgIndex]);
            }
            else {
                // fallback if missing image
                sf::RectangleShape bgRect(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
                bgRect.setFillColor(bgColor);
                window.draw(bgRect);
            }


            // 2) Draw world with scrolling camera
            window.setView(view);

            for (auto& lava : lavaPools) {
                window.draw(lava.shape);
            }

            for (auto& platform : platforms) {
                window.draw(platform.shape);
            }

            for (auto& diamond : diamonds) {
                diamond.draw(window);
            }


            if (hammer && !hammer->collected) {
                hammer->draw(window);
            }

            // Exit door
            window.draw(exitDoor);

            for (auto& rock : fallingRocks) {
                if (rock.active || rock.resetTimer > 0) {
                    window.draw(rock.shape);
                }
            }

            for (auto& icicle : icicles) {
                window.draw(icicle.shape);
            }

            for (auto& enemy : enemies) {
                enemy.draw(window);
            }

            player.draw(window);

            // 3) HUD & overlays in screen-space again
            window.setView(window.getDefaultView());
            drawHUD();

            if (state == PAUSED)        drawPauseMenu();
            if (state == GAME_OVER)     drawGameOver();
            if (state == LEVEL_COMPLETE) drawLevelComplete();
        }

        // ALWAYS display once per frame
        window.display();
    }


    void run() {
        while (window.isOpen()) {
            handleInput();
            update();
            render();
        }
    }
};

int main() {
    Game game;
    game.run();
    return 0;
}