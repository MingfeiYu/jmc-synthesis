#include <mockturtle/io/verilog_reader.hpp>
#include <mockturtle/io/write_bristol.hpp>
#include <mockturtle/networks/xag.hpp>

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
  std::string benchmark_suite_name{};

  for ( std::size_t suite_id = 0 ; suite_id < 2; ++suite_id )
  {
    const std::string* benchmark_suite = ( suite_id == 0 ) ? arith_cf_benchmark : crypto_gp_benchmark;
    const uint32_t* benchmark_suite_num_parties = ( suite_id == 0 ) ? arith_cf_num_parties : crypto_gp_num_parties;
    std::size_t benchmark_suite_size = ( suite_id == 0 ) ? std::size( arith_cf_benchmark ) : std::size( crypto_gp_benchmark );
    benchmark_suite_name = ( suite_id == 0 ) ? "arith_cf" : "crypto_gp";
    
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
      fmt::print( "[i] Read benchmark successfully.\n" );

      mockturtle::write_bristol_params wbps{};
      std::vector<uint32_t> num_frontier_per_party( num_parties, ( ntk.num_pis() / num_parties ) );
      wbps.input_partition = num_frontier_per_party;
      std::string partition_suffix{};
      for ( auto v: num_frontier_per_party )
      {
        partition_suffix += fmt::format( "_{}", v );
      }
      mockturtle::write_bristol( ntk, fmt::format( "../experiments/{}_benchmarks/{}{}.txt", benchmark_suite_name, benchmark, partition_suffix ), wbps );

      fmt::print( "[i] Convert circuit successfully.\n" );
    }
  }
}