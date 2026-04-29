#pragma once

#include "../traits.hpp"
#include "../utils/node_map.hpp"
#include "../views/topo_view.hpp"

#include <lorina/bristol.hpp>

#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mockturtle
{

struct write_bristol_params
{
  /*
   * Input partition in Bristol header format.
   * Example: {5, 4} means 2 input groups, with 5 and 4 wires.
   * If left empty, all PIs are emitted as one single input group.
   */
  std::vector<uint32_t> input_partition{};

  /*
   * Output partition in Bristol header format.
   * If left empty, all POs are emitted as one single output group.
   */
  std::vector<uint32_t> output_partition{};

  /*
   * If true, complemented signals are materialized via INV gates and cached.
   * This should normally remain true because Bristol has no edge complementation.
   */
  bool write_inverters_as_inv{ true };

  /*
   * If true, the input block with the largest size is moved to the front
   * before assigning Bristol PI wire indices. The relative order of all
   * remaining blocks is preserved. This is useful for making the largest
   * input owner become the first Bristol input group.
   */
  bool promote_largest_input_block_to_front{ false };

  bool verbose{ false };
};

namespace detail
{

struct bristol_gate
{
  std::vector<uint32_t> in;
  uint32_t out;
  std::string type;
};

inline uint32_t sum_partition( std::vector<uint32_t> const& partition )
{
  uint32_t total{ 0u };
  for ( auto const x : partition )
  {
    total += x;
  }
  return total;
}

template<class Ntk>
class bristol_writer_impl
{
public:
  using node = mockturtle::node<Ntk>;
  using signal = mockturtle::signal<Ntk>;

public:
  bristol_writer_impl( Ntk const& ntk, std::ostream& os, write_bristol_params const& ps )
      : ntk_( ntk ), os_( os ), ps_( ps ), node_to_wire_( ntk )
  {
  }

  void run()
  {
    static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
    static_assert( has_num_pis_v<Ntk>, "Ntk does not implement the num_pis method" );
    static_assert( has_num_pos_v<Ntk>, "Ntk does not implement the num_pos method" );
    static_assert( has_foreach_pi_v<Ntk>, "Ntk does not implement the foreach_pi method" );
    static_assert( has_foreach_po_v<Ntk>, "Ntk does not implement the foreach_po method" );
    static_assert( has_foreach_fanin_v<Ntk>, "Ntk does not implement the foreach_fanin method" );
    static_assert( has_get_node_v<Ntk>, "Ntk does not implement the get_node method" );
    static_assert( has_get_constant_v<Ntk>, "Ntk does not implement the get_constant method" );
    static_assert( has_is_constant_v<Ntk>, "Ntk does not implement the is_constant method" );
    static_assert( has_is_pi_v<Ntk>, "Ntk does not implement the is_pi method" );
    static_assert( has_is_and_v<Ntk>, "Ntk does not implement the is_and method" );
    static_assert( has_is_xor_v<Ntk>, "Ntk does not implement the is_xor method" );
    static_assert( has_is_complemented_v<Ntk>, "Ntk does not implement the is_complemented method" );

    assert( ntk_.is_combinational() && "Network has to be combinational" );

    prepare_partitions();
    assign_pi_wires();
    emit_internal_gates();
    emit_outputs();
    write_header_and_body();
  }

private:
  void prepare_partitions()
  {
    if ( ps_.input_partition.empty() )
    {
      input_partition_.push_back( ntk_.num_pis() );
    }
    else
    {
      input_partition_ = ps_.input_partition;
      if ( sum_partition( input_partition_ ) != ntk_.num_pis() )
      {
        std::cerr << "[e] input partition does not match number of PIs\n";
      }
      assert( sum_partition( input_partition_ ) == ntk_.num_pis() );
    }

    if ( ps_.output_partition.empty() )
    {
      output_partition_.push_back( ntk_.num_pos() );
    }
    else
    {
      output_partition_ = ps_.output_partition;
      if ( sum_partition( output_partition_ ) != ntk_.num_pos() )
      {
        std::cerr << "[e] output partition does not match number of POs\n";
      }
      assert( sum_partition( output_partition_ ) == ntk_.num_pos() );
    }
  }

  void assign_pi_wires()
  {
    std::vector<node> pis;
    pis.reserve( ntk_.num_pis() );
    ntk_.foreach_pi( [&]( auto const& n ) {
      pis.push_back( n );
    } );

    std::vector<uint32_t> block_order( input_partition_.size() );
    for ( uint32_t i = 0u; i < block_order.size(); ++i )
    {
      block_order[i] = i;
    }

    if ( ps_.promote_largest_input_block_to_front && !input_partition_.empty() )
    {
      uint32_t max_pos = 0u;
      for ( uint32_t i = 1u; i < input_partition_.size(); ++i )
      {
        if ( input_partition_[i] > input_partition_[max_pos] )
        {
          max_pos = i;
        }
      }

      if ( max_pos != 0u )
      {
        std::vector<uint32_t> promoted_order;
        promoted_order.reserve( block_order.size() );
        promoted_order.push_back( max_pos );
        for ( uint32_t i = 0u; i < block_order.size(); ++i )
        {
          if ( i != max_pos )
          {
            promoted_order.push_back( i );
          }
        }
        block_order = promoted_order;

        std::vector<uint32_t> promoted_partition;
        promoted_partition.reserve( input_partition_.size() );
        promoted_partition.push_back( input_partition_[max_pos] );
        for ( uint32_t i = 0u; i < input_partition_.size(); ++i )
        {
          if ( i != max_pos )
          {
            promoted_partition.push_back( input_partition_[i] );
          }
        }
        input_partition_ = promoted_partition;
      }
    }

    std::vector<uint32_t> original_offsets( block_order.size(), 0u );
    uint32_t prefix = 0u;
    auto const& original_partition = ps_.input_partition.empty() ? input_partition_ : ps_.input_partition;
    for ( uint32_t i = 0u; i < block_order.size(); ++i )
    {
      original_offsets[i] = prefix;
      prefix += original_partition[i];
    }

    for ( uint32_t new_block = 0u; new_block < block_order.size(); ++new_block )
    {
      const auto old_block = block_order[new_block];
      const auto old_begin = original_offsets[old_block];
      const auto block_size = original_partition[old_block];
      for ( uint32_t j = 0u; j < block_size; ++j )
      {
        const auto& n = pis[old_begin + j];
        node_to_wire_[n] = next_wire_++;
      }
    }
  }

  void emit_internal_gates()
  {
    topo_view topo{ ntk_ };
    topo.foreach_node( [&]( auto const& n ) {
      if ( ntk_.is_constant( n ) || ntk_.is_pi( n ) )
      {
        return;
      }

      std::vector<uint32_t> children;
      ntk_.foreach_fanin( n, [&]( auto const& f, auto ) {
        children.push_back( materialize_signal( f ) );
      } );

      const auto out = next_wire_++;

      if constexpr ( has_is_and_v<Ntk> )
      {
        if ( ntk_.is_and( n ) )
        {
          gates_.push_back( {children, out, "AND"} );
          node_to_wire_[n] = out;
          return;
        }
      }

      if constexpr ( has_is_xor_v<Ntk> )
      {
        if ( ntk_.is_xor( n ) )
        {
          gates_.push_back( {children, out, "XOR"} );
          node_to_wire_[n] = out;
          return;
        }
      }

      std::cerr << "[e] write_bristol currently only supports nodes recognized as AND/XOR\n";
      assert( false );
    } );
  }

  void emit_outputs()
  {
    ntk_.foreach_po( [&]( auto const& f ) {
      /*
       * MP-SPDZ's Bristol loader understands INV/AND/XOR, but not EQW.
       * Therefore, we materialize each PO as a fresh wire at the end using
       * only INV gates:
       *   - complemented PO: out = INV( base )
       *   - non-complemented PO: out = INV( INV( base ) )
       *
       * This guarantees that all designated output wires occupy the final
       * contiguous block of wire indices while avoiding EQW entirely.
       */
      const auto n = ntk_.get_node( f );
      assert( !ntk_.is_constant( n ) && "constant-driven POs are not supported" );

      const auto base = node_to_wire_[n];
      uint32_t out_wire = 0u;

      if ( ntk_.is_complemented( f ) )
      {
        out_wire = next_wire_++;
        gates_.push_back( {{base}, out_wire, "INV"} );
      }
      else
      {
        const auto tmp_wire = next_wire_++;
        gates_.push_back( {{base}, tmp_wire, "INV"} );
        out_wire = next_wire_++;
        gates_.push_back( {{tmp_wire}, out_wire, "INV"} );
      }

      output_wires_.push_back( out_wire );
    } );
  }

  uint32_t materialize_signal( signal const& f )
  {
    const auto n = ntk_.get_node( f );

    if ( ntk_.is_constant( n ) )
    {
      const bool complemented = ntk_.is_complemented( f );
      if ( ntk_.get_node( ntk_.get_constant( false ) ) == n && !complemented )
      {
        std::cerr << "[e] Bristol writer does not support constant-0 fanins directly\n";
        assert( false );
      }
      if ( ntk_.get_node( ntk_.get_constant( true ) ) == n && !complemented )
      {
        std::cerr << "[e] Bristol writer does not support constant-1 fanins directly\n";
        assert( false );
      }
      std::cerr << "[e] Bristol writer does not support constant-driven signals directly\n";
      assert( false );
    }

    const auto base = node_to_wire_[n];
    if ( !ntk_.is_complemented( f ) )
    {
      return base;
    }

    if ( !ps_.write_inverters_as_inv )
    {
      std::cerr << "[e] complemented signals require INV materialization in Bristol\n";
      assert( false );
    }

    auto it = inverted_wires_.find( base );
    if ( it != inverted_wires_.end() )
    {
      return it->second;
    }

    const auto inv = next_wire_++;
    gates_.push_back( {{base}, inv, "INV"} );
    inverted_wires_[base] = inv;
    return inv;
  }

  void write_header_and_body()
  {
    lorina::bristol_writer writer( os_ );
    writer.on_header( static_cast<uint32_t>( gates_.size() ), next_wire_, input_partition_, output_partition_ );
    for ( auto const& gate : gates_ )
    {
      writer.on_gate( gate.in, gate.out, gate.type );
    }
  }

private:
  Ntk const& ntk_;
  std::ostream& os_;
  write_bristol_params ps_;

  node_map<uint32_t, Ntk> node_to_wire_;
  std::unordered_map<uint32_t, uint32_t> inverted_wires_;
  std::vector<bristol_gate> gates_;
  std::vector<uint32_t> input_partition_;
  std::vector<uint32_t> output_partition_;
  std::vector<uint32_t> output_wires_;
  uint32_t next_wire_{ 0u };
};

} // namespace detail

template<class Ntk>
void write_bristol( Ntk const& ntk, std::ostream& os, write_bristol_params const& ps = {} )
{
  detail::bristol_writer_impl<Ntk>( ntk, os, ps ).run();
}

template<class Ntk>
void write_bristol( Ntk const& ntk, std::string const& filename, write_bristol_params const& ps = {} )
{
  std::ofstream os( filename, std::ofstream::out );
  write_bristol( ntk, os, ps );
  os.close();
}

} // namespace mockturtle
