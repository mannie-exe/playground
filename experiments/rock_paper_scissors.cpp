#include <cmath>
#include <format>
#include <iostream>
#include <print>
#include <string>

#include "random.h"
#include "readchar.h"

using namespace std;

const int MAX_ROUNDS = 5;
const int WIN_SCORE = MAX_ROUNDS / 2 + 1;

const string ROCK_INPUTS = "Rr";
const string PAPER_INPUTS = "Pp";
const string SCISSOR_INPUTS = "Ss";
const string EXIT_INPUTS = "Qq";

bool isInput(char match, const string &inputs) {
  for (char input : inputs) {
    if (match == input)
      return true;
  }
  return false;
}

struct GameMove {
  GameMove() { _value = 0; }

  // Default constructors
  static GameMove ROCK() { return GameMove{_ROCK}; }
  static GameMove PAPER() { return GameMove{_PAPER}; }
  static GameMove SCISSOR() { return GameMove{_SCISSOR}; }
  static GameMove RANDOM() { return GameMove{Random::Int(1, 3)}; }

  int getIndex() { return _value; }

  bool isSame(GameMove otherMove) {
    if (_value == otherMove._value)
      return true;
    return false;
  }

  bool beats(GameMove otherMove) {
    if (isSame(otherMove))
      return false;

    if (_value == _ROCK && otherMove._value == _PAPER)
      return false;
    if (_value == _PAPER && otherMove._value == _SCISSOR)
      return false;
    if (_value == _SCISSOR && otherMove._value == _ROCK)
      return false;

    return true;
  }

  bool isInitialized() {
    if (_value == 0)
      return false;
    return true;
  }

  string asString() const {
    switch (_value) {
    case 1:
      return "ROCK";
    case 2:
      return "PAPER";
    case 3:
      return "SCISSOR";
    default:
      return "ERROR";
    }
  }

private:
  static const int _INIT = 0;
  static const int _ROCK = 1;
  static const int _PAPER = 2;
  static const int _SCISSOR = 3;

  int _value = _INIT;

  GameMove(int value) { _value = value; }
};

ostream &operator<<(ostream &stream, const GameMove &move) {
  stream << move.asString();
  return stream;
}

template <> struct formatter<GameMove> : formatter<string> {
  auto format(const GameMove &move, format_context &ctx) const {
    return formatter<string>::format(move.asString(), ctx);
  }
};

int main() {
  println("Hello, world! Let's play... 🪨ROCK 📃PAPER ✂️SCISSOR!");
  println("Best of {} rounds wins! ({} to win)\n", MAX_ROUNDS, WIN_SCORE);

  char playerInput;
  bool didExit = false;
  GameMove playerMove;
  GameMove computerMove;
  int playerScore = 0;
  int currentRound = 1;
  int draws = 0;
  bool roundWon = false;
  bool roundDraw = false;

GAME_START:
  while (true) {
    cout << "Pick your move: [Rr]ock, [Pp]aper, [Ss]cissor (or [Qq]uit) ->"
         << flush;
    playerInput = readchar();
    cout << endl;

    // Handle exit input
    didExit = isInput(playerInput, EXIT_INPUTS) ? true : false;
    if (didExit)
      break;

    // Handle game input
    if (isInput(playerInput, ROCK_INPUTS))
      playerMove = GameMove::ROCK();
    if (isInput(playerInput, PAPER_INPUTS))
      playerMove = GameMove::PAPER();
    if (isInput(playerInput, SCISSOR_INPUTS))
      playerMove = GameMove::SCISSOR();

    // Handle non-move-input
    if (!playerMove.isInitialized())
      continue;

    // Pick computer move
    computerMove = GameMove::RANDOM();

    println("You: {} | VERSUS | {} :Computer ", playerMove, computerMove);

    // Resolve score
    if (playerMove.isSame(computerMove)) {
      draws++;
      roundDraw = true;
      println("You both picked: {}! Draw!", playerMove);
    } else {
      if (playerMove.beats(computerMove)) {
        playerScore++;
        roundWon = true;
        println("{} beats {}! +1", playerMove, computerMove);
      } else {
        println("{} beats {}! -1", computerMove, playerMove);
      }

      // Handle win
      if (playerScore == WIN_SCORE) {
        println("You won!");
        break;
      }

      // Handle loss
      if (currentRound - draws - playerScore == WIN_SCORE) {
        println("You lost!");
        break;
      }
    }

    // Print score
    println("Round {} {}! Score: {} | Remaining Rounds: {} | Draws: {}",
            currentRound,
            roundWon    ? "won"
            : roundDraw ? "is a draw"
                        : "lost",
            playerScore, MAX_ROUNDS - currentRound + draws, draws);

    currentRound++;

    // Reset round state
    playerMove = GameMove{};
    playerInput = 0x00;
    roundWon = false;
    roundDraw = false;
  }

  // Handle replay
  if (!didExit) {
    cout << "Play again? [Qq]uit or press any other key to play ->" << flush;
    playerInput = readchar();
    cout << endl;

    didExit = isInput(playerInput, EXIT_INPUTS) ? true : false;
    if (!didExit) {
      // Reset game state
      playerInput = 0x00;
      playerMove = GameMove{};
      computerMove = GameMove{};
      playerScore = 0;
      draws = 0;
      roundWon = false;
      roundDraw = false;

      goto GAME_START;
    }
  }
  return 0;
}
