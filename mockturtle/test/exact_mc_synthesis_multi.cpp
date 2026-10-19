#include <catch.hpp>

#include <kitty/constructors.hpp>
#include <kitty/dynamic_truth_table.hpp>
#include <mockturtle/algorithms/exact_mc_synthesis_multi.hpp>

#include <string>
#include <vector>

using namespace mockturtle;

namespace
{

struct instance
{
  uint32_t num_vars;
  std::vector<std::string> tts;
  uint32_t num_ands;
};

std::vector<kitty::dynamic_truth_table> functions( instance const& inst )
{
  std::vector<kitty::dynamic_truth_table> funcs;
  for ( auto const& hex : inst.tts )
  {
    kitty::dynamic_truth_table tt( inst.num_vars );
    kitty::create_from_hex_string( tt, hex );
    funcs.push_back( tt );
  }
  return funcs;
}

/* groups from results/revision/optcheck_k8.csv, with their exact optimum */
const std::vector<instance> instances = {
    { 3u, { "09", "01" }, 2u },             /* database lookup: 3 */
    { 3u, { "12", "11" }, 2u },             /* database lookup: 2 */
    { 4u, { "0303", "aba9" }, 3u },         /* database lookup: 4 */
    { 4u, { "0000", "08c4", "00c0" }, 3u }, /* database lookup: 4 */
};

} // namespace

TEST_CASE( "Exact multi-output MC of small groups", "[exact_mc_synthesis_multi]" )
{
  for ( auto const& inst : instances )
  {
    const auto res = exact_mc_synthesis_multi( functions( inst ) );
    CHECK( res.status == exact_mc_synthesis_multi_status::optimum );
    CHECK( res.num_ands == inst.num_ands );
  }
}

TEST_CASE( "Infeasibility below the exact multi-output MC", "[exact_mc_synthesis_multi]" )
{
  for ( auto const& inst : instances )
  {
    exact_mc_synthesis_multi_params ps;
    ps.max_and_gates = inst.num_ands - 1u;
    const auto res = exact_mc_synthesis_multi( functions( inst ), ps );
    CHECK( res.status == exact_mc_synthesis_multi_status::infeasible );
  }
}
