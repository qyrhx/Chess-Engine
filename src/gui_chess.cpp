#include <iostream>
#include <array>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

#include <raylib.h>

#include "./board.hpp"
#include "./debug.hpp"
#include "./bitboard.hpp"
#include "./search.hpp"

namespace consts {
constexpr int pieceSize   = 60;
constexpr int winW        = pieceSize * 8 + 140;
constexpr int winH        = pieceSize * 8;
constexpr int searchDepth = 4;
}  // namespace consts

using TexturesArr = std::array<Texture, 12>;

Vector2 sq_to_v(const Chess::Square sq);
Chess::Square v_to_sq(const Vector2 &v);
TexturesArr get_textures();
void draw_board(const Chess::Board &b, TexturesArr &txtrs);

constexpr int boxW = 300;
constexpr int boxH = 200;
void draw_text(
    const std::string &text, int boxX = consts::winW / 2 - boxW / 2,
    int boxY = consts::winH / 2 - boxH / 2
);
void draw_promotion_prompt();

enum class GameUIState {
  HumanToMove,        // waiting for a click
  AwaitingPromotion,  // human picked a promoting move, waiting for piece choice
  EngineThinking,     // background search in flight
  GameOver
};

// Which color the engine plays. Flip to White or make configurable.
constexpr Chess::PieceColor engine_color = Chess::Black;

int main() {
  InitWindow(consts::winW, consts::winH, "Chess");
  SetTargetFPS(60);

  Texture2D board_txtr = LoadTexture("../assets/img/board.png");
  TexturesArr txtrs    = get_textures();

  Chess::Board b{Chess::standard_chess};

  GameUIState state        = GameUIState::HumanToMove;
  Chess::Square from       = Chess::NoSquare;
  Chess::Square pending_to = Chess::NoSquare;  // move awaiting promotion choice

  // --- search thread state ---
  std::thread search_thread;
  std::atomic_bool stop_search{false};
  std::atomic_bool search_done{false};
  Chess::MoveEval search_result{};
  std::mutex board_mutex;  // guards `b` while search thread reads it

  int last_eval = 0;

  auto start_engine_search = [&]() {
    search_done = false;
    stop_search = false;
    // search_thread must not be joinable already
    search_thread = std::thread([&]() {
      Chess::MoveEval res = search(b, consts::searchDepth, stop_search);
      search_result       = res;
      search_done         = true;
    });
    state         = GameUIState::EngineThinking;
  };

  while (not WindowShouldClose()) {
    const Vector2 mp = GetMousePosition();

    switch (state) {
      case GameUIState::HumanToMove: {
        if (b.color_to_play == engine_color) {
          // Shouldn't normally happen, but guards against desync
          start_engine_search();
          break;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          const Chess::Square clicked = v_to_sq(mp);
          if (clicked == Chess::NoSquare)
            break;  // clicked outside the board

          if (from == Chess::NoSquare) {
            // Only allow picking up your own piece
            if (b.get_piece_color(clicked) == b.color_to_play)
              from = clicked;
          } else if (clicked == from) {
            from = Chess::NoSquare;  // deselect
          } else {
            if (b.is_valid_move(from, clicked) && b.is_promotion(from, clicked)) {
              pending_to = clicked;
              state      = GameUIState::AwaitingPromotion;
            } else {
              if (b.make_move(from, clicked) == Chess::NoErr) {
                from = Chess::NoSquare;
                if (b.get_state() == Chess::Checkmate || b.get_state() == Chess::Draw)
                  state = GameUIState::GameOver;
                else if (b.color_to_play == engine_color)
                  start_engine_search();
              } else {
                from = Chess::NoSquare;  // invalid target, reset selection
              }
            }
          }
        }
        break;
      }

      case GameUIState::AwaitingPromotion: {
        Chess::PieceType pt = Chess::NoType;
        if (IsKeyPressed(KEY_Q))
          pt = Chess::Queen;
        else if (IsKeyPressed(KEY_R))
          pt = Chess::Rook;
        else if (IsKeyPressed(KEY_B))
          pt = Chess::Bishop;
        else if (IsKeyPressed(KEY_N))
          pt = Chess::Knight;

        if (pt != Chess::NoType) {
          b.make_move(from, pending_to, pt);
          from       = Chess::NoSquare;
          pending_to = Chess::NoSquare;
          if (b.get_state() == Chess::Checkmate || b.get_state() == Chess::Draw)
            state = GameUIState::GameOver;
          else if (b.color_to_play == engine_color)
            start_engine_search();
          else
            state = GameUIState::HumanToMove;
        }
        break;
      }

      case GameUIState::EngineThinking: {
        if (search_done) {
          search_thread.join();
          b.make_move(search_result.move.from, search_result.move.to);
          if (b.get_state() == Chess::Checkmate || b.get_state() == Chess::Draw)
            state = GameUIState::GameOver;
          else
            state = GameUIState::HumanToMove;
        }
        break;
      }

      case GameUIState::GameOver:
        // no input handling; could add a "press R to restart" here
        break;
    }

    BeginDrawing();
    ClearBackground(BLACK);

    DrawTexture(board_txtr, 0, 0, RAYWHITE);

    if (from != Chess::NoSquare) {
      const Vector2 v = sq_to_v(from);
      DrawRectangle(
          int(v.x), int(v.y), consts::pieceSize, consts::pieceSize, Color{255, 0, 0, 100}
      );
      for (auto sq : b.get_possible_moves(from)) {
        const Vector2 tmp = sq_to_v(sq);
        DrawRectangle(
            int(tmp.x), int(tmp.y), consts::pieceSize, consts::pieceSize, Color{255, 0, 0, 100}
        );
      }
    }
    draw_board(b, txtrs);

    if (state == GameUIState::AwaitingPromotion)
      draw_promotion_prompt();

    if (b.get_state() == Chess::Checkmate)
      draw_text("Checkmate!");
    if (b.get_state() == Chess::Draw)
      draw_text("Draw!");

    // Only recompute eval when it's safe to read `b` (not mid-search)
    if (state != GameUIState::EngineThinking)
      last_eval = evaluate(b);
    const std::string evalstr = std::format("Board Eval:\n\t{}", last_eval);
    DrawText(evalstr.c_str(), consts::winW - 130, 30, 18, GREEN);

    const std::string statusstr =
        state == GameUIState::EngineThinking
            ? "Engine thinking..."
            : std::format("{} to move", b.color_to_play == Chess::White ? "White" : "Black");
    DrawText(statusstr.c_str(), consts::winW - 130, 100, 18, GREEN);

    EndDrawing();
  }

  if (search_thread.joinable()) {
    stop_search = true;
    search_thread.join();
  }

  UnloadTexture(board_txtr);
  for (auto &txtr : txtrs)
    UnloadTexture(txtr);
  CloseWindow();
}

Vector2 sq_to_v(const Chess::Square sq) {
  return Vector2{
      float(sq % 8) * consts::pieceSize,
      float(7 - int(sq / 8)) * consts::pieceSize,
  };
}

Chess::Square v_to_sq(const Vector2 &v) {
  if (v.x < 0 || v.x >= consts::pieceSize * 8 || v.y < 0 || v.y >= consts::pieceSize * 8)
    return Chess::NoSquare;
  Chess::Square sq = Chess::Square(int(v.x) / consts::pieceSize);
  sq += 8 * (7 - int(v.y) / consts::pieceSize);
  return sq;
}

TexturesArr get_textures() {
  using namespace Chess;
  TexturesArr txtrs{};
  txtrs[WP] = LoadTexture("../assets/img/pieces/wp.png");
  txtrs[WN] = LoadTexture("../assets/img/pieces/wn.png");
  txtrs[WB] = LoadTexture("../assets/img/pieces/wb.png");
  txtrs[WR] = LoadTexture("../assets/img/pieces/wr.png");
  txtrs[WQ] = LoadTexture("../assets/img/pieces/wq.png");
  txtrs[WK] = LoadTexture("../assets/img/pieces/wk.png");

  txtrs[BP] = LoadTexture("../assets/img/pieces/bp.png");
  txtrs[BN] = LoadTexture("../assets/img/pieces/bn.png");
  txtrs[BB] = LoadTexture("../assets/img/pieces/bb.png");
  txtrs[BR] = LoadTexture("../assets/img/pieces/br.png");
  txtrs[BQ] = LoadTexture("../assets/img/pieces/bq.png");
  txtrs[BK] = LoadTexture("../assets/img/pieces/bk.png");

  return txtrs;
}

void draw_board(const Chess::Board &b, TexturesArr &txtrs) {
  Chess::Bitboard piecesBB = b.all_pieces();
  while (piecesBB) {
    const Chess::Square sq = Chess::pop_lsb(piecesBB);
    const Vector2 v        = sq_to_v(sq);
    const auto i           = b.get_pieceBB_index(sq);
    DrawTexture(txtrs[i], int(v.x), int(v.y), RAYWHITE);
  }
}

void draw_text(const std::string &text, int boxX, int boxY) {
  DrawRectangle(boxX, boxY, boxW, boxH, Color{0, 0, 0, 150});
  DrawText(text.c_str(), boxX + 60, boxY + 50, 28, RAYWHITE);
}

void draw_promotion_prompt() {
  constexpr int boxW = 250;
  constexpr int boxH = 140;
  constexpr int boxX = consts::winW / 2 - boxW / 2;
  constexpr int boxY = consts::winH / 2 - boxH / 2;

  DrawRectangle(boxX, boxY, boxW, boxH, Color{0, 0, 0, 200});
  DrawText("<Q> Queen", boxX + 30, boxY + 15, 22, RAYWHITE);
  DrawText("<R> Rook", boxX + 30, boxY + 45, 22, RAYWHITE);
  DrawText("<B> Bishop", boxX + 30, boxY + 75, 22, RAYWHITE);
  DrawText("<N> Knight", boxX + 30, boxY + 105, 22, RAYWHITE);
}
