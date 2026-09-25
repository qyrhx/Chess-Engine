#pragma once

#include "./board.hpp"
#include "./bitboard.hpp"

namespace Chess {

// Standard 24-point scale: each N/B = 1, R = 2, Q = 4 (per side, so max 24)
static constexpr int phase_weight[6] = {1, 1, 2, 4, 0, 0};  // N B R Q K P

static int game_phase(const Board &b) {
  int phase = 0;
  for (int i = WN; i <= BP; ++i) {
    if (i == WK || i == BK)
      continue;
    PieceType pt = PieceType(i <= WP ? i : i - 6);  // map to N..P
    phase += popcnt(b.piecesBB[i]) * phase_weight[pt];
  }
  return phase > 24 ? 24 : phase;  // clamp, in case of promotions etc.
}

// MG/EG piece values
static constexpr int mg_value[6] = {320, 330, 500, 900, 0, 100};
static constexpr int eg_value[6] = {320, 330, 520, 950, 0, 120};

// MG/EG PSTs (PeSTO-style, indexed [piece][square], White POV, A1=0)
// clang-format off
static constexpr int mg_psq[6][64] = {
  // N, B, R, Q, K, P — reuse your existing tables here for MG,
  // and add a second EG set below. (omitted for brevity — same shape
  // as your current `psq` array)
};
static constexpr int eg_psq[6][64] = {
  // King EG table matters most to add, e.g. centralized king:
  // {-50,-40,-30,-20,-20,-30,-40,-50, ... , 50,40,30,20,20,30,40,50}
};
// clang-format on

static void material_and_psq(const Board &b, int &mg, int &eg) {
  mg = 0;
  eg = 0;
  for (int i = WN; i <= BP; ++i) {
    bool is_black = i >= BN;
    PieceType pt  = PieceType(is_black ? i - 6 : i);
    Bitboard bb   = b.piecesBB[i];
    while (bb) {
      Square sq  = pop_lsb(bb);
      Square sq_ = is_black ? Square(sq ^ 56) : sq;
      int sign   = is_black ? -1 : 1;
      mg += sign * (mg_value[pt] + mg_psq[pt][sq_]);
      eg += sign * (eg_value[pt] + eg_psq[pt][sq_]);
    }
  }
}

// Bishop pair
static int bishop_pair(const Board &b) {
  int score = 0;
  if (popcnt(b.piecesBB[WB]) >= 2)
    score += 30;
  if (popcnt(b.piecesBB[BB]) >= 2)
    score -= 30;
  return score;
}

// Pawn structure
static int pawn_structure(const Board &b) {
  int score   = 0;
  Bitboard wp = b.piecesBB[WP];
  Bitboard bp = b.piecesBB[BP];

  for (File f = fileA; f <= fileH; f = File(f + 1)) {
    int wcount = popcnt(wp & file_bb(f));
    int bcount = popcnt(bp & file_bb(f));
    if (wcount > 1)
      score -= 12 * (wcount - 1);  // doubled
    if (bcount > 1)
      score += 12 * (bcount - 1);

    Bitboard adj = (f > fileA ? file_bb(File(f - 1)) : 0) | (f < fileH ? file_bb(File(f + 1)) : 0);
    if (wcount && ! (wp & adj))
      score -= 15 * wcount;  // isolated
    if (bcount && ! (bp & adj))
      score += 15 * bcount;
  }

  // Passed pawns
  Bitboard wbb = wp;
  while (wbb) {
    Square sq           = pop_lsb(wbb);
    File f              = get_file(sq);
    Rank r              = get_rank(sq);
    Bitboard front_span = 0;
    for (File ff = File(f > fileA ? f - 1 : f); ff <= (f < fileH ? File(f + 1) : f);
         ff      = File(ff + 1))
      for (Rank rr = Rank(r + 1); rr <= rank8; rr = Rank(rr + 1))
        front_span |= sqbb(Square(ff + rr * 8));
    if (! (front_span & bp)) {
      static constexpr int bonus[8] = {0, 5, 10, 20, 35, 60, 100, 0};
      score += bonus[r];
    }
  }
  Bitboard bbb = bp;
  while (bbb) {
    Square sq           = pop_lsb(bbb);
    File f              = get_file(sq);
    Rank r              = get_rank(sq);
    Bitboard front_span = 0;
    for (File ff = File(f > fileA ? f - 1 : f); ff <= (f < fileH ? File(f + 1) : f);
         ff      = File(ff + 1))
      for (Rank rr = Rank(0); rr < r; rr = Rank(rr + 1))
        front_span |= sqbb(Square(ff + rr * 8));
    if (! (front_span & wp)) {
      static constexpr int bonus[8] = {0, 100, 60, 35, 20, 10, 5, 0};
      score -= bonus[r];
    }
  }
  return score;
}

// Rook on open/semi-open file
static int rook_files(const Board &b) {
  int score   = 0;
  Bitboard wp = b.piecesBB[WP], bp = b.piecesBB[BP];
  Bitboard wr = b.piecesBB[WR], br = b.piecesBB[BR];
  while (wr) {
    File f        = get_file(pop_lsb(wr));
    bool own_pawn = file_bb(f) & wp, enemy_pawn = file_bb(f) & bp;
    if (! own_pawn && ! enemy_pawn)
      score += 25;  // open
    else if (! own_pawn)
      score += 12;  // semi-open
  }
  while (br) {
    File f        = get_file(pop_lsb(br));
    bool own_pawn = file_bb(f) & bp, enemy_pawn = file_bb(f) & wp;
    if (! own_pawn && ! enemy_pawn)
      score -= 25;
    else if (! own_pawn)
      score -= 12;
  }
  return score;
}

inline int evaluate(const Board &b) {
  int mg, eg;
  material_and_psq(b, mg, eg);

  int static_extra = bishop_pair(b) + pawn_structure(b) + rook_files(b);
  mg += static_extra;
  eg += static_extra;

  int phase = game_phase(b);
  int score = (mg * phase + eg * (24 - phase)) / 24;

  score += (b.color_to_play == White ? 10 : -10);  // tempo

  return b.color_to_play == White ? score : -score;  // if you want side-to-move relative
}

}  // namespace Chess
