/* Sensitivity of the JMC reduction and of the runtime to the cut-size bound
 * kappa.  Follows `xag_decomp_max_local.cpp`, except that kappa is taken from
 * the command line and that no Bristol files are written.
 *
 * The cut-size bound is a compile-time constant (`JMC_NUM_VARS`); the wrappers
 * `xag_kappa<k>.cpp` instantiate this program for the remaining values of
 * kappa.  The priority-cut budget can be overridden with JMC_CUT_LIMIT.
 *
 * Usage: ./experiments/xag_decomp_kappa_sweep <kappa> [benchmark ...]
 * To be run from the build directory, which must contain `xag_mc_min_6_db`.
 */

#include <mockturtle/algorithms/xag_decomp_max_local.hpp>
#include <mockturtle/properties/mccost.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/views/depth_view.hpp>

#include <chrono>
#include <cstdlib>
#include <set>
#include <string>

#include "experiments.hpp"

static const std::string arith_cf_benchmark[] = {
  "INT-add", "INT-div", "INT-mul", "FP-add", "FP-div", "FP-mul", "FP-eq",
};

static const std::string crypto_gp_benchmark[] = {
  "AES", "SHA3", "Hamming", "Encoder", "LiteMIPS"
};

static const uint32_t arith_cf_num_parties[] = { 2u, 2u, 2u, 2u, 2u, 2u, 2u };
static const uint32_t crypto_gp_num_parties[] = { 2u, 2u, 2u, 2u, 2u };

/* multiplicative depth: XOR gates are free in GC, only AND levels are counted */
struct and_depth_cost
{
  uint32_t operator()( mockturtle::xag_network const& ntk, mockturtle::xag_network::node const& n ) const
  {
    return ntk.is_and( n ) ? 1u : 0u;
  }
};

static std::pair<uint32_t, uint32_t> depths( mockturtle::xag_network const& ntk )
{
  mockturtle::depth_view<mockturtle::xag_network> d_all{ ntk };
  mockturtle::depth_view<mockturtle::xag_network, and_depth_cost> d_and{ ntk };
  return { d_all.depth(), d_and.depth() };
}

int main( int argc, char** argv )
{
  uint32_t kappa = ( argc > 1 ) ? static_cast<uint32_t>( std::atoi( argv[1] ) ) : 8u;

  /* restrict the run to the benchmarks given on the command line */
  std::set<std::string> only;
  for ( int i = 2; i < argc; ++i )
    only.insert( std::string( argv[i] ) );

  std::string db_name = "xag_mc_min_6_db";

  fmt::print( "[i] kappa = {}\n", kappa );
  fmt::print( "CSV,suite,benchmark,kappa,and,joint_and,opt_and,opt_joint_and,runtime_s,depth,and_depth,opt_depth,opt_and_depth\n" );

  for ( std::size_t suite_id = 0; suite_id < 2; ++suite_id )
  {
    const std::string* benchmark_suite = ( suite_id == 0 ) ? arith_cf_benchmark : crypto_gp_benchmark;
    const uint32_t* benchmark_suite_num_parties = ( suite_id == 0 ) ? arith_cf_num_parties : crypto_gp_num_parties;
    std::size_t benchmark_suite_size = ( suite_id == 0 ) ? std::size( arith_cf_benchmark ) : std::size( crypto_gp_benchmark );
    std::string benchmark_suite_name = ( suite_id == 0 ) ? "arith_cf" : "crypto_gp";

    for ( auto i = 0u; i < benchmark_suite_size; ++i )
    {
      std::string benchmark = benchmark_suite[i];
      if ( !only.empty() && only.count( benchmark ) == 0 )
        continue;

      uint32_t num_parties = benchmark_suite_num_parties[i];
      fmt::print( "[i] Processing {} (kappa={})...\n", benchmark, kappa );

      mockturtle::xag_network ntk;
      const std::string path = "../experiments/" + benchmark_suite_name + "_benchmarks/" + benchmark + ".v";
      const auto read_stats = lorina::read_verilog( path, mockturtle::verilog_reader( ntk, benchmark ) );
      if ( read_stats != lorina::return_code::success )
      {
        fmt::print( "[e] Cannot access {}... Failed to read benchmark...\n", path );
        abort();
      }

      const uint32_t num_ands = *mockturtle::multiplicative_complexity( ntk );
      const auto [depth_orig, and_depth_orig] = depths( ntk );
      setenv( "JMC_DUMP_TAG", benchmark.c_str(), 1 );

      bool until_saturation{ true };
      mockturtle::owner_view<mockturtle::xag_network> ntk_mpc( ntk, num_parties );
      const uint32_t cost_orig = mockturtle::costs<mockturtle::owner_view<mockturtle::xag_network>, mockturtle::mc_mpc_cost>( ntk_mpc );
      uint32_t cost_cur = cost_orig;
      uint32_t cost_opt{ 0u };
      uint32_t iter_cnt{ 1u };
      bool first{ true };

      const auto begin_time = std::chrono::high_resolution_clock::now();
      do
      {
        mockturtle::xag_decomp_max_local_params ps;
        ps.cut_enum_ps.cut_size = kappa;
        if ( const char* cl = std::getenv( "JMC_CUT_LIMIT" ) )
        {
          ps.cut_enum_ps.cut_limit = static_cast<uint32_t>( std::atoi( cl ) );
        }
        mockturtle::xag_decomp_max_local_stats st;
        if ( !first )
        {
          ntk_mpc = mockturtle::owner_view<mockturtle::xag_network>( ntk, num_parties );
        }
        first = false;

        mockturtle::xag_decomp_max_local_in_place decomp( num_parties, ntk_mpc, db_name, ps, st );
        decomp.run();
        fmt::print( "STAT,{},{},{},{},{},{}\n", benchmark, kappa, iter_cnt, st.num_decomp, st.num_decomp_success, st.num_better_impl );
        mockturtle::xag_network ntk_opt = ntk_mpc.toNtk();
        cost_opt = mockturtle::costs<mockturtle::owner_view<mockturtle::xag_network>, mockturtle::mc_mpc_cost>( ntk_mpc );
        st.cost_optimal = cost_opt;
        if ( cost_opt < cost_cur )
        {
          cost_cur = cost_opt;
          ntk = ntk_opt;
          fmt::print( "[m] Finished the {}-th round of optimization\n", iter_cnt++ );
        }
        else
        {
          fmt::print( "[m] Finished the {}-th round of optimization\n", iter_cnt );
          break;
        }
      } while ( until_saturation );

      std::chrono::duration<float> duration = std::chrono::high_resolution_clock::now() - begin_time;

      const uint32_t num_ands_opt = *mockturtle::multiplicative_complexity( ntk );
      const auto [depth_opt, and_depth_opt] = depths( ntk );

      fmt::print( "CSV,{},{},{},{},{},{},{},{:.3f},{},{},{},{}\n",
                  benchmark_suite_name, benchmark, kappa,
                  num_ands, cost_orig, num_ands_opt, cost_opt, duration.count(),
                  depth_orig, and_depth_orig, depth_opt, and_depth_opt );
      std::fflush( stdout );
    }
  }

  return 0;
}
