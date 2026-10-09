/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2022  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file exact_mc_synthesis_multi.hpp
  \brief SAT-based multi-output XAG synthesis based on MC

  \author Mingfei Yu
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include <bill/sat/interface/common.hpp>
#include <bill/sat/interface/glucose.hpp>
#include <kitty/bit_operations.hpp>
#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <kitty/properties.hpp>

#include "../networks/xag.hpp"
#include "../views/cnf_view.hpp"
#include "cnf.hpp"

namespace mockturtle
{

struct exact_mc_synthesis_multi_params
{
  /*! \brief Lower bound on the number of AND gates. */
  uint32_t min_and_gates{ 0u };

  /*! \brief Upper bound on the number of AND gates. */
  uint32_t max_and_gates{ 64u };

  /*! \brief Conflict limit for the SAT solver (0 = unlimited). */
  uint32_t conflict_limit{ 0u };

  /*! \brief Use subset symmetry breaking. */
  bool break_subset_symmetries{ true };

  /*! \brief Use multi-level subset symmetry breaking. */
  bool break_multi_level_subset_symmetries{ true };

  /*! \brief Use symmetric variables. */
  bool break_symmetric_variables{ true };

  /*! \brief Ensure to use all gates and essential variables. */
  bool ensure_to_use_gates{ true };

  /*! \brief Be verbose. */
  bool verbose{ false };
};

/*! \brief Outcome of an exact multi-output MC synthesis call. */
enum class exact_mc_synthesis_multi_status
{
  /*! \brief An MC-optimum implementation was found. */
  optimum,
  /*! \brief No implementation exists with at most `max_and_gates` AND gates. */
  infeasible,
  /*! \brief The conflict limit was reached before a conclusion. */
  unknown
};

struct exact_mc_synthesis_multi_result
{
  /*! \brief Number of AND gates; valid if `status` is `optimum`. */
  uint32_t num_ands{ 0u };

  /*! \brief Outcome of the synthesis call. */
  exact_mc_synthesis_multi_status status{ exact_mc_synthesis_multi_status::unknown };

  /*! \brief Largest AND gate count proven insufficient, or -1 if none. */
  int32_t last_unsat{ -1 };
};

namespace detail
{

template<bill::solvers Solver>
struct exact_mc_synthesis_multi_impl
{
  using problem_network_t = cnf_view<xag_network, false, Solver>;
  using sig_t = signal<problem_network_t>;

  exact_mc_synthesis_multi_impl( std::vector<kitty::dynamic_truth_table> const& funcs, exact_mc_synthesis_multi_params const& ps )
      : num_vars_( funcs.front().num_vars() ),
        ps_( ps )
  {
    // normalize to f(0) = 0 (output inversion is free) and drop constant outputs
    for ( auto const& f : funcs )
    {
      const auto g = kitty::get_bit( f, 0 ) ? ~f : f;
      if ( !kitty::is_const0( g ) )
      {
        funcs_.push_back( g );
      }
    }
  }

  exact_mc_synthesis_multi_result run()
  {
    exact_mc_synthesis_multi_result res;
    if ( funcs_.empty() )
    {
      res.num_ands = 0u;
      res.status = exact_mc_synthesis_multi_status::optimum;
      return res;
    }

    uint32_t lower = ps_.min_and_gates;
    for ( auto const& f : funcs_ )
    {
      const auto degree = kitty::polynomial_degree( f );
      lower = std::max( lower, degree == 0u ? 0u : degree - 1u );
    }

    for ( auto num_ands = lower; num_ands <= ps_.max_and_gates; ++num_ands )
    {
      if ( ps_.verbose )
      {
        fmt::print( "[i] try with {} AND gates\n", num_ands );
      }
      problem_network_t pntk;
      reset( pntk );
      for ( auto i = 0u; i < num_ands; ++i )
      {
        add_gate( pntk );
      }
      for ( auto i = 0u; i < funcs_.size(); ++i )
      {
        add_output( pntk );
      }
      prune_search_space( pntk );
      for ( auto b = 1u; b < funcs_.front().num_bits(); ++b )
      {
        constrain_assignment( pntk, b );
      }

      bill::result::clause_type assumptions;
      pntk.foreach_po( [&]( auto const& f ) {
        assumptions.push_back( pntk.lit( f ) );
      } );
      const auto result = pntk.solve( assumptions, ps_.conflict_limit );
      if ( !result )
      {
        res.status = exact_mc_synthesis_multi_status::unknown;
        return res;
      }
      if ( *result )
      {
        res.num_ands = num_ands;
        res.status = exact_mc_synthesis_multi_status::optimum;
        return res;
      }
      res.last_unsat = static_cast<int32_t>( num_ands );
    }
    res.status = exact_mc_synthesis_multi_status::infeasible;
    return res;
  }

private:
  void reset( problem_network_t const& pntk )
  {
    gate_vars_.clear();
    out_vars_.clear();
    truth_vars_.clear();
    truth_vars_.resize( funcs_.front().num_bits() );
    for ( auto i = 0u; i < num_vars_; ++i )
    {
      const auto var_tt = kitty::nth_var<kitty::dynamic_truth_table>( num_vars_, i );
      for ( auto b = 0u; b < funcs_.front().num_bits(); ++b )
      {
        truth_vars_[b].push_back( pntk.get_constant( kitty::get_bit( var_tt, b ) ) );
      }
    }
  }

  void add_gate( problem_network_t& pntk )
  {
    const uint32_t gate_index = static_cast<uint32_t>( gate_vars_.size() ) / 2;
    for ( auto j = 0u; j < 2u; ++j )
    {
      gate_vars_.push_back( std::vector<sig_t>( num_vars_ + gate_index ) );
      std::generate( gate_vars_.back().begin(), gate_vars_.back().end(), [&]() { return pntk.create_pi(); } );
    }
  }

  void add_output( problem_network_t& pntk )
  {
    out_vars_.push_back( std::vector<sig_t>( num_vars_ + gate_vars_.size() / 2 ) );
    std::generate( out_vars_.back().begin(), out_vars_.back().end(), [&]() { return pntk.create_pi(); } );
  }

  // all LTFI vectors: gate fanins first, then outputs
  std::vector<std::vector<sig_t> const*> all_ltfis() const
  {
    std::vector<std::vector<sig_t> const*> v;
    for ( auto const& l : gate_vars_ )
    {
      v.push_back( &l );
    }
    for ( auto const& l : out_vars_ )
    {
      v.push_back( &l );
    }
    return v;
  }

  void constrain_assignment( problem_network_t& pntk, uint32_t bit )
  {
    const auto create_xor_clause = [&]( std::vector<sig_t> const& ltfi_vars ) -> sig_t {
      std::vector<sig_t> ltfi( ltfi_vars.size() );
      for ( auto j = 0u; j < ltfi.size(); ++j )
      {
        ltfi[j] = pntk.create_and( ltfi_vars[j], truth_vars_[bit][j] );
      }
      return pntk.create_nary_xor( ltfi );
    };

    for ( auto i = 0u; i < gate_vars_.size() / 2; ++i )
    {
      truth_vars_[bit].push_back( pntk.create_and( create_xor_clause( gate_vars_[2 * i] ), create_xor_clause( gate_vars_[2 * i + 1] ) ) );
    }

    for ( auto o = 0u; o < out_vars_.size(); ++o )
    {
      const auto po_signal = create_xor_clause( out_vars_[o] );
      pntk.create_po( kitty::get_bit( funcs_[o], bit ) ? po_signal : pntk.create_not( po_signal ) );
    }
  }

  void prune_search_space( problem_network_t& pntk )
  {
    const auto ltfis = all_ltfis();

    // At least one element in LTFI
    for ( auto const* ltfi : ltfis )
    {
      pntk.add_clause( *ltfi );
    }

    // linear TFIs are no subset of each other
    if ( ps_.break_subset_symmetries )
    {
      for ( auto i = 0u; i < gate_vars_.size() / 2u; ++i )
      {
        auto const& ltfi1 = gate_vars_[2 * i];
        auto const& ltfi2 = gate_vars_[2 * i + 1];
        std::vector<sig_t> ands( ltfi1.size() );
        std::vector<sig_t> ands2( ltfi1.size() );
        for ( auto j = 0u; j < ltfi1.size(); ++j )
        {
          ands[j] = pntk.create_and( ltfi1[j], pntk.create_not( ltfi2[j] ) );
          ands2[j] = pntk.create_and( ltfi2[j], pntk.create_not( ltfi1[j] ) );
        }
        pntk.add_clause( ands );
        pntk.add_clause( ands2 );
      }
    }

    // left linear TFI is lexicographically smaller than right one
    for ( auto i = 0u; i < gate_vars_.size() / 2u; ++i )
    {
      auto const& ltfi2 = gate_vars_[2 * i];
      auto const& ltfi1 = gate_vars_[2 * i + 1];
      const auto n = ltfi1.size();
      std::vector<sig_t> as( n - 1u );
      std::generate( as.begin(), as.end(), [&]() { return pntk.create_pi(); } );

      pntk.add_clause( !ltfi1[0], ltfi2[0] );
      pntk.add_clause( !ltfi1[0], as[0] );
      pntk.add_clause( ltfi2[0], as[0] );
      for ( auto k = 1u; k < n - 1; ++k )
      {
        pntk.add_clause( !ltfi1[k], ltfi2[k], !as[k - 1] );
        pntk.add_clause( !ltfi1[k], as[k], !as[k - 1] );
        pntk.add_clause( ltfi2[k], as[k], !as[k - 1] );
      }
      pntk.add_clause( !ltfi1.back(), !as.back() );
      pntk.add_clause( ltfi2.back(), !as.back() );
    }

    // break on multi-level subset relation, over gate fanins and outputs
    if ( ps_.break_multi_level_subset_symmetries )
    {
      for ( auto ii = 0u; ii < ltfis.size(); ++ii )
      {
        auto const& ltfi = *ltfis[ii];
        const auto num_prev_gates = ltfi.size() - num_vars_;
        for ( auto i = 0u; i < num_prev_gates; ++i )
        {
          const auto n = gate_vars_[2 * i].size();
          std::vector<sig_t> ands_left, ands_right;
          ands_left.push_back( ltfi[num_vars_ + i] );
          for ( auto k = 0u; k < n; ++k )
          {
            ands_left.push_back( pntk.create_or( !ltfi[k], gate_vars_[2 * i][k] ) );
            ands_left.push_back( pntk.create_or( !ltfi[k], gate_vars_[2 * i + 1][k] ) );
            ands_right.push_back( pntk.create_xnor( ltfi[k], pntk.create_and( gate_vars_[2 * i][k], gate_vars_[2 * i + 1][k] ) ) );
          }
          pntk.create_po( pntk.create_or( !pntk.create_nary_and( ands_left ), pntk.create_nary_and( ands_right ) ) );
        }
      }
    }

    // break on variables that are symmetric in all outputs
    if ( ps_.break_symmetric_variables )
    {
      for ( auto jj = 1u; jj < num_vars_; ++jj )
      {
        for ( auto j = 0u; j < jj; ++j )
        {
          const bool sym = std::all_of( funcs_.begin(), funcs_.end(), [&]( auto const& f ) { return kitty::is_symmetric_in( f, j, jj ); } );
          if ( !sym )
          {
            continue;
          }
          for ( auto ii = 0u; ii < ltfis.size(); ++ii )
          {
            std::vector<sig_t> clause;
            clause.push_back( !( *ltfis[ii] )[jj] );
            for ( auto i = 0u; i <= ii; ++i )
            {
              clause.push_back( ( *ltfis[i] )[j] );
            }
            pntk.add_clause( clause );
          }
        }
      }
    }

    // ensure to use essential variables and gates
    if ( ps_.ensure_to_use_gates )
    {
      const auto num_ands = gate_vars_.size() / 2;
      for ( auto j = 0u; j < num_vars_ + num_ands; ++j )
      {
        if ( j < num_vars_ && std::none_of( funcs_.begin(), funcs_.end(), [&]( auto const& f ) { return kitty::has_var( f, j ); } ) )
        {
          continue;
        }
        std::vector<sig_t> clause;
        for ( auto const* ltfi : ltfis )
        {
          if ( j < ltfi->size() )
          {
            clause.push_back( ( *ltfi )[j] );
          }
        }
        pntk.add_clause( clause );
      }
    }
  }

private:
  uint32_t num_vars_;
  std::vector<kitty::dynamic_truth_table> funcs_;
  std::vector<std::vector<sig_t>> gate_vars_;
  std::vector<std::vector<sig_t>> out_vars_;
  std::vector<std::vector<sig_t>> truth_vars_;
  exact_mc_synthesis_multi_params const& ps_;
};

} /* namespace detail */

/*! \brief Exact multi-output MC synthesis of an XAG.
 *
 * Synthesizes an XAG that realizes all functions in `funcs` with the minimum
 * total number of AND gates, which are shared among the outputs.  The number
 * of AND gates is increased from a lower bound until the SAT instance becomes
 * satisfiable.  Functions must be defined over the same variables.
 *
 * \param funcs Multi-output function to synthesize
 * \param ps Parameters
 */
template<bill::solvers Solver = bill::solvers::glucose_41>
exact_mc_synthesis_multi_result exact_mc_synthesis_multi( std::vector<kitty::dynamic_truth_table> const& funcs, exact_mc_synthesis_multi_params const& ps = {} )
{
  return detail::exact_mc_synthesis_multi_impl<Solver>{ funcs, ps }.run();
}

} /* namespace mockturtle */
