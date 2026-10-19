#pragma once

#include "mc_analyzer.hpp"

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operations.hpp>
#include <kitty/operators.hpp>
#include <kitty/print.hpp>
#include <kitty/static_truth_table.hpp>

#include <limits>
#include <numeric>

namespace mockturtle
{
class mc_analyzer;

struct acdecomp_max_local_params
{
	uint32_t lut_size = 6u; /* `lut_size` has to be no larger than six, as required by the synthesis engine */
	uint32_t max_free_set_size = 4u;
};

class acdecomp_max_local
{
private:
	static constexpr uint32_t max_num_vars = 10u;
	using TT = kitty::static_truth_table<max_num_vars>;
	struct encoding_column
	{
		uint64_t column[2];
		uint32_t index;
		uint32_t cost;
		float sort_cost;
	};

public:
	explicit acdecomp_max_local( kitty::dynamic_truth_table const& tt, acdecomp_max_local_params ps )
		: _tt_orig( tt ),
		  _num_vars( tt.num_vars() ),
		  _ps( ps )
	{}

	bool investigate_decomp( std::vector<std::vector<uint32_t>> ownerships )
	{
		if ( _num_vars > _ps.max_free_set_size + _ps.lut_size )
		{
			_ps.max_free_set_size = _num_vars - _ps.lut_size;
		}

		for ( uint32_t i = 1u; i < ownerships.size(); ++i )
		{
			const uint32_t ownerships_i_size = ownerships[i].size();

			if ( ownerships_i_size > _ps.max_free_set_size )
			{
				continue;
			}

      std::copy( _tt_orig.begin(), _tt_orig.end(), _tt_res.begin() );
      std::iota( _perm.begin(), _perm.end(), 0u );
			push_into_free_set( ownerships[i] );

			if ( check_multiplicity( ownerships[i] ) )
			{
				return true;
			}
		}

		return false;
	}

	bool compute_decomp()
	{
		std::vector<TT> isets = compute_isets();
		compute_encodings();

		if ( _multiplicity <= 4u )
		{
			return solve_encoding_exact( isets );
		}

		return solve_encoding_heuristic( isets );
	}

	void get_decomp()
	{
    _decomp_bs.clear();

    for ( uint32_t i = 0; i < _bound_set.size(); ++i )
    {
      decomp_result dec;
      auto tt = _bound_set[i];
      auto care = _care_set[i];

      /* compute and minimize support for bound set variables */
      uint32_t k = 0;
      for ( uint32_t j = 0; j < _num_vars - _free_set_size; ++j )
      {
        if ( !kitty::has_var( tt, care, j ) )
        {
          /* fix truth table */
          adjust_tt_on_dc( tt, care, j );
          continue;
        }

        if ( k < j )
        {
          kitty::swap_inplace( tt, k, j );
          kitty::swap_inplace( care, k, j );
        }
        dec.support.push_back( _perm[_free_set_size + j] );
        ++k;
      }

      dec.tt = kitty::shrink_to( tt, dec.support.size() );
      _decomp_bs.push_back( dec );
    }

    /* compute the decomposition for the top-level LUT */
    compute_top_mux();
	}

  int32_t multi_run( mc_analyzer& mc_engine, std::vector<std::vector<uint32_t>> ownerships )
  {
    std::vector<decomp_result> best_decomp_bs{};
    std::vector<decomp_result> best_decomp_fs{};
    std::vector<masked_encoding> best_encodings{};
    uint8_t best_sel_polar{};
    bool best_min_mc_mux{};
    bool is_first_solution{ true };
    uint32_t best_cost{ 0u };

    if ( _num_vars > _ps.max_free_set_size + _ps.lut_size )
    {
      _ps.max_free_set_size = _num_vars - _ps.lut_size;
    }

    /* reorder `ownerships` into `pi_ownerships` */
    std::vector<uint32_t> pis_ownership_per_pi( _num_vars );
    for ( auto i{ 1u }; i < ownerships.size(); ++i )
    {
      for ( auto element : ownerships[i] )
      {
        pis_ownership_per_pi[element] = ( 1 << ( i - 1 ) );
      }
    }
    for ( auto element : ownerships[0] )
    {
      /* TODO : any better solution? */
      pis_ownership_per_pi[element] = 3u;
    }

    for ( uint32_t i = 1u; i < ownerships.size(); ++i )
    {
      const uint32_t ownerships_i_size = ownerships[i].size();
      if ( ownerships_i_size > _ps.max_free_set_size || ownerships_i_size == 1u )
			{
				continue;
			}

      std::copy( _tt_orig.begin(), _tt_orig.end(), _tt_res.begin() );
      std::iota( _perm.begin(), _perm.end(), 0u );
			push_into_free_set( ownerships[i] );

      if ( check_multiplicity( ownerships[i] ) )
      {
        std::vector<TT> isets = compute_isets();
        compute_encodings();

        bool success_decomp = ( _multiplicity <= 4u ) ? solve_encoding_exact( isets ) : solve_encoding_heuristic( isets );
        if ( !success_decomp )
        {
          continue;
        }

        get_decomp();
        if ( is_first_solution )
        {
          assert( _decomp_fs.size() == _multiplicity );
          int32_t current_cost = mc_engine.estimate_decomp_res_cost( _decomp_bs, _num_vars, _decomp_fs.size(), ownerships, i, _multiplicity, _min_mc_mux );

          if ( current_cost >= 0 )
          {
            is_first_solution = false;
            best_cost = static_cast<uint32_t>( current_cost );
            best_decomp_bs = _decomp_bs;
            best_decomp_fs = _decomp_fs;
            best_encodings = _encodings;
            best_sel_polar = _sel_polar;
            best_min_mc_mux = _min_mc_mux;
          }
        }
        else
        {
          assert( _decomp_fs.size() == _multiplicity );
          int32_t current_cost = mc_engine.estimate_decomp_res_cost( _decomp_bs, _num_vars, _decomp_fs.size(), ownerships, i, _multiplicity, _min_mc_mux );

          if ( ( current_cost >= 0 ) && ( static_cast<uint32_t>( current_cost ) < best_cost ) )
          {
            best_cost = static_cast<uint32_t>( current_cost );
            best_decomp_bs = _decomp_bs;
            best_decomp_fs = _decomp_fs;
            best_encodings = _encodings;
            best_sel_polar = _sel_polar;
            best_min_mc_mux = _min_mc_mux;
          }
        }
      }
    }

    _decomp_bs = best_decomp_bs;
    _decomp_fs = best_decomp_fs;
    _encodings = best_encodings;
    _sel_polar = best_sel_polar;
    _min_mc_mux = best_min_mc_mux;

    return is_first_solution ? -1 : static_cast<int32_t>( best_cost );
  }

private:
	inline void push_into_free_set( std::vector<uint32_t> const& indices )
	{
    uint32_t k = 0u;
    for ( uint32_t i = 0u; i < indices.size(); ++i )
    {
      while ( k != indices[i] )
      {
        ++k;
      }

      if ( _perm[i] == k )
      {
        ++k;
        continue;
      }

      std::swap( _perm[i], _perm[k] );
      kitty::swap_inplace( _tt_res, i, k );
      ++k;
    }
	}

	bool check_multiplicity( std::vector<uint32_t> const& indices )
	{
		std::function<uint32_t( TT const& tt )> column_multiplicity_fn[5] = {
			[this]( TT const& tt ) { return column_multiplicity_le3<1u>( tt ); },
			[this]( TT const& tt ) { return column_multiplicity_le3<2u>( tt ); },
			[this]( TT const& tt ) { return column_multiplicity_le3<3u>( tt ); },
			[this]( TT const& tt ) { return column_multiplicity_le5<4u>( tt ); },
			[this]( TT const& tt ) { return column_multiplicity_le5<5u>( tt ); }
		};

		_free_set_size = indices.size();
		_multiplicity = column_multiplicity_fn[_free_set_size - 1]( _tt_res );

		/* cases where more than two-level decomposition is required is not considered for now  */
		/* when `_multiplicity` is larger than 16, `g` function cannot be realized via one LUT6 */
		if ( _multiplicity <= ( 1 << ( _num_vars - _free_set_size ) ) &&
         _multiplicity <= ( 1 << ( _ps.lut_size - _free_set_size ) ) && _multiplicity <= 16u )
		{
			return true;
		}

		return false;
	}

	template<uint32_t free_set_size>
	uint32_t column_multiplicity_le3( TT const& tt )
	{
    uint64_t multiplicity_set[4] = { 0u, 0u, 0u, 0u };
    uint32_t multiplicity = 0;
    uint32_t const num_blocks = ( _num_vars > 6 ) ? ( 1u << ( _num_vars - 6 ) ) : 1;
    uint64_t constexpr masks_bits[] = { 0x0, 0x3, 0xF, 0x3F };
    uint64_t constexpr masks_idx[] = { 0x0, 0x0, 0x0, 0x3 };

    /* extract iset functions */
    const uint32_t len = ( num_blocks == 1u ) ? ( 1 << ( _num_vars - _free_set_size ) ) : ( 64u >> _free_set_size );
    for ( auto i = 0u; i < num_blocks; ++i )
    {
      uint64_t cof = tt._bits[i];
      for ( auto j = 0; j < len; ++j )
      {
        uint64_t& repr = multiplicity_set[( cof >> 6 ) & masks_idx[_free_set_size]];
        repr |= ( uint64_t( 1 ) << ( cof & masks_bits[_free_set_size] ) );
        cof >>= ( 1u << _free_set_size );
      }
    }

    multiplicity = __builtin_popcountl( multiplicity_set[0] );

    if ( _free_set_size == 3 )
    {
      multiplicity += __builtin_popcountl( multiplicity_set[1] );
      multiplicity += __builtin_popcountl( multiplicity_set[2] );
      multiplicity += __builtin_popcountl( multiplicity_set[3] );
    }

    return multiplicity;
	}

	template<uint32_t free_set_size>
	uint32_t column_multiplicity_le5( TT const& tt )
	{
		std::array<uint64_t, 64u> multiplicity_set;
		const uint32_t num_blocks = ( _num_vars > 6u ) ? ( 1u << ( _num_vars - 6u ) ) : 1u;
		constexpr uint64_t masks[] = { 0x0, 0x3, 0xF, 0xFF, 0xFFFF, 0xFFFFFFFF };
		uint32_t multiplicity_set_size = 0u;

		uint64_t fs_func_prev = UINT64_MAX;
    const uint32_t len = ( num_blocks == 1u ) ? ( 1 << ( _num_vars - _free_set_size ) ) : ( 64u >> _free_set_size );
		for ( uint32_t i = 0u; i < num_blocks; ++i )
		{
			uint64_t coef = tt._bits[i];
			for ( uint32_t j = 0u; j < len; ++j )
			{
				uint64_t fs_func = coef & masks[free_set_size];
				if ( fs_func != fs_func_prev )
				{
					multiplicity_set[multiplicity_set_size++] = fs_func;
					fs_func_prev = fs_func;
				}
				coef >>= ( 1u << free_set_size );
			}
		}

		std::sort( multiplicity_set.begin(), multiplicity_set.begin() + multiplicity_set_size );

		uint32_t multiplicity = 1u;
		for ( uint8_t i = 1u; i < multiplicity_set_size; ++i )
		{
			if ( multiplicity_set[i] != multiplicity_set[i - 1] )
			{
				++multiplicity;
			}
		}

		return multiplicity;
	}

	std::vector<TT> compute_isets()
	{
    /* construct isets involved in multiplicity */
    uint32_t isets_support = _num_vars - _free_set_size;
    std::vector<TT> isets( _multiplicity );

    /* construct isets */
    std::unordered_map<uint64_t, uint32_t> column_to_iset;
    TT tt = _tt_res;
    uint32_t offset = 0;
    uint32_t num_blocks = ( _num_vars > 6 ) ? ( 1u << ( _num_vars - 6 ) ) : 1;
    uint64_t constexpr masks[] = { 0x0, 0x3, 0xF, 0xFF, 0xFFFF, 0xFFFFFFFF };

    auto it = std::begin( tt );
    const uint32_t len = ( num_blocks == 1u ) ? ( 1 << ( _num_vars - _free_set_size ) ) : ( 64u >> _free_set_size );
    for ( auto i = 0u; i < num_blocks; ++i )
    {
      for ( auto j = 0; j < len; ++j )
      {
        uint64_t val = *it & masks[_free_set_size];

        auto el = column_to_iset.find( val );
        if ( el != column_to_iset.end() )
        {
          isets[el->second]._bits[i / ( 1u << _free_set_size )] |= UINT64_C( 1 ) << ( j + offset );
        }
        else
        {
          assert( column_to_iset.size() < _multiplicity );
          isets[column_to_iset.size()]._bits[i / ( 1u << _free_set_size )] |= UINT64_C( 1 ) << ( j + offset );
          column_to_iset[val] = column_to_iset.size();
        }

        *it >>= ( 1u << _free_set_size );
      }

      offset = ( offset + ( 64 >> _free_set_size ) ) & 0x3F;
      ++it;
    }

    /* extend isets to cover the whole truth table */
    for ( TT& iset : isets )
    {
      tt_expansion( iset, isets_support );
    }

    /* save free_set functions */
    std::vector<TT> free_set_tts( _multiplicity );

    for ( auto const& pair : column_to_iset )
    {
      free_set_tts[pair.second]._bits[0] = pair.first;
      tt_expansion( free_set_tts[pair.second], _free_set_size );
    }

    _free_set = std::move( free_set_tts );

		return isets;
	}

	void tt_expansion( TT& tt, uint32_t num_vars_local )
	{
		if ( num_vars_local < 6u )
		{
			auto mask = *tt.begin();
			for ( uint32_t i = num_vars_local; i < _num_vars; ++i )
			{
				mask |= ( mask << ( 1 << i ) );
			}

			std::fill( tt.begin(), tt.end(), mask );
		}
		else
    {
			uint32_t num_blocks = ( 1u << ( num_vars_local - 6u ) );
			auto it = tt.begin();
			while ( it != tt.end() )
			{
				it = std::copy( tt.cbegin(), tt.cbegin() + num_blocks, it );
			}
		}
	}

	void compute_encodings()
	{
    uint32_t count = 0;

    /* enable don't cares only if not a power of 2 */
    uint32_t num_combs = 2;
    if ( __builtin_popcount( _multiplicity ) == 1 )
    {
      uint32_t num_combs_exact[4] = { 1, 3, 35, 6435 };
      for ( uint32_t i = 0; i < 4; ++i )
      {
        if ( ( _multiplicity >> i ) == 2u )
        {
          num_combs = num_combs_exact[i];
        }
      }
      _min_support_encodings = std::vector<std::array<uint32_t, 2>>( num_combs );
      compute_encodings_rec<false, true>( 0, 0, 0, count, _multiplicity >> 1, true );
      assert( count == num_combs );
      return;
    }

    /* constrain the number of offset classes for a strict encoding */
    int32_t min_set_size = 1;
    if ( _multiplicity <= 4 )
      min_set_size = 2;
    else if ( _multiplicity <= 8 )
      min_set_size = 4;
    else
      min_set_size = 8;
    min_set_size = _multiplicity - min_set_size;

    if ( _multiplicity > 8 )
    {
      /* distinct elements in 2 indistinct bins with at least `min_set_size` elements in the indistinct bins */
      uint32_t class_sizes[13] = { 3, 3, 15, 25, 35, 35, 255, 501, 957, 1749, 3003, 4719, 6435 };
      num_combs = class_sizes[_multiplicity - 3];
      _min_support_encodings = std::vector<std::array<uint32_t, 2>>( num_combs );
      compute_encodings_rec<false, false>( 0, 0, 0, count, min_set_size, true );
    }
    else
    {
      /* distinct elements in 3 bins, of which 2 are indistinct, and with at least `min_set_size` elements in the indistinct bins */
      uint32_t class_sizes[13] = { 6, 3, 90, 130, 105, 35, 9330, 23436, 48708, 78474, 91377, 70785, 32175 };
      num_combs = class_sizes[_multiplicity - 3];
      _min_support_encodings = std::vector<std::array<uint32_t, 2>>( num_combs );
      compute_encodings_rec<true, false>( 0, 0, 0, count, min_set_size, true );
    }

    assert( count == num_combs );
	}

	template<bool enable_dc, bool equal_size_partition>
	void compute_encodings_rec( uint32_t onset, uint32_t offset, uint32_t iset_index, uint32_t& count, int32_t min_num_isets, bool is_first )
	{
    if ( iset_index == _multiplicity )
    {
      if ( equal_size_partition )
      {
        /* sets must be equally populated */
        if ( __builtin_popcount( onset ) != __builtin_popcount( offset ) )
        {
          return;
        }
      }
      else if ( __builtin_popcount( onset ) < min_num_isets || __builtin_popcount( offset ) < min_num_isets )
      {
        /* ON-set and OFF-set must be populated with at least min_set_size elements */
        return;
      }

      _min_support_encodings[count][0] = onset;
      _min_support_encodings[count][1] = offset;
      ++count;

      return;
    }

    /* var in DCSET */
    if ( enable_dc )
    {
      compute_encodings_rec<enable_dc, equal_size_partition>( onset, offset, iset_index + 1, count, min_num_isets, is_first );
    }

    /* move var in ONSET */
    onset |= 1 << iset_index;
    compute_encodings_rec<enable_dc, equal_size_partition>( onset, offset, iset_index + 1, count, min_num_isets, false );
    onset &= ~( 1 << iset_index );

    /* remove symmetries */
    if ( is_first )
    {
      return;
    }

    /* move var in OFFSET */
    offset |= 1 << iset_index;
    compute_encodings_rec<enable_dc, equal_size_partition>( onset, offset, iset_index + 1, count, min_num_isets, false );
    offset &= ~( 1 << iset_index );
	}

	bool solve_encoding_exact( std::vector<TT> const& isets )
	{
    std::vector<encoding_column> matrix;
    matrix.reserve( _min_support_encodings.size() );
    _bound_set.clear();

    /* create covering matrix */
    create_covering_matrix<false>( isets, matrix, false );
    if ( matrix.size() == 0u )
    {
      return false;
    }

    /* solve the covering problem */
    std::array<uint32_t, 6> solution = solve_covering_exact( matrix );

    /* check for failed decomposition */
    if ( solution[0] == UINT32_MAX )
    {
      return false;
    }

    /* compute best bound sets */
    uint32_t isets_support = _num_vars - _free_set_size;
    _care_set.clear();
    _isets_onset.clear();
    _isets_offset.clear();
    for ( uint32_t i = 0; i < solution[5]; ++i )
    {
      TT tt;
      TT care;

      const uint32_t onset = _min_support_encodings[matrix[solution[i]].index][0];
      const uint32_t offset = _min_support_encodings[matrix[solution[i]].index][1];
      for ( uint32_t j = 0; j < _multiplicity; ++j )
      {
        if ( ( ( onset >> j ) & 1 ) )
        {
          tt |= isets[j];
        }
        if ( ( ( offset >> j ) & 1 ) )
        {
          care |= isets[j];
        }
      }

      care |= tt;

      _bound_set.push_back( tt );
      /* `_care_set` could be exploited to obtain lower-MC BS functions */
      _care_set.push_back( care );
      _isets_onset.push_back( onset );
      _isets_offset.push_back( offset );
    }

    return true;
	}

	bool solve_encoding_heuristic( std::vector<TT> const& isets )
	{
    std::vector<encoding_column> matrix;
    matrix.reserve( _min_support_encodings.size() );
    _bound_set.clear();

    /* create covering matrix */
    create_covering_matrix<true>( isets, matrix, true );

    /* solve the covering problem: heuristic pass + local search */
    std::array<uint32_t, 6> solution = solve_covering_heuristic( matrix );

    /* check for failed decomposition */
    if ( solution[0] == UINT32_MAX )
    {
      return false;
    }

    /* improve solution with local search */
    while ( improve_covering( matrix, solution ) )
      ;

    /* compute best bound sets */
    uint32_t isets_support = _num_vars - _free_set_size;
    _care_set.clear();
    _isets_onset.clear();
    _isets_offset.clear();
    for ( uint32_t i = 0; i < solution[5]; ++i )
    {
      TT tt;
      TT care;

      const uint32_t onset = _min_support_encodings[matrix[solution[i]].index][0];
      const uint32_t offset = _min_support_encodings[matrix[solution[i]].index][1];
      for ( uint32_t j = 0; j < _multiplicity; ++j )
      {
        if ( ( ( onset >> j ) & 1 ) )
        {
          tt |= isets[j];
        }
        if ( ( ( offset >> j ) & 1 ) )
        {
          care |= isets[j];
        }
      }

      care |= tt;

      _bound_set.push_back( tt );
      _care_set.push_back( care );
      _isets_onset.push_back( onset );
      _isets_offset.push_back( offset );
    }

    return true;
	}

	template<bool use_heuristic>
	void create_covering_matrix( std::vector<TT> const& isets, std::vector<encoding_column>& matrix, bool sort )
	{
    uint32_t combinations = ( _multiplicity * ( _multiplicity - 1 ) ) / 2;
    uint32_t iset_support = _num_vars - _free_set_size;

    /* insert dichotomies */
    for ( uint32_t i = 0; i < _min_support_encodings.size(); ++i )
    {
      uint32_t const onset = _min_support_encodings[i][0];
      uint32_t const offset = _min_support_encodings[i][1];

      /* compute function and distinguishable seed dichotomies */
      uint64_t column[2] = { 0, 0 };
      TT tt;
      TT care;
      uint32_t pair_pointer = 0;
      for ( uint32_t j = 0; j < _multiplicity; ++j )
      {
        auto onset_shift = ( onset >> j );
        auto offset_shift = ( offset >> j );
        if ( ( onset_shift & 1 ) )
        {
          tt |= isets[j];
        }

        if ( ( offset_shift & 1 ) )
        {
          care |= isets[j];
        }

        /* compute included seed dichotomies */
        for ( uint32_t k = j + 1; k < _multiplicity; ++k )
        {
          /* if in different sets */
          if ( ( ( ( onset_shift & ( offset >> k ) ) | ( ( onset >> k ) & offset_shift ) ) & 1 ) )
          {
            column[pair_pointer >> 6u] |= UINT64_C( 1 ) << ( pair_pointer & 0x3F );
          }

          ++pair_pointer;
        }
      }

      care |= tt;

      /* compute cost */
      uint32_t cost = 0;
      for ( uint32_t j = 0; j < iset_support; ++j )
      {
        cost += ( kitty::has_var( tt, care, j ) ? 1 : 0 );
      }

      /* discard solutions with support over LUT size */
      if ( cost > _ps.lut_size )
      {
        fmt::print( "[m] requires too many supports ({} vs. {})! Skipping...\n", cost, _ps.lut_size );
        continue;
      }

      /* buffers have zero cost */
      if ( cost == 1 )
        cost = 0;

      float sort_cost = 0;
      if ( use_heuristic )
      {
        sort_cost = 1.0f / ( __builtin_popcountl( column[0] ) + __builtin_popcountl( column[1] ) );
      }
      else
      {
        sort_cost = cost + ( ( combinations - __builtin_popcountl( column[0] + __builtin_popcountl( column[1] ) ) ) << _num_vars );
      }

      /* insert */
      matrix.emplace_back( encoding_column{ { column[0], column[1] }, i, cost, sort_cost } );
    }

    if ( !sort )
    {
      return;
    }

    if ( use_heuristic )
    {
      std::sort( matrix.begin(), matrix.end(), [&]( encoding_column const& a, encoding_column const& b ) {
        return a.cost < b.cost;
      } );
    }
    else
    {
      std::sort( matrix.begin(), matrix.end(), [&]( encoding_column const& a, encoding_column const& b ) {
        return a.sort_cost < b.sort_cost;
      } );
    }
	}

	std::array<uint32_t, 6u> solve_covering_exact( std::vector<encoding_column> const& matrix )
	{
		std::array<uint32_t, 6u> solution = { UINT32_MAX };
		uint32_t best_cost = UINT32_MAX;
		uint32_t num_combs = ( _multiplicity * ( _multiplicity - 1u ) ) / 2;

		if ( _multiplicity <= 2u )
		{
			solution[5] = 1u;
			solution[0] = 0u;
		}
		else
		{
			/* 2u < _multiplicity <= 4u */
			solution[5] = 2u;
			for ( uint32_t i = 0u; i < matrix.size() - 1u; ++i )
			{
				for ( uint32_t j = i + 1u; j < matrix.size(); ++j )
				{
					if ( matrix[i].cost + matrix[j].cost >= best_cost )
					{
						continue;
					}

					if ( __builtin_popcountl( matrix[i].column[0] | matrix[j].column[0] ) +
					     __builtin_popcountl( matrix[i].column[1] | matrix[j].column[1] ) == num_combs )
					{
						solution[0] = i;
						solution[1] = j;
						best_cost = matrix[i].cost + matrix[j].cost;
					}
				}
			}
		}

		return solution;
	}

	std::array<uint32_t, 6u> solve_covering_heuristic( std::vector<encoding_column>& matrix )
	{
		std::array<uint32_t, 6u> solution = { UINT32_MAX };
		uint32_t num_combs = ( _multiplicity * ( _multiplicity - 1u ) ) / 2;
		uint64_t column0 = 0u, column1 = 0u;
		float best_sort_cost = std::numeric_limits<float>::max();
		uint32_t best_index = 0u;

		for ( uint32_t i = 0u; i < matrix.size(); ++i )
		{
			if ( matrix[i].sort_cost < best_sort_cost )
			{
				best_index = i;
				best_sort_cost = matrix[i].sort_cost;
			}
		}

		/* select the `best_index`-th column */
		column0 = matrix[best_index].column[0];
		column1 = matrix[best_index].column[1];
		std::swap( matrix[0], matrix[best_index] );

		uint32_t pivot = 1u;
		while ( ( pivot < ( _ps.lut_size - _free_set_size ) ) &&
		        ( ( __builtin_popcountl( column0 ) + __builtin_popcountl( column1 ) ) != num_combs ) )
		{
			best_index = 0u;
			best_sort_cost = std::numeric_limits<float>::max();

			/* among the remaining columns, select the one that minimizes the sort cost */
			for ( uint32_t i = pivot; i < matrix.size(); ++i )
			{
				float sort_cost = 1.0f / ( __builtin_popcountl( matrix[i].column[0] & ~column0 ) + __builtin_popcountl( matrix[i].column[1] & ~column1 ) );
				if ( sort_cost < best_sort_cost )
				{
					best_index = i;
					best_sort_cost = sort_cost;
				}
			}

			column0 |= matrix[best_index].column[0];
			column1 |= matrix[best_index].column[1];
			std::swap( matrix[pivot], matrix[best_index] );
			++pivot;
		}

		if ( __builtin_popcountl( column0 ) + __builtin_popcountl( column1 ) == num_combs )
		{
			for ( uint32_t i = 0u; i < pivot; ++i )
			{
				solution[i] = i;
			}
			solution[5] = pivot;
		}

		return solution;
	}

	bool improve_covering( std::vector<encoding_column> const& matrix, std::array<uint32_t, 6>& solution )
	{
		uint32_t best_cost = 0u, current_cost = 0u;
		uint32_t num_elements = solution[5];
		uint32_t num_combs = ( _multiplicity * ( _multiplicity - 1u ) ) / 2;
		bool improved = false;

		for ( uint32_t i = 0u; i < num_elements; ++i )
		{
			best_cost += matrix[solution[i]].cost;
		}

		uint64_t column0 = 0u, column1 = 0u;
		/* for each currently selected column, look for a lower-cost alternative */
		for ( uint32_t i = 0u; i < num_elements; ++i )
		{
			current_cost = best_cost - matrix[solution[i]].cost;
			column0 = 0u;
			column1 = 0u;

			for ( uint32_t j = 0u; j < num_elements; ++j )
			{
				if ( j == i )
				{
					continue;
				}
				column0 |= matrix[solution[j]].column[0];
				column1 |= matrix[solution[j]].column[1];
			}

			for ( uint32_t j = 0u; j < matrix.size(); ++j )
			{
				if ( ( __builtin_popcountl( column0 | matrix[j].column[0] ) + __builtin_popcountl( column1 | matrix[j].column[1] ) ) != num_combs )
				{
					continue;
				}

				if ( ( current_cost + matrix[j].cost ) < best_cost )
				{
					solution[i] = j;
					best_cost = current_cost + matrix[j].cost;
					improved = true;
				}
			}
		}

		return improved;
	}

  void compute_top_mux()
	{
    const uint32_t num_top_vars = _decomp_bs.size() + _free_set.size();
    /* always consider heuristic top MUX construction */
    const bool comput_mux = false;
    decomp_result decomp_res;
    std::vector<kitty::dynamic_truth_table> fs_vars{};
    _decomp_fs.clear();
    _decomp_fs.resize( _free_set.size() );
    _sel_polar = 0u;

    std::vector<uint32_t> free_set_vars( _free_set_size );
    for ( auto i{ 0u }; i < _free_set_size; ++i )
    {
      free_set_vars[i] = _perm[i];
    }

    /* determine `_decomp_fs` */
    for ( auto i{ 0u }; i < _free_set.size(); ++i )
    {
      decomp_result dec;
      dec.support = free_set_vars;
      auto tt = _free_set[i];
      dec.tt = kitty::shrink_to( tt, _free_set_size );
      _decomp_fs[i] = dec;
      fs_vars.emplace_back( num_top_vars );
      kitty::create_nth_var( fs_vars[i], i );
      decomp_res.support.push_back( _num_vars + i );
    }

    std::vector<kitty::dynamic_truth_table> bs_vars;
    uint32_t offset = 0u;
    std::vector<uint32_t> ss_ind;

    for ( uint32_t i = 0u; i < _decomp_bs.size(); ++i )
    {
      decomp_result const& decomp_res_i = _decomp_bs[i];
      bs_vars.emplace_back( num_top_vars );
      kitty::create_nth_var( bs_vars[i], _free_set.size() + i );

      if ( decomp_res_i.support.size() == 1u )
      {
        decomp_res.support.push_back( decomp_res_i.support[0] );
        if ( decomp_res_i.tt._bits[0] & 1 )
        {
          bs_vars[i] = ~bs_vars[i];
          _sel_polar |= ( 1 << i );
        }

        ss_ind.push_back( i );
        ++offset;
      }
      else
      {
        decomp_res.support.push_back( _num_vars + _free_set.size() + i - offset );
      }
    }

    for ( auto it = ss_ind.rbegin(); it != ss_ind.rend(); ++it )
    {
      _decomp_bs.erase( _decomp_bs.begin() + *it );
    }

    kitty::dynamic_truth_table tt( num_top_vars );
    if ( comput_mux )
    {
      for ( auto i{ 0u }; i < fs_vars.size(); ++i )
      {
        kitty::dynamic_truth_table fs_tt = fs_vars[i];
        for ( auto j{ 0u }; j < bs_vars.size(); ++j )
        {
          if ( ( _isets_onset[j] >> i ) & 1 )
          {
            fs_tt &= bs_vars[j];
            continue;
          }
          if ( ( _isets_offset[j] >> i ) & 1 )
          {
            fs_tt &= ~bs_vars[j];
          }
        }
        tt |= fs_tt;
      }
    }
    else
    {
      /* derive the encodings */
      _encodings.clear();
      _encodings.resize( _free_set.size() );
      for ( auto i{ 0u }; i < fs_vars.size(); ++i )
      {
        _encodings[i] = { 0u, 0u };
        kitty::dynamic_truth_table fs_tt = fs_vars[i];
        for ( auto j{ 0u }; j < bs_vars.size(); ++j )
        {
          if ( ( _isets_onset[j] >> i ) & 1 )
          {
            _encodings[i].value |= ( 1u << j );
            _encodings[i].mask |= ( 1u << j );
            fs_tt &= bs_vars[j];
            continue;
          }
          if ( ( _isets_offset[j] >> i ) & 1 )
          {
            _encodings[i].mask |= ( 1u << j );
            fs_tt &= ~bs_vars[j];
          }
        }
        tt |= fs_tt;
      }
    }
    decomp_res.tt = tt;
    _decomp_bs.push_back( decomp_res );
  }

private:
	acdecomp_max_local_params _ps;
	TT _tt_res;
	kitty::dynamic_truth_table const& _tt_orig;
	uint32_t _num_vars = 0u;
	std::array<uint32_t, max_num_vars> _perm;
	uint32_t _multiplicity = UINT32_MAX;
	uint32_t _free_set_size = UINT32_MAX; /* FS support size */
	std::vector<TT> _free_set; /* FS functions */
	std::vector<TT> _bound_set; /* BS functions */
	std::vector<TT> _care_set;
	std::vector<uint64_t> _isets_onset;
	std::vector<uint64_t> _isets_offset;
	std::vector<std::array<uint32_t, 2u>> _min_support_encodings;
public:
	std::vector<decomp_result> _decomp_bs;
  std::vector<decomp_result> _decomp_fs;
  std::vector<masked_encoding> _encodings;
  uint8_t _sel_polar;
  bool _min_mc_mux{};
};
} /* namespace mockturtle */