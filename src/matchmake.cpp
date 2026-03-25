#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstring>

#include <SFML/Graphics.hpp>

#include "headers/NNUE.hpp"
#include "headers/Checkers.hpp"
#include "headers/EGTB.hpp"
#include "headers/NNUEInference.hpp"

#include "matchmaking/v1.hpp"
#include "matchmaking/v2.hpp"

// int main() {
//     Checkers board{};
//     EGTB egtb;
//     egtb.buildOrLoad("egtb.bin");

//     NNUE nnueV1{ {128, 256, 32, 1} }; nnueV1.load("nnue_best.bin");
//     NNUEInference nnueInferenceV1{ nnueV1 };

//     NNUE nnueV2{ {128, 256, 32, 1} }; nnueV2.load("nnue_best.bin");
//     NNUEInference nnueInferenceV2{ nnueV2 };

//     int v1Wins{ 0 };
//     int v2Wins{ 0 };
//     int draws{ 0 };

//     auto v1Player{ v1::AIPlayer(board, egtb, nnueInferenceV1) };
//     auto v2Player{ v2::AIPlayer(board, egtb, nnueInferenceV2) }; // piece eval

//     for (int i{ 0 }; i < 1000; i++) {
//         bool v1IsDark{ i % 2 == 0 };
//         int numMoves{ 0 };

//         while (true) {
//             int score;
//             std::vector<int> moves;
//             bool isV1Turn = (board.isDarkTurn() == v1IsDark);

//             if (isV1Turn)
//                 std::tie(score, moves) = v1Player.search(100, false);
//             else
//                 std::tie(score, moves) = v2Player.search(100, false);

//             if (moves.empty()) {
//                 if (isV1Turn) v2Wins++;
//                 else v1Wins++;
//                 break;
//             }

//             for (int m : moves) board.makeMove(m);

//             if ((board.isDarkTurn() == v1IsDark) == isV1Turn) {
//                 std::cerr << "Error: Wrong player moved!\n";
//                 return 1;
//             }

//             if (board.isDraw()) {
//                 draws++;
//                 break;
//             }

//             numMoves++;
//             std::cout << "Game " << i + 1 << ": Move " << numMoves << "\n";
//         }

//         std::cout << "Game " << i + 1 << ": V1 Wins: " << v1Wins << " V2 Wins: " << v2Wins << " Draws: " << draws << "\n";
//         board.reset();
//         v1Player.resetTT();
//         v2Player.resetTT();
//     }
// }

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

int main() {
    constexpr int windowSize{ 8 * squareSize };
    sf::RenderWindow window(sf::VideoMode({ windowSize, windowSize }), "SFML");

    Checkers board{};
    EGTB egtb;
    egtb.buildOrLoad("egtb.bin");

    NNUE nnueV1{ {128, 256, 32, 1} }; nnueV1.load("nnue_best.bin");
    NNUEInference nnueInferenceV1{ nnueV1 };

    NNUE nnueV2{ {128, 256, 32, 1} }; nnueV2.load("nnue_best.bin");
    NNUEInference nnueInferenceV2{ nnueV2 };

    int v1Wins{ 0 };
    int v2Wins{ 0 };
    int draws{ 0 };

    auto v1Player{ v1::AIPlayer(board, egtb, nnueInferenceV1) };
    auto v2Player{ v2::AIPlayer(board, egtb, nnueInferenceV2) }; // piece eval


    for (int i{ 0 }; i < 1000; i++) {
        bool v1IsDark{ i % 2 == 0 };
        int numMoves{ 0 };
        while (window.isOpen()) {
            while (const std::optional event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>())
                    window.close();
            }

            window.clear();

            displayGrid(window);

            int score;
            std::vector<int> moves;
            bool isV1Turn = (board.isDarkTurn() == v1IsDark);

            if (isV1Turn)
                std::tie(score, moves) = v1Player.search(100, false);
            else
                std::tie(score, moves) = v2Player.search(100, false);

            if (moves.empty()) {
                if (isV1Turn) v2Wins++;
                else v1Wins++;
                break;
            }

            for (int m : moves) board.makeMove(m);

            if ((board.isDarkTurn() == v1IsDark) == isV1Turn) {
                std::cerr << "Error: Wrong player moved!\n";
                return 1;
            }

            if (board.isDraw()) {
                draws++;
                break;
            }

            numMoves++;
            std::cout << "Game " << i + 1 << ": Move " << numMoves << " Score: " << score << "\n";

            displayBoard(board, window);

            window.display();
        }

        std::cout << "Game " << i + 1 << ": V1 Wins: " << v1Wins << " V2 Wins: " << v2Wins << " Draws: " << draws << "\n";
        board.reset();
        v1Player.resetTT();
        v2Player.resetTT();

        if (!window.isOpen()) break;
    }
}
