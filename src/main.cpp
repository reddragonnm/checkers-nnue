#include <SFML/Graphics.hpp>

#include <iostream>
#include <bitset>

#include "Checkers.hpp"

constexpr int squareSize{ 100 };

void displayGrid(sf::RenderWindow& window) {
    static sf::RectangleShape rect{ {squareSize, squareSize} };

    for (int i{ 0 }; i < 8; i++) {
        for (int j{ 0 }; j < 8; j++) {
            rect.setFillColor(
                ((i + j) % 2 == 0)
                ? sf::Color{ 242, 218, 174 }
                : sf::Color{ 182, 118, 83 }
            );
            rect.setPosition({ static_cast<float>(i * squareSize), static_cast<float>(j * squareSize) });
            window.draw(rect);
        }
    }
}

void displayBoard(const Checkers& board, sf::RenderWindow& window) {
    float circleRadius{ 0.75f * static_cast<float>(squareSize) / 2.f };
    static sf::CircleShape circle{ circleRadius };
    circle.setOrigin({ circleRadius, circleRadius });

    std::uint64_t idx{ 1 };
    auto darkBoard{ board.getDarkPieces() };
    auto lightBoard{ board.getLightPieces() };


    circle.setFillColor(sf::Color{ 80, 52, 41 });
    for (int i{ 0 }; i < 64; i++) {
        if ((idx << i) & darkBoard) {
            circle.setPosition({
                squareSize / 2.f + squareSize * ((63 - i) % 8),
                squareSize / 2.f + squareSize * ((63 - i) / 8)
                });
            window.draw(circle);
        }
    }

    circle.setFillColor(sf::Color{ 219, 172, 126 });
    for (int i{ 0 }; i < 64; i++) {
        if ((idx << i) & lightBoard) {
            circle.setPosition({
                squareSize / 2.f + squareSize * ((63 - i) % 8),
                squareSize / 2.f + squareSize * ((63 - i) / 8)
                });
            window.draw(circle);
        }
    }
}

void displayValidMoves(const Checkers& board, sf::RenderWindow& window, int selected) {
    auto moves{ board.getMoves() };

    for (const auto& move : moves) {

        if (move & (1 << 13)) { // capture

        }
        else { // move
            if (static_cast<int>(move >> 6) == 63 - selected) {
                auto toSq{ 63 - static_cast<int>(0x3f & move) };

                sf::RectangleShape rect{ {squareSize, squareSize} };
                rect.setFillColor(sf::Color::Red);
                rect.setPosition({
                    static_cast<float>(toSq % 8) * squareSize,
                    static_cast<float>(toSq / 8) * squareSize
                    });

                window.draw(rect);
            }
        }
    }
}

int main()
{
    int selected{ -1 };
    constexpr int windowSize{ 8 * squareSize };
    sf::RenderWindow window(sf::VideoMode({ windowSize, windowSize }), "SFML");

    Checkers board{};
    board.generateMoves();

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            else if (const auto* mouseButtonPressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouseButtonPressed->button == sf::Mouse::Button::Left) {
                    selected = 8 * (mouseButtonPressed->position.y / squareSize) + (mouseButtonPressed->position.x / squareSize);
                }
            }
        }

        window.clear();

        displayGrid(window);

        if (selected != -1) {
            sf::RectangleShape selectedRect{ {squareSize, squareSize} };
            selectedRect.setFillColor(sf::Color::Green);
            selectedRect.setPosition({
                static_cast<float>(selected % 8) * squareSize,
                static_cast<float>(selected / 8) * squareSize
                });

            window.draw(selectedRect);

            displayValidMoves(board, window, selected);
        }

        displayBoard(board, window);

        window.display();
    }
}
