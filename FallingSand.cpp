#include <iostream>

#include <SFML/Graphics.hpp>
#include "SFML/System.hpp"

const int WIDTH = 256;
const int HEIGHT = 256;
const int TEXT_SIZE = 14;

const float FPS_UPDATE_INTERVAL = 0.016f;

std::vector<uint8_t> grid(WIDTH * HEIGHT, 0);
std::vector<uint8_t> pixels(WIDTH * HEIGHT * 4, 0);

inline void stepSim()
{
    //return;
    for (int y = HEIGHT - 2; y >= 0; --y)
    {
        for (int x = 0; x < WIDTH; ++x)
        {
            const int i = y * WIDTH + x;
            const int below = (y + 1) * WIDTH + x;

            if (grid[i] == 1)
            {
                if (below >= WIDTH * HEIGHT)
                {
                    continue;
                }

                if (grid[below] == 0)
                {
                    grid[i] = 0;
                    grid[below] = 1;
                }
                else if (below % WIDTH + 1 < WIDTH && grid[below + 1] == 0)
                {
                    grid[i] = 0;
                    grid[below + 1] = 1;
                }
                else if (below % WIDTH - 1 >= 0 && grid[below - 1] == 0)
                {
                    grid[i] = 0;
                    grid[below - 1] = 1;
                }
            }
        }
    }
}

inline void updatePixelsFromGrid()
{
    //return;
    for (int y = 0; y < HEIGHT; ++y)
    {
        for (int x = 0; x < WIDTH; ++x)
        {
            int i = y * WIDTH + x;

            if (grid[i] == 1) // sand
            {
                pixels[i * 4 + 0] = 255;
                pixels[i * 4 + 1] = 200;
                pixels[i * 4 + 2] = 50;
                pixels[i * 4 + 3] = 255;
            }
            else // empty
            {
                pixels[i * 4 + 0] = 0;
                pixels[i * 4 + 1] = 0;
                pixels[i * 4 + 2] = 0;
                pixels[i * 4 + 3] = 255;
            }
        }
    }
}

inline void parseMouseClick(sf::Vector2i position)
{
    std::cout << "Mouse clicked at :" << position.x << " " << position.y << "\n";

    unsigned int index = position.y * WIDTH + position.x;
    grid[index] = 1;
}

inline void parseMouseInput(sf::RenderWindow& window)
{
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
    {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);

        if (mousePos.x < 0 || mousePos.x >= WIDTH || mousePos.y >= HEIGHT || mousePos.y < 0)
        {
            return;
        }

        parseMouseClick(mousePos);
    }
}

inline void parseAndShowPerformanceMetrics(std::chrono::steady_clock::time_point& lastFrameTime, std::chrono::steady_clock::time_point& fpsTimer, int& frameCount, int& fps, sf::Text& fpsText)
{
    using clock = std::chrono::high_resolution_clock;

    auto now = clock::now();
    float dt = std::chrono::duration<float>(now - lastFrameTime).count();
    lastFrameTime = now;

    frameCount++;

    float elapsed = std::chrono::duration<float>(now - fpsTimer).count();

    if (elapsed >= FPS_UPDATE_INTERVAL)
    {
        fps = static_cast<int>(frameCount / elapsed);
        frameCount = 0;
        fpsTimer = now;

        fpsText.setString("FPS: " + std::to_string(fps));
    }
}

int main()
{
    sf::RenderWindow window(sf::VideoMode({ WIDTH, HEIGHT }), "Falling sand simulation");

    sf::Texture texture(sf::Vector2u(WIDTH, HEIGHT));
    texture.update(pixels.data());
    sf::Sprite sprite(texture);

    sf::Font font;
    std::filesystem::path fontPath = "arial.ttf";
    if (!font.openFromFile(fontPath))
    {
        std::cerr << "Failed to load font\n";
        return -1;
    }

    sf::Text fpsText(font, "FPS: 0", TEXT_SIZE);
    fpsText.setFillColor(sf::Color::White);
    fpsText.setPosition(sf::Vector2(1.f, 1.f));

    using clock = std::chrono::high_resolution_clock;

    auto lastFrameTime = clock::now();
    auto fpsTimer = clock::now();

    int frameCount = 0;
    int fps = 0;

    while (window.isOpen())
    {
        while (const std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }

        parseAndShowPerformanceMetrics(lastFrameTime, fpsTimer, frameCount, fps, fpsText);

        parseMouseInput(window);
        stepSim();
        updatePixelsFromGrid();
        texture.update(pixels.data());

        window.clear();
        window.draw(sprite);
        window.draw(fpsText);
        window.display();
    }
}