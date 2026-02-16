#include <iostream>

#include <SFML/Graphics.hpp>
#include "SFML/System.hpp"

const int WIDTH = 256;
const int HEIGHT = 256;

std::vector<std::uint8_t> grid(WIDTH* HEIGHT, 0);

std::vector<uint8_t> pixels(WIDTH* HEIGHT * 4, 0);

void stepSim()
{
    for (unsigned i = 0; i < WIDTH * HEIGHT; ++i)
    {
        // Coloring everything with some coral/orange
        pixels[i * 4 + 0] = 255; // R
        pixels[i * 4 + 1] = 100; // G
        pixels[i * 4 + 2] = 50;  // B
        pixels[i * 4 + 3] = 255; // A
    }
}

int main()
{
    sf::RenderWindow window(sf::VideoMode({ WIDTH, HEIGHT }), "Falling sand simulation");

    sf::Texture texture(sf::Vector2u(WIDTH, HEIGHT));
    texture.update(pixels.data());

    sf::Sprite sprite(texture);

    while (window.isOpen())
    {
        while (const std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        stepSim();
        texture.update(pixels.data());

        window.clear();
        window.draw(sprite);
        window.display();
    }
}