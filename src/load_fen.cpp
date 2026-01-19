#include <iostream>
#include <format>
#include <string>
#include <regex>
#include <cassert>

#include "./utils.hpp"
#include "./board.hpp"
#include "./zobrist.hpp"

namespace Chess {

using std::string;
using std::vector;

static string trim(const string &str);
static vector<string> split(const string &str, const string &delim);

void Board::load_fen(const string &FEN) {
  if (all_pieces())
    for (auto &bb : piecesBB)
      bb = 0;
  move_history.clear();
  zobrist.clear();
  state = None;

  static const std::regex fen_notation_regex{
      "^([kqrbnpKQRBNP1-8]{1,8}\\/){7}[kqrbnpKQRBNP1-8]{1,8} "
      "(w|b) (-|[KQkq]{1,4}) (-|(([A-H]|[a-h])[1-8]))( (0|(100)|([1-9][0-9]?)))?( "
      "[0-9]{1,3})?$"
  };

  const string fen = trim(FEN);
  assert(std::regex_search(fen, fen_notation_regex));

  vector<string> tokens{split(fen, " ")};
  for (auto &t : tokens)
    t = trim(t);

  // Placing pieces on the board
  int x = 0;
  int y = 7;
  for (auto c : tokens[0]) {
    if (c == '/')
      x = 0, --y;
    else if (isdigit(c))
      x += c - '0';
    else {
      const char lc = tolower(c);
      int i;
      switch (lc) {
        case 'k': i = WK; break;
        case 'q': i = WQ; break;
        case 'r': i = WR; break;
        case 'b': i = WB; break;
        case 'n': i = WN; break;
        default: i = WP; break;
      }
      if (islower(c))
        i += 6;
      piecesBB[i] |= 1ull << (x + y * 8);
      ++x;
    }
  }

  color_to_play = tokens[1] == "w" ? White : Black;

  // Castling rights
  cr = NoCastling;
  if (tokens[2] != "-") {
    for (auto c : tokens[2]) {
      switch (c) {
        case 'K': cr |= White_OO; break;
        case 'Q': cr |= White_OOO; break;
        case 'k': cr |= Black_OO; break;
        default: cr |= Black_OOO; break;  // 'q'
      }
    }
  }

  if (tokens[3] == "-") {
    enpassant_square = NoSquare;
  }
  // Regex doesn't allow invalid squares, so no checks are needed
  else {
    enpassant_square = Square((std::tolower(tokens[3][0]) - 'a') * 8 + tokens[3][1] - '1');
  }
  // Again, no need for checks
  if (tokens.size() > 4) {
    fifty_move_counter = std::stoi(tokens[4]);
  }
  gen_board_legal_moves();
  zobrist.emplace_back(calc_zobrist_key());
  std::cout << std::format("\n\n[Engine] Loaded FEN position: {}\n", fen);
}

string trim(const string &str) {
  if (str.empty())
    return str;
  return str
      .substr(0, str.find_last_not_of(" \t\n") + 1)  // right trim
      .substr(str.find_first_not_of(" \t\n"));       // left trim
}

vector<string> split(const string &str, const string &delim) {
  vector<string> res{};
  res.reserve(8);

  size_t begin = 0, end = 0;
  while (true) {
    end = str.find(delim, begin);
    if (end == string::npos) {
      res.emplace_back(str.substr(begin));
      break;
    }
    res.emplace_back(str.substr(begin, end - begin));
    begin = end + delim.length();
  }
  return res;
}

}  // namespace Chess
