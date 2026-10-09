/* Compares the per-output database lookup used to synthesize the bound-set and
 * free-set functions against the exact multi-output MC optimum.
 *
 * Input: instance file written by `mc_analyzer` (JMC_DUMP_INSTANCES), one line
 * per free-set/bound-set group: tag kind n k tt_1;...;tt_k mc_db_sum new_ands
 * Output: CSV with one row per distinct multi-output function:
 *   n,k,tts,mc_db_sum,lb,opt,status,time_s
 * where `lb` is the largest single-output MC of the group, `opt` is the exact
 * multi-output optimum, and `status` is one of
 *   certified : mc_db_sum equals lb, hence optimal without calling the solver
 *   exact     : the solver found an implementation with fewer AND gates
 *   exact_sum : the solver proved that mc_db_sum is optimal
 *   unknown   : the conflict limit was reached, `opt` is a lower bound
 *
 * Usage: ./experiments/multi_output_mc_check <instances> <out.csv> [conflict_limit]
 * To be run from the build directory, which must contain `xag_mc_min_6_db`.
 */

#include <mockturtle/algorithms/exact_mc_synthesis_multi.hpp>
#include <mockturtle/algorithms/mc_analyzer.hpp>

#include <chrono>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct instance
{
  uint32_t n;
  std::vector<std::string> tts;
  uint32_t mc_sum;
};

/* reads the distinct instances, preserving the order of first appearance */
static std::vector<instance> read_instances( std::string const& path )
{
  std::vector<instance> instances;
  std::map<std::string, std::size_t> index;
  std::ifstream in( path );
  std::string line;

  while ( std::getline( in, line ) )
  {
    std::istringstream ss( line );
    std::string tag, kind, tts;
    uint32_t n, k, mc_sum, new_ands;
    if ( !( ss >> tag >> kind >> n >> k >> tts >> mc_sum >> new_ands ) )
    {
      continue;
    }

    const std::string key = std::to_string( n ) + " " + tts;
    if ( index.count( key ) )
    {
      continue;
    }

    instance inst;
    inst.n = n;
    inst.mc_sum = mc_sum;
    std::istringstream ts( tts );
    std::string tt;
    while ( std::getline( ts, tt, ';' ) )
    {
      inst.tts.push_back( tt );
    }
    index[key] = instances.size();
    instances.push_back( inst );
  }

  return instances;
}

int main( int argc, char** argv )
{
  if ( argc < 3 )
  {
    fmt::print( "usage: {} <instances> <out.csv> [conflict_limit]\n", argv[0] );
    return 1;
  }
  const std::string in_path = argv[1];
  const std::string out_path = argv[2];
  const uint32_t conflict_limit = ( argc > 3 ) ? static_cast<uint32_t>( std::atoi( argv[3] ) ) : 0u;

  mockturtle::mc_analyzer db( "xag_mc_min_6_db" );

  const auto instances = read_instances( in_path );
  fmt::print( "[i] {} distinct instances\n", instances.size() );

  std::ofstream out( out_path );
  out << "n,k,tts,mc_db_sum,lb,opt,status,time_s\n";

  uint32_t ctr = 0u;
  for ( auto const& inst : instances )
  {
    std::vector<kitty::dynamic_truth_table> funcs;
    uint32_t lb = 0u;
    for ( auto const& hex : inst.tts )
    {
      kitty::dynamic_truth_table tt( inst.n );
      kitty::create_from_hex_string( tt, hex );
      funcs.push_back( tt );
      const auto tt_n = kitty::get_bit( tt, 0 ) ? ~tt : tt;
      if ( !kitty::is_const0( tt_n ) )
      {
        lb = std::max<uint32_t>( lb, db.get_mc( tt ) );
      }
    }

    std::string tts_joined;
    for ( auto const& hex : inst.tts )
    {
      tts_joined += ( tts_joined.empty() ? "" : ";" ) + hex;
    }

    const auto t0 = std::chrono::high_resolution_clock::now();
    uint32_t opt = inst.mc_sum;
    std::string status;
    if ( inst.mc_sum <= lb )
    {
      status = "certified";
      opt = inst.mc_sum;
    }
    else
    {
      mockturtle::exact_mc_synthesis_multi_params ps;
      ps.min_and_gates = lb;
      ps.max_and_gates = inst.mc_sum - 1u;
      ps.conflict_limit = conflict_limit;
      const auto res = mockturtle::exact_mc_synthesis_multi( funcs, ps );
      switch ( res.status )
      {
      case mockturtle::exact_mc_synthesis_multi_status::optimum:
        status = "exact";
        opt = res.num_ands;
        break;
      case mockturtle::exact_mc_synthesis_multi_status::infeasible:
        status = "exact_sum";
        opt = inst.mc_sum;
        break;
      default:
        status = "unknown";
        opt = std::max( static_cast<uint32_t>( res.last_unsat + 1 ), lb );
        break;
      }
    }
    const std::chrono::duration<double> dt = std::chrono::high_resolution_clock::now() - t0;

    out << inst.n << "," << inst.tts.size() << "," << tts_joined << "," << inst.mc_sum << "," << lb << "," << opt << "," << status << "," << fmt::format( "{:.3f}", dt.count() ) << "\n";
    out.flush();
    if ( ++ctr % 100 == 0 )
    {
      fmt::print( "[i] {}/{} done\n", ctr, instances.size() );
    }
  }
  return 0;
}
