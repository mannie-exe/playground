#include <format>
#include <iostream>
#include <print>
#include <string_view>

#include <random.hpp>

#include "readchar.hpp"

using namespace std;

constexpr int MAX_ROUNDS = 5;
constexpr int WIN_SCORE = MAX_ROUNDS / 2 + 1;

constexpr string_view ROCK_INPUTS = "Rr";
constexpr string_view PAPER_INPUTS = "Pp";
constexpr string_view SCISSOR_INPUTS = "Ss";
constexpr string_view EXIT_INPUTS = "Qq";

Random random;

bool isInput(char match, string_view inputs) {
  for (char input : inputs) {
    if (match == input)
      return true;
  }
  return false;
}

class GameMove {
public:
  enum class Value { NONE = 0, ROCK = 1, PAPER = 2, SCISSOR = 3 };

  constexpr GameMove() = default;

  static constexpr GameMove Rock() { return GameMove{Value::ROCK}; }
  static constexpr GameMove Paper() { return GameMove{Value::PAPER}; }
  static constexpr GameMove Scissor() { return GameMove{Value::SCISSOR}; }
  static GameMove Random() {
    return GameMove{static_cast<Value>(random.get(1, 3))};
  }

  constexpr bool isInitialized() const { return _value != Value::NONE; }

  constexpr bool isSame(GameMove other) const { return _value == other._value; }

  constexpr bool beats(GameMove other) const {
    return (_value == Value::ROCK && other._value == Value::SCISSOR) ||
           (_value == Value::PAPER && other._value == Value::ROCK) ||
           (_value == Value::SCISSOR && other._value == Value::PAPER);
  }

  constexpr string_view toString() const {
    switch (_value) {
    case Value::ROCK:
      return "ROCK";
    case Value::PAPER:
      return "PAPER";
    case Value::SCISSOR:
      return "SCISSOR";
    default:
      return "ERROR";
    }
  }

private:
  constexpr explicit GameMove(Value value) : _value(value) {}

  Value _value = Value::NONE;
};

ostream &operator<<(ostream &stream, const GameMove &move) {
  stream << move.toString();
  return stream;
}

template <> struct formatter<GameMove> : formatter<string_view> {
  auto format(const GameMove &move, format_context &ctx) const {
    return std::format_to(ctx.out(), "{}", move);
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
      playerMove = GameMove::Rock();
    if (isInput(playerInput, PAPER_INPUTS))
      playerMove = GameMove::Paper();
    if (isInput(playerInput, SCISSOR_INPUTS))
      playerMove = GameMove::Scissor();

    // Handle non-move-input
    if (!playerMove.isInitialized())
      continue;

    // Pick computer move
    computerMove = GameMove::Random();

    println("You | {} vs {} | Computer ", playerMove, computerMove);

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
    println("Round {} {}! Score: {} | Remaining Rounds: {} | Draws: {}\n",
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
      currentRound = 1;
      draws = 0;
      roundWon = false;
      roundDraw = false;

      goto GAME_START;
    }
  }
  return 0;
}
