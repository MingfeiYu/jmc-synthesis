#include <catch.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <mockturtle/algorithms/mc_analyzer.hpp>
#include <mockturtle/algorithms/simulation.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/properties/mccost.hpp>

#include <algorithm>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

using namespace mockturtle;

namespace
{

mc_analyzer& database()
{
  static mc_analyzer db( JMC_DB_PATH );
  return db;
}

kitty::dynamic_truth_table from_expression( uint32_t num_vars, std::string const& expression )
{
  kitty::dynamic_truth_table tt( num_vars );
  kitty::create_from_expression( tt, expression );
  return tt;
}

} // namespace

TEST_CASE( "MC of small functions from the database", "[mc_analyzer]" )
{
  auto& db = database();

  CHECK( db.get_mc( from_expression( 2u, "(ab)" ) ) == 1u );
  CHECK( db.get_mc( from_expression( 3u, "<abc>" ) ) == 1u );
  CHECK( db.get_mc( from_expression( 3u, "(abc)" ) ) == 2u );
  CHECK( db.get_mc( from_expression( 4u, "[(ab)(cd)]" ) ) == 2u );
  CHECK( db.get_mc( from_expression( 6u, "[<abc><def>]" ) ) == 2u );
}

TEST_CASE( "Synthesis by database lookup", "[mc_analyzer]" )
{
  auto& db = database();

  const std::vector<std::pair<uint32_t, std::string>> functions = {
      { 2u, "(ab)" },
      { 3u, "<abc>" },
      { 3u, "(abc)" },
      { 4u, "[(ab)(cd)]" },
      { 5u, "{(ab)[c(de)]}" },
      { 6u, "<<abc>de>" },
      { 6u, "[<abc><def>]" },
      { 6u, "{(ab)(cd)(ef)}" },
      { 6u, "[a(b[c(d[ef])])]" } };

  for ( auto const& [num_vars, expression] : functions )
  {
    const auto tt = from_expression( num_vars, expression );

    xag_network xag;
    std::vector<xag_network::signal> pis( num_vars );
    std::generate( pis.begin(), pis.end(), [&]() { return xag.create_pi(); } );

    /* a decomposition consisting of the top function only */
    decomp_result top{ tt, std::vector<uint32_t>( num_vars ) };
    std::iota( top.support.begin(), top.support.end(), 0u );
    xag.create_po( db.min_mc_resyn( xag, pis, { std::vector<decomp_result>{}, std::vector<decomp_result>{ top } }, {}, 0u, true ) );

    CHECK( simulate<kitty::dynamic_truth_table>( xag, default_simulator<kitty::dynamic_truth_table>( num_vars ) )[0] == tt );
    CHECK( *multiplicative_complexity( xag ) == db.get_mc( tt ) );
  }
}
