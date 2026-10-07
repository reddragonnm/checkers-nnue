#include <SFML/Graphics.hpp>

#include <iostream>

#include "headers/Checkers.hpp"
#include "headers/AIPlayer.hpp"
#include "headers/EGTB.hpp"
#include "headers/NNUE.hpp"
#include "headers/NNUEInference.hpp"

constexpr int squareSize{ 100 };
constexpr int searchTime{ 3000 }; // milliseconds

int displaySquare(int square, bool flipBoard) {
    return flipBoard ? square : 63 - square;
}

void logMove(const char* player, const Checkers& board, int index, std::uint16_t move) {
    std::cout << "MOVE " << player << " index=" << index
        << " from=" << board.getFromSquare(move)
        << " to=" << board.getToSquare(move)
        << " capture=" << board.isCaptureMove(move)
        << " hash=" << board.hash()
        << " drawCounter=" << board.getDrawCounter() << '\n';
}

void displayGrid(sf::RenderWindow& window) {
    static sf::RectangleShape rect{ { squareSize, squareSize } };

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

void displayBoard(const Checkers& board, sf::RenderWindow& window, bool flipBoard) {
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
            auto square{ displaySquare(i, flipBoard) };
            circle.setPosition({ squareSize / 2.f + squareSize * (square % 8),
                                squareSize / 2.f + squareSize * (square / 8) });
            window.draw(circle);
        }
    }

    circle.setFillColor(sf::Color{ 219, 172, 126 });
    for (int i{ 0 }; i < 64; ++i) {
        if ((idx << i) & lightBoard) {
            auto square{ displaySquare(i, flipBoard) };
            circle.setPosition({ squareSize / 2.f + squareSize * (square % 8),
                                squareSize / 2.f + squareSize * (square / 8) });
            window.draw(circle);
        }
    }

    float circle2Radius{ 0.3f * static_cast<float>(squareSize) / 2.f };
    sf::CircleShape circle2{ circle2Radius };
    circle2.setOrigin({ circle2Radius, circle2Radius });
    circle2.setFillColor(sf::Color::Yellow);
    for (int i{ 0 }; i < 64; ++i) {
        if ((idx << i) & kingPieces) {
            auto square{ displaySquare(i, flipBoard) };
            circle2.setPosition({ squareSize / 2.f + squareSize * (square % 8),
                                 squareSize / 2.f + squareSize * (square / 8) });
            window.draw(circle2);
        }
    }
}

void displayValidMoves(const Checkers& board, sf::RenderWindow& window, int selected,
    bool flipBoard) {
    const int numMoves{ board.getNumMoves() };
    const auto& moves{ board.getMoves() };

    for (int i{ 0 }; i < numMoves; i++) {
        const auto& move = moves[i];
        if (board.isCaptureMove(move)) { // capture
            if (selected == displaySquare(board.getFromSquare(move), flipBoard)) {
                auto toSq{ displaySquare(board.getToSquare(move), flipBoard) };

                sf::RectangleShape rect{ { squareSize, squareSize } };
                rect.setFillColor(sf::Color::Red);
                rect.setPosition({ static_cast<float>(toSq % 8) * squareSize,
                                  static_cast<float>(toSq / 8) * squareSize });

                window.draw(rect);
            }
        }
        else { // move
            if (selected == displaySquare(board.getFromSquare(move), flipBoard)) {
                auto toSq{ displaySquare(board.getToSquare(move), flipBoard) };

                sf::RectangleShape rect{ { squareSize, squareSize } };
                rect.setFillColor(sf::Color::Red);
                rect.setPosition({ static_cast<float>(toSq % 8) * squareSize,
                                  static_cast<float>(toSq / 8) * squareSize });

                window.draw(rect);
            }
        }
    }
}

std::vector<int> attemptToMakeMove(int selected, int newPos, Checkers& board, AIPlayer& ai,
    bool& gameOver, bool humanFirst, bool flipBoard) {
    if (selected == -1 || gameOver || board.isDarkTurn() != humanFirst)
        return {};

    auto moves{ board.getMoves() };
    int numMoves{ board.getNumMoves() };
    for (int i = 0; i < numMoves; ++i) {
        if ((selected == displaySquare(board.getFromSquare(moves[i]), flipBoard)) &&
            (newPos == displaySquare(board.getToSquare(moves[i]), flipBoard))) {
            board.makeMove(i);
            logMove("human", board, i, moves[i]);
            if (board.isDraw()) {
                std::cout << "DRAW\n";
                gameOver = true;
                return {};
            }
            if (board.isDarkTurn() != humanFirst) {
                auto res{ ai.search(searchTime, false, true) };
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

    NNUE nnue{ { 128, 256, 32, 1 } }; nnue.load("nnue_22000.bin");
    NNUEInference nnueInference{ nnue };

    Checkers board{ &nnueInference };
    AIPlayer ai{ board, egtb, nnueInference };

    bool gameOver{ false };

    std::vector<int> aiPendingMoves;
    sf::Clock aiTimer;
    const sf::Time moveDelay{ sf::milliseconds(200) };

    bool humanFirst{ true };
    std::cout << "Do you want to play first? (y/n): ";
    char input;
    std::cin >> input;
    if (input == 'n' || input == 'N') {
        humanFirst = false;
    }

    bool flipBoard{ false };
    std::cout << "Do you want to flip the board? (y/n): ";
    std::cin >> input;
    if (input == 'y' || input == 'Y') {
        flipBoard = true;
    }
    std::cout << "Move log uses engine square numbers 0-63. Initial hash=" << board.hash() << '\n';

    if (!humanFirst) {
        aiPendingMoves = ai.search(searchTime, false, true).pv;
        aiTimer.restart();
    }

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();

            else if (const auto* mouseButtonPressed =
                event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouseButtonPressed->button == sf::Mouse::Button::Left) {
                    int pos{ 8 * (mouseButtonPressed->position.y / squareSize) +
                            (mouseButtonPressed->position.x / squareSize) };

                    std::vector<int> path{ attemptToMakeMove(selected, pos, board, ai, gameOver,
                        humanFirst, flipBoard) };
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
                    std::cout << "UNDO hash=" << board.hash() << '\n';
                    gameOver = false;
                }
            }
        }

        if (!aiPendingMoves.empty() && aiTimer.getElapsedTime() > moveDelay) {
            int index = aiPendingMoves.front();
            auto move = board.getMoves()[index];
            board.makeMove(index);
            logMove("ai", board, index, move);
            aiPendingMoves.erase(aiPendingMoves.begin());
            aiTimer.restart();

            if (board.isDraw()) {
                std::cout << "DRAW!\n";
                gameOver = true;
            }
            else if (board.isDarkTurn() && board.getNumMoves() == 0) {
                std::cout << "AI WINS!\n";
                gameOver = true;
            }
        }

        window.clear();

        displayGrid(window);

        if (selected != -1) {
            sf::RectangleShape selectedRect{ { squareSize, squareSize } };
            selectedRect.setFillColor(sf::Color::Green);
            selectedRect.setPosition({ static_cast<float>(selected % 8) * squareSize,
                                      static_cast<float>(selected / 8) * squareSize });

            window.draw(selectedRect);

            displayValidMoves(board, window, selected, flipBoard);
        }

        displayBoard(board, window, flipBoard);

        window.display();
    }
}
