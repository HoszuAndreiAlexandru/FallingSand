#include <iostream>
#include <thread>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

#include <SFML/Graphics.hpp>
#include "SFML/System.hpp"

const char* WINDOW_TITLE = "Falling sand simulation";

const bool USE_MULTITHREADING = false;

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;

const int SIM_SCALE = 1;
const int SIM_WIDTH = WINDOW_WIDTH / SIM_SCALE;
const int SIM_HEIGHT = WINDOW_HEIGHT / SIM_SCALE;

const char* FONT_NAME = "arial.ttf";
const int TEXT_SIZE = 14;

const float SIM_STEP = 1.0f / 60.0f;
const float FPS_UPDATE_INTERVAL = 1.0f / 60.0f;

const int DEFAULT_CACHE_SIZE = 1 << 14;

struct RGBA { uint8_t r, g, b, a; };

struct ParticleData { RGBA color; };

enum Particle
{
    EMPTY,
    SAND,
};

constexpr ParticleData PARTICLE_DATA[] =
{
    /* Empty */ { 0,   0,   0,   255 },
    /* Sand  */ { 255, 200, 50,  255 },
};

struct ThreadPool {
    std::vector<std::thread> workers;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::condition_variable barrier;

    bool stop = false;
    std::atomic<int> activeTasks{ 0 };
    std::function<void(int, int)> task;
    int currentStart, currentEnd, currentChunkSize;

    ThreadPool(size_t threads)
    {
        for (size_t i = 0; i < threads; ++i)
        {
            workers.emplace_back([this]
            {
                while (true)
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] { return this->stop || this->activeTasks > 0; });

                    if (this->stop)
                    {
                        return;
                    }

                    int taskIndex = --activeTasks;
                    lock.unlock();

                    int start = currentStart + (taskIndex * currentChunkSize);
                    int end = std::min(start + currentChunkSize, currentEnd);

                    if (start < end)
                    {
                        task(start, end);
                    }

                    if (activeTasks == 0) barrier.notify_one();
                }
                });
        }
    }

    void execute(int start, int end, std::function<void(int, int)> job)
    {
        int totalItems = end - start;
        if (totalItems <= 0)
        {
            return;
        }

        int numThreads = workers.size();
        currentChunkSize = (totalItems + numThreads - 1) / numThreads;
        currentStart = start;
        currentEnd = end;
        task = job;
        activeTasks = numThreads;

        condition.notify_all();

        // Barrier: Wait until all threads finished the current job
        std::unique_lock<std::mutex> lock(queueMutex);
        barrier.wait(lock, [this] { return activeTasks == 0; });
    }

    ~ThreadPool()
    {
        stop = true;
        condition.notify_all();
        for (std::thread& worker : workers)
        {
            worker.join();
        }
    }
};

#pragma region Rand utils
static uint32_t rngState = 0x12345678;

inline uint32_t xorshift32()
{
    uint32_t x = rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rngState = x;
    return x;
}

inline bool randomBool()
{
    return xorshift32() & 1;
}
#pragma endregion

struct PerformanceMetrics
{
    int frameCount = 0;
    int fps = 0;

    std::chrono::steady_clock::time_point lastFrameTime;
    std::chrono::steady_clock::time_point fpsTimer;
};

struct Frontend
{
    sf::RenderWindow window;
    sf::Texture texture;
    sf::Sprite sprite;
    sf::Font font;
    sf::Text fpsText;

    Frontend(sf::VideoMode mode = sf::VideoMode({ WINDOW_WIDTH, WINDOW_HEIGHT }),
        const std::string& title = WINDOW_TITLE,
        const std::string& fontName = FONT_NAME
    )
        : window(mode, title)
        , texture(sf::Vector2u(SIM_WIDTH, SIM_HEIGHT))
        , sprite(texture)
        , fpsText(font, "FPS: 0", TEXT_SIZE)
    {
        if (!font.openFromFile(fontName))
        {
            throw std::runtime_error("Failed to load font!");
        }

        fpsText.setFillColor(sf::Color::White);
        fpsText.setPosition({ 1.f, 1.f });
    };

    void pollEvents()
    {
        while (const std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }
    };

    void displayNewFrame()
    {
        window.clear();
        window.draw(sprite);
        window.draw(fpsText);
        window.display();
    };

    float parseAndShowPerformanceMetrics(PerformanceMetrics& perf)
    {
        using clock = std::chrono::high_resolution_clock;

        auto now = clock::now();
        float deltaTime = std::chrono::duration<float>(now - perf.lastFrameTime).count();
        perf.lastFrameTime = now;

        perf.frameCount++;

        float elapsed = std::chrono::duration<float>(now - perf.fpsTimer).count();

        if (elapsed >= FPS_UPDATE_INTERVAL)
        {
            perf.fps = static_cast<int>(perf.frameCount / elapsed);
            perf.frameCount = 0;
            perf.fpsTimer = now;

            fpsText.setString("FPS: " + std::to_string(perf.fps));
        }

        return deltaTime;
    }
};

struct Renderer
{
    std::vector<uint8_t> pixels;
    std::vector<int> dirtyPixels;

    ThreadPool pool{ std::thread::hardware_concurrency() };

    void update(std::vector<uint8_t>& grid)
    {
        if (USE_MULTITHREADING)
        {
            updatePixelsFromGridMT(grid);
        }
        else
        {
            updatePixelsFromGrid(grid);
        };
    }

    void dirty(int position)
    {
        dirtyPixels.push_back(position);
    }

    void clear()
    {
        pixels.clear();
        dirtyPixels.clear();
    }

private:
    void updatePixel(int position, Particle particle)
    {
        pixels[position + 0] = PARTICLE_DATA[particle].color.r;
        pixels[position + 1] = PARTICLE_DATA[particle].color.g;
        pixels[position + 2] = PARTICLE_DATA[particle].color.b;
        pixels[position + 3] = PARTICLE_DATA[particle].color.a;
    }

    void updatePixelsFromGridMT(std::vector<uint8_t>& grid)
    {
        const int n = static_cast<int>(dirtyPixels.size());
        if (n == 0)
        {
            return;
        }

        auto workerJob = [&](int start, int end)
            {
                for (int idx = start; idx < end; ++idx)
                {
                    int i = dirtyPixels[idx];
                    int p = i * 4;
                    updatePixel(p, (Particle)grid[i]);
                }
            };

        pool.execute(0, n, workerJob);

        dirtyPixels.clear();
    }

    void updatePixelsFromGrid(std::vector<uint8_t>& grid)
    {
        for (int i : dirtyPixels)
        {
            int p = i * 4;
            updatePixel(p, (Particle)grid[i]);
        }

        dirtyPixels.clear();
    }
};

struct SimulationCache
{
    std::vector<int> activeCellsNow;
    std::vector<int> activeCellsNext;
    std::vector<uint8_t> activeFlag; // For duplicate issue

    void activateCell(int position)
    {
        if (activeFlag[position])
        {
            return;
        }

        activeFlag[position] = 1;
        activeCellsNext.push_back(position);
    }

    void activateCell(int x, int y)
    {
        activateCell(y * SIM_WIDTH + x);
    }

    void activateCell(int x, int y, bool activateNeighborhood)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            int ny = y + dy;
            if (ny < 0 || ny >= SIM_HEIGHT - 1)
            {
                continue;
            }

            int row = ny * SIM_WIDTH;
            for (int dx = -1; dx <= 1; ++dx)
            {
                int nx = x + dx;
                if (nx < 0 || nx >= SIM_WIDTH - 1)
                {
                    continue;
                }

                activateCell(row + nx);
            }
        }
    }

    void reset()
    {
        activeCellsNow.swap(activeCellsNext);
        activeCellsNext.clear();
    }

    void clearAll()
    {
        activeCellsNow.clear();
        activeCellsNext.clear();
        std::fill(activeFlag.begin(), activeFlag.end(), 0);
    }
};

struct Simulation
{
    std::vector<uint8_t> grid;
    std::function<void(int x, int y, int newValue)> modifyCell;

    Simulation(std::vector<uint8_t> g)
        : grid(std::move(g))
    {}

    void step(SimulationCache& cache)
    {
        for (int i : cache.activeCellsNow)
        {
            cache.activeFlag[i] = 0;

            if (grid[i] == Particle::EMPTY) // If space is empty ignore it
            {
                continue;
            }

            int x = i % SIM_WIDTH;
            int y = i / SIM_WIDTH;

            if (y >= SIM_HEIGHT - 1)
            {
                continue;
            }

            int row = y * SIM_WIDTH;

            if (grid[i] == Particle::SAND) SAND_SIM(x, y, row);
        }
    }

    void clear()
    {
        std::fill(grid.begin(), grid.end(), 0);
    }

private:
    void SAND_SIM(int& x, int& y, int& row)
    {
        const int LEFT = -1;
        const int RIGHT = 1;
        const int BELOW = 1;

        int rowBelow = row + SIM_WIDTH;
        int below = rowBelow + x;

        if (grid[below] == Particle::EMPTY)
        {
            modifyCell(x, y, Particle::EMPTY);
            modifyCell(x, y + BELOW, Particle::SAND);
            return;
        }

        bool tryLeft = randomBool();

        if (tryLeft)
        {
            if (x > 0 && grid[below + LEFT] == Particle::EMPTY)
            {
                modifyCell(x, y, Particle::EMPTY);
                modifyCell(x + LEFT, y + BELOW, Particle::SAND);
            }
        }
        else
        {
            if (x < SIM_WIDTH - 1 && grid[below + RIGHT] == Particle::EMPTY)
            {
                modifyCell(x, y, Particle::EMPTY);
                modifyCell(x + RIGHT, y + BELOW, Particle::SAND);
            }
        }
    }
};

struct SimulationOrchestrator
{
    int SIM_SIZE;

    Simulation simulation;
    SimulationCache cache;
    Renderer renderer;

    float accumulator = 0;

    SimulationOrchestrator(int simsize)
        : SIM_SIZE(simsize)
        , cache{}
        , simulation(std::vector<uint8_t>(SIM_SIZE, Particle::EMPTY))
        , renderer{ std::vector<uint8_t>(SIM_SIZE * 4, 0), {} }
        , accumulator(0.f)
    {
        simulation.modifyCell = [this](int x, int y, int value) {
            this->modifyCell(x, y, value);
        };

        init();
    }

    void parseInputs(Frontend& fe)
    {
        parseMouseInput(fe.window);
        parseKeyboardInput(fe.window);
    };

    void execute(float deltaTime, Frontend& fe)
    {
        bool textureNeedsUpdate = false;
        accumulator += deltaTime;

        while (accumulator >= SIM_STEP)
        {
            cache.reset();
            simulation.step(cache);
            accumulator -= SIM_STEP;
            textureNeedsUpdate = true;
        }

        if (textureNeedsUpdate)
        {
            renderer.update(simulation.grid);

            fe.texture.update(renderer.pixels.data());
        }
    };

private:
    void init()
    {
        simulation.grid.resize(SIM_SIZE, Particle::EMPTY);
        cache.activeFlag.resize(SIM_SIZE, 0);
        renderer.pixels.resize(SIM_SIZE * 4, 0);
    }

    void modifyCell(int x, int y, int newValue)
    {
        int position = y * SIM_WIDTH + x;
        simulation.grid[position] = newValue;
        renderer.dirty(position);
        cache.activateCell(x, y, true);
    }

#pragma region Input parsing
    void parseMouseClick(sf::Vector2i position)
    {
        const unsigned int index = position.y * SIM_WIDTH + position.x;

        modifyCell(position.x, position.y, Particle::SAND);
    }

    void parseMouseInput(sf::RenderWindow& window)
    {
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            mousePos.x /= SIM_SCALE;
            mousePos.y /= SIM_SCALE;

            if (mousePos.x < 0 || mousePos.x >= SIM_WIDTH || mousePos.y >= SIM_HEIGHT || mousePos.y < 0)
            {
                return;
            }

            parseMouseClick(mousePos);
        }
    }

    bool keyWasPressed(sf::Keyboard::Key key)
    {
        return sf::Keyboard::isKeyPressed(key);
    }

    void parseKeyboardInput(sf::RenderWindow& window)
    {
        if (keyWasPressed(sf::Keyboard::Key::R))
        {
            simulation.clear();
            renderer.clear();
            cache.clearAll();
        }
    }
#pragma endregion
};

int main()
{
    const int SIM_SIZE = SIM_WIDTH * SIM_HEIGHT;
    using clock = std::chrono::high_resolution_clock;

    SimulationOrchestrator orchestrator{ SIM_SIZE };

    PerformanceMetrics perf{
        0,
        0,
        clock::now(),
        clock::now()
    };

    Frontend fe{
    };

    while (fe.window.isOpen())
    {
        fe.pollEvents();
        orchestrator.execute(fe.parseAndShowPerformanceMetrics(perf), fe);
        orchestrator.parseInputs(fe);
        fe.displayNewFrame();
    }
}