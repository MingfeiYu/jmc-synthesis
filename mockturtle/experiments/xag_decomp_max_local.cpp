#include <mockturtle/algorithms/xag_decomp_max_local.hpp>
#include <mockturtle/properties/mccost.hpp>
#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_bristol.hpp>

#include <chrono>

#include "experiments.hpp"

static const std::string arith_cf_benchmark[] = {
  "INT-add",
  "INT-div",
  "INT-mul",
  "FP-add",
  "FP-div",
  "FP-mul",
  "FP-eq",
};

static const std::string crypto_gp_benchmark[] = {
  "AES",
  "SHA3",
  "Hamming",
  "Encoder",
  "LiteMIPS"
};

static const uint32_t arith_cf_num_parties[] = {
  2u, 2u, 2u, 2u, 2u, 2u, 2u
};

static const uint32_t crypto_gp_num_parties[] = {
  2u, 2u, 2u, 2u, 2u
};

int main()
{
  std::string db_name = "xag_mc_min_6_db";
  std::string benchmark_suite_name{};

  for ( std::size_t suite_id = 0 ; suite_id < 2; ++suite_id )
  {
  const std::string* benchmark_suite = ( suite_id == 0 ) ? arith_cf_benchmark : crypto_gp_benchmark;
  const uint32_t* benchmark_suite_num_parties = ( suite_id == 0 ) ? arith_cf_num_parties : crypto_gp_num_parties;
  std::size_t benchmark_suite_size = ( suite_id == 0 ) ? std::size( arith_cf_benchmark ) : std::size( crypto_gp_benchmark );
  benchmark_suite_name = ( suite_id == 0 ) ? "arith_cf" : "crypto_gp";

  experiments::experiment<std::string, uint32_t, uint32_t, uint32_t, uint32_t, float> exp( ( fmt::format( "hybrid_{}_max_local", benchmark_suite_name ) ), "benchmark", "AND count", "joint AND count", "opt. AND count", "opt. joint AND count", "runtime [s]" );

  for ( auto i = 0u; i < benchmark_suite_size; ++i )
  {
    std::string benchmark = benchmark_suite[i];
    uint32_t num_parties = benchmark_suite_num_parties[i];
    fmt::print( "[i] Processing {}...\n", benchmark );
    mockturtle::xag_network ntk;
    const std::string path = "../experiments/" + benchmark_suite_name + "_benchmarks/" + benchmark + ".v";
    const auto read_stats = lorina::read_verilog( ( path ), mockturtle::verilog_reader( ntk, benchmark ) );
    if ( read_stats != lorina::return_code::success )
    {
      fmt::print( "[e] Cannot access {}... Failed to read benchmark...\n", path );
      abort();
    }

    const uint32_t num_ands = *mockturtle::multiplicative_complexity( ntk );

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
      mockturtle::xag_decomp_max_local_stats st;
      if ( !first )
      {
        ntk_mpc = mockturtle::owner_view<mockturtle::xag_network>( ntk, num_parties );
      }
      first = false;

      mockturtle::xag_decomp_max_local_in_place decomp( num_parties, ntk_mpc, db_name, ps, st );
      decomp.run();
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

    exp( benchmark, num_ands, cost_orig, num_ands_opt, cost_opt, duration.count() );

    fmt::print( "[i] Identifying the new input frontier...\n" );
    auto res = mockturtle::identify_frontier_and_extract_mpc( ntk, num_parties );
    for ( auto j{ 0u }; j < res.num_frontier_per_party.size(); ++j )
    {
      fmt::print( "[i] {} input bits required from the {}-th party\n", res.num_frontier_per_party[j], ( j + 1 ) );
    }
    fmt::print( "\n" );

    mockturtle::write_bristol_params wbps{};
    wbps.input_partition = res.num_frontier_per_party;
    wbps.promote_largest_input_block_to_front = true;
    auto partition = wbps.input_partition;
    if ( wbps.promote_largest_input_block_to_front && !partition.empty() )
    {
      auto it = std::max_element( partition.begin(), partition.end() );
      std::rotate( partition.begin(), it, it + 1 );
    }
    std::string partition_suffix{};
    for ( auto v: partition )
    {
      partition_suffix += fmt::format( "_{}", v );
    }
    mockturtle::write_bristol( res.mpc_ntk , fmt::format( "../experiments/{}_benchmarks/jmc_opt_bristol/{}_opt{}.txt", benchmark_suite_name, benchmark, partition_suffix ), wbps );
  }

  exp.save();
  exp.table();
  }

  return 0;
}
