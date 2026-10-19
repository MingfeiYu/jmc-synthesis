#include <catch.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <kitty/operators.hpp>
#include <mockturtle/algorithms/acdecomp_max_local.hpp>
#include <mockturtle/algorithms/mc_analyzer.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/xag.hpp>

#include <algorithm>
#include <vector>

using namespace mockturtle;

namespace
{

mc_analyzer& database()
{
  static mc_analyzer db( JMC_DB_PATH );
  return db;
}

std::vector<kitty::dynamic_truth_table> variables( uint32_t num_vars )
{
  std::vector<kitty::dynamic_truth_table> xs( num_vars, kitty::dynamic_truth_table( num_vars ) );
  for ( auto i = 0u; i < num_vars; ++i )
  {
    kitty::create_nth_var( xs[i], i );
  }
  return xs;
}

kitty::dynamic_truth_table ite( kitty::dynamic_truth_table const& s, kitty::dynamic_truth_table const& t, kitty::dynamic_truth_table const& e )
{
  return ( s & t ) | ( ~s & e );
}

/* `ownerships[0]` lists the jointly derived leaves, `ownerships[i]` those of the i-th party */
void check_decomposition( kitty::dynamic_truth_table const& tt, std::vector<std::vector<uint32_t>> const& ownerships )
{
  const uint32_t num_vars = tt.num_vars();
  auto& db = database();

  acdecomp_max_local acd( tt, acdecomp_max_local_params{} );
  REQUIRE( acd.multi_run( db, ownerships ) >= 0 );

  /* every free-set function reads the inputs of a single party only */
  std::vector<uint32_t> owner( num_vars );
  for ( auto i = 0u; i < ownerships.size(); ++i )
  {
    for ( auto leaf : ownerships[i] )
    {
      owner[leaf] = i;
    }
  }
  REQUIRE( !acd._decomp_fs.empty() );
  for ( auto const& fs : acd._decomp_fs )
  {
    for ( auto leaf : fs.support )
    {
      REQUIRE( leaf < num_vars );
      CHECK( owner[leaf] != 0u );
      CHECK( owner[leaf] == owner[fs.support.front()] );
    }
  }

  /* the synthesized decomposition implements the original function */
  xag_network xag;
  std::vector<xag_network::signal> pis( num_vars );
  std::generate( pis.begin(), pis.end(), [&]() { return xag.create_pi(); } );
  xag.create_po( db.min_mc_resyn( xag, pis, { acd._decomp_fs, acd._decomp_bs }, acd._encodings, acd._sel_polar, acd._min_mc_mux ) );

  CHECK( simulate<kitty::dynamic_truth_table>( xag, default_simulator<kitty::dynamic_truth_table>( num_vars ) )[0] == tt );
}

} // namespace

TEST_CASE( "ACD of six-input two-party functions", "[acdecomp_max_local]" )
{
  /* a, b, c belong to the first party, d, e, f to the second */
  const auto x = variables( 6u );
  const auto &a = x[0], &b = x[1], &c = x[2], &d = x[3], &e = x[4], &f = x[5];

  check_decomposition( ite( ( d & e ) ^ f, a & b & c, a ^ ( b | c ) ), { {}, { 0, 1, 2 }, { 3, 4, 5 } } );
  check_decomposition( kitty::ternary_majority( a, b, c ) ^ ( d & e & f ), { {}, { 0, 1, 2 }, { 3, 4, 5 } } );
}

TEST_CASE( "ACD with a jointly derived leaf", "[acdecomp_max_local]" )
{
  /* a, b, c belong to the first party, d, e to the second, f is jointly derived */
  const auto x = variables( 6u );
  const auto &a = x[0], &b = x[1], &c = x[2], &d = x[3], &e = x[4], &f = x[5];

  check_decomposition( ite( d ^ ( e & f ), a & ( b | c ), b ^ c ), { { 5 }, { 0, 1, 2 }, { 3, 4 } } );
}

TEST_CASE( "ACD of functions with more than six inputs", "[acdecomp_max_local]" )
{
  SECTION( "seven inputs, multiplicity three" )
  {
    /* a, b, c belong to the first party, d, e, f, g to the second */
    const auto x = variables( 7u );
    const auto &a = x[0], &b = x[1], &c = x[2], &d = x[3], &e = x[4], &f = x[5], &g = x[6];

    check_decomposition( ite( d & e, a & b, ite( f ^ g, b ^ c, a & c ) ), { {}, { 3, 4, 5, 6 }, { 0, 1, 2 } } );
  }

  SECTION( "eight inputs" )
  {
    /* a, b, c, d belong to the first party, e, f, g, h to the second */
    const auto x = variables( 8u );
    const auto &a = x[0], &b = x[1], &c = x[2], &d = x[3], &e = x[4], &f = x[5], &g = x[6], &h = x[7];

    check_decomposition( ite( ( e & f ) ^ ( g & h ), ( a & b ) ^ ( c & d ), a | b | c | d ), { {}, { 0, 1, 2, 3 }, { 4, 5, 6, 7 } } );
  }
}
