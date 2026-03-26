#include <SFML/Graphics.hpp>

#include <iostream>

#include "headers/Checkers.hpp"
#include "headers/AIPlayer.hpp"
#include "headers/EGTB.hpp"
#include "headers/NNUE.hpp"
#include "headers/NNUEInference.hpp"

constexpr int squareSize{ 100 };

void displayGrid(sf::RenderWindow& window) {
    static sf::RectangleShape rect{ {squareSize, squareSize} };

    for (int i{ 0 }; i < 8; ++i) {
        for (int j{ 0 }; j < 8; ++j) {
            rect.setFillColor(((i + j) % 2 == 0) ? sf::Color{ 242, 218, 174 }
            : sf::Color{ 182, 118, 83 });
            rect.setPosition(
                { static_cast<float>(i * squareSize), static_cast<float>(j * squareSize) });
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
    auto kingPieces{ board.getKingPieces() };

    circle.setFillColor(sf::Color{ 80, 52, 41 });
    for (int i{ 0 }; i < 64; ++i) {
        if ((idx << i) & darkBoard) {
            circle.setPosition({ squareSize / 2.f + squareSize * ((63 - i) % 8),
                                squareSize / 2.f + squareSize * ((63 - i) / 8) });
            window.draw(circle);
        }
    }

    circle.setFillColor(sf::Color{ 219, 172, 126 });
    for (int i{ 0 }; i < 64; ++i) {
        if ((idx << i) & lightBoard) {
            circle.setPosition({ squareSize / 2.f + squareSize * ((63 - i) % 8),
                                squareSize / 2.f + squareSize * ((63 - i) / 8) });
            window.draw(circle);
        }
    }

    float circle2Radius{ 0.3f * static_cast<float>(squareSize) / 2.f };
    sf::CircleShape circle2{ circle2Radius };
    circle2.setOrigin({ circle2Radius, circle2Radius });
    circle2.setFillColor(sf::Color::Yellow);
    for (int i{ 0 }; i < 64; ++i) {
        if ((idx << i) & kingPieces) {
            circle2.setPosition({ squareSize / 2.f + squareSize * ((63 - i) % 8),
                                 squareSize / 2.f + squareSize * ((63 - i) / 8) });
            window.draw(circle2);
        }
    }
}

void displayValidMoves(const Checkers& board, sf::RenderWindow& window, int selected) {
    const int numMoves{ board.getNumMoves() };
    const auto& moves{ board.getMoves() };

    for (int i{ 0 }; i < numMoves; i++) {
        const auto& move = moves[i];
        if (board.isCaptureMove(move)) { // capture
            if (selected == 63 - board.getFromSquare(move)) {
                auto toSq{ 63 - board.getToSquare(move) };

                sf::RectangleShape rect{ {squareSize, squareSize} };
                rect.setFillColor(sf::Color::Red);
                rect.setPosition({ static_cast<float>(toSq % 8) * squareSize,
                                  static_cast<float>(toSq / 8) * squareSize });

                window.draw(rect);
            }
        }
        else { // move
            if (selected == 63 - board.getFromSquare(move)) {
                auto toSq{ 63 - board.getToSquare(move) };

                sf::RectangleShape rect{ {squareSize, squareSize} };
                rect.setFillColor(sf::Color::Red);
                rect.setPosition({ static_cast<float>(toSq % 8) * squareSize,
                                  static_cast<float>(toSq / 8) * squareSize });

                window.draw(rect);
            }
        }
    }
}

std::vector<int> attemptToMakeMove(int selected, int newPos, Checkers& board, AIPlayer& ai,
    bool& gameOver) {
    if (selected == -1 || gameOver)
        return {};

    auto moves{ board.getMoves() };
    int numMoves{ board.getNumMoves() };
    for (int i = 0; i < numMoves; ++i) {
        if ((selected == 63 - board.getFromSquare(moves[i])) &&
            (newPos == 63 - board.getToSquare(moves[i]))) {
            board.makeMove(i);
            if (!board.isDarkTurn()) {
                auto res{ ai.search(1000, false, true) };
                if (res.pv.empty()) {
                    std::cout << "YOU WIN!\n";
                    gameOver = true;
                }
                return res.pv;
            }
            else
                return {};
        }
    }

    return {};
}

int main() {
    int selected{ -1 };
    constexpr int windowSize{ 8 * squareSize };
    sf::RenderWindow window(sf::VideoMode({ windowSize, windowSize }), "SFML");

    EGTB egtb;
    egtb.buildOrLoad("egtb.bin", "egtb_dtz.bin");

    NNUE nnue{ {128, 256, 32, 1} }; nnue.load("nnue_best.bin");
    NNUEInference nnueInference{ nnue };

    Checkers board{ &nnueInference };
    AIPlayer ai{ board, egtb, nnueInference };

    bool gameOver{ false };

    std::vector<int> aiPendingMoves;
    sf::Clock aiTimer;
    const sf::Time moveDelay{ sf::milliseconds(200) };

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();

            else if (const auto* mouseButtonPressed =
                event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouseButtonPressed->button == sf::Mouse::Button::Left) {
                    int pos{ 8 * (mouseButtonPressed->position.y / squareSize) +
                            (mouseButtonPressed->position.x / squareSize) };

                    std::vector<int> path{ attemptToMakeMove(selected, pos, board, ai, gameOver) };
                    if (!path.empty()) {
                        aiPendingMoves = path;
                        aiTimer.restart();
                        selected = -1;
                    }
                    else {
                        selected = pos;
                    }
                }
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::U) {
                    board.undoMove();
                }
            }
        }

        if (!aiPendingMoves.empty() && aiTimer.getElapsedTime() > moveDelay) {
            board.makeMove(aiPendingMoves.front());
            aiPendingMoves.erase(aiPendingMoves.begin());
            aiTimer.restart();

            if (board.isDarkTurn() && board.getNumMoves() == 0) {
                std::cout << "AI WINS!\n";
                gameOver = true;
            }
        }

        window.clear();

        displayGrid(window);

        if (selected != -1) {
            sf::RectangleShape selectedRect{ {squareSize, squareSize} };
            selectedRect.setFillColor(sf::Color::Green);
            selectedRect.setPosition({ static_cast<float>(selected % 8) * squareSize,
                                      static_cast<float>(selected / 8) * squareSize });

            window.draw(selectedRect);

            displayValidMoves(board, window, selected);
        }

        displayBoard(board, window);

        window.display();
    }
}
