#include <catch.hpp>

#include <lorina/verilog.hpp>
#include <mockturtle/algorithms/equivalence_checking.hpp>
#include <mockturtle/algorithms/miter.hpp>
#include <mockturtle/algorithms/xag_decomp_max_local.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/networks/xag.hpp>
#include <mockturtle/properties/mccost.hpp>
#include <mockturtle/utils/cost_functions.hpp>

#include <vector>

using namespace mockturtle;

namespace
{

xag_network read_benchmark( std::string const& path, std::string const& module_name )
{
  xag_network xag;
  const auto result = lorina::read_verilog( path, verilog_reader( xag, module_name ) );
  REQUIRE( result == lorina::return_code::success );
  return xag;
}

uint32_t joint_ands( xag_network const& xag, uint32_t num_parties )
{
  owner_view<xag_network> ntk{ xag, num_parties };
  return costs<owner_view<xag_network>, mc_mpc_cost>( ntk );
}

/* repeats the optimization until the number of jointly derived AND gates stops decreasing */
xag_network optimize_until_saturation( xag_network ntk, uint32_t num_parties )
{
  uint32_t cost_cur = joint_ands( ntk, num_parties );
  while ( true )
  {
    owner_view<xag_network> ntk_mpc{ ntk, num_parties };
    xag_decomp_max_local_params ps;
    xag_decomp_max_local_stats st;
    xag_decomp_max_local_in_place decomp( num_parties, ntk_mpc, JMC_DB_PATH, ps, st );
    decomp.run();

    const uint32_t cost_opt = costs<owner_view<xag_network>, mc_mpc_cost>( ntk_mpc );
    if ( cost_opt >= cost_cur )
    {
      return ntk;
    }
    cost_cur = cost_opt;
    ntk = ntk_mpc.toNtk();
  }
}

} // namespace

TEST_CASE( "JMC optimization of Encoder", "[xag_decomp_max_local]" )
{
  const std::string path = JMC_EXPERIMENTS_PATH "crypto_gp_benchmarks/Encoder.v";
  const auto orig = read_benchmark( path, "Encoder" );

  /* numbers reported in Table 2 of the paper */
  CHECK( *multiplicative_complexity( orig ) == 53u );
  CHECK( joint_ands( orig, 2u ) == 10u );

  const auto opt = optimize_until_saturation( read_benchmark( path, "Encoder" ), 2u );

  CHECK( *multiplicative_complexity( opt ) == 53u );
  CHECK( joint_ands( opt, 2u ) == 6u );

  /* the optimization preserves the function of the circuit */
  const auto equivalent = equivalence_checking( *miter<xag_network>( orig, opt ) );
  REQUIRE( equivalent );
  CHECK( *equivalent );

  /* the extracted joint sub-circuit is the one evaluated in Table 4 */
  const auto res = identify_frontier_and_extract_mpc( opt, 2u );
  CHECK( res.num_frontier_per_party == std::vector<uint32_t>{ 5u, 5u } );
  CHECK( *multiplicative_complexity( res.mpc_ntk ) == 6u );
}
