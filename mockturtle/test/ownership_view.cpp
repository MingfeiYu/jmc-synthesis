#include <catch.hpp>

#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/ownership_view.hpp>

using namespace mockturtle;

TEST_CASE( "Ownership of nodes in a two-party XAG", "[ownership_view]" )
{
  xag_network xag;
  const auto a0 = xag.create_pi();
  const auto a1 = xag.create_pi();
  const auto b0 = xag.create_pi();
  const auto b1 = xag.create_pi();

  const auto f_a = xag.create_and( a0, a1 );
  const auto f_b = xag.create_xor( b0, b1 );
  const auto f_ab = xag.create_and( f_a, f_b );
  xag.create_po( xag.create_xor( f_ab, a0 ) );

  /* by default, the first half of the PIs belongs to the first party */
  owner_view<xag_network> ntk{ xag, 2u };

  CHECK( ntk.get_ownership( a0 ) == 1u );
  CHECK( ntk.get_ownership( a1 ) == 1u );
  CHECK( ntk.get_ownership( b0 ) == 2u );
  CHECK( ntk.get_ownership( b1 ) == 2u );

  /* locally derivable nodes report their party, jointly derived ones 0 */
  CHECK( ntk.is_local( ntk.get_node( f_a ) ) == 1u );
  CHECK( ntk.is_local( ntk.get_node( f_b ) ) == 2u );
  CHECK( ntk.is_local( ntk.get_node( f_ab ) ) == 0u );
  CHECK( ntk.get_ownership( f_ab ) == 3u );
}

TEST_CASE( "User-specified PI ownership", "[ownership_view]" )
{
  xag_network xag;
  const auto x0 = xag.create_pi();
  const auto x1 = xag.create_pi();
  const auto x2 = xag.create_pi();
  const auto x3 = xag.create_pi();

  const auto f0 = xag.create_and( x0, x2 );
  const auto f1 = xag.create_and( x2, x3 );
  xag.create_po( xag.create_xor( f0, f1 ) );

  SECTION( "number of PIs per party" )
  {
    /* x0, x1, x2 belong to the first party, x3 to the second */
    owner_view<xag_network> ntk{ xag, 2u, { 3u, 1u } };
    CHECK( ntk.is_local( ntk.get_node( f0 ) ) == 1u );
    CHECK( ntk.is_local( ntk.get_node( f1 ) ) == 0u );
  }

  SECTION( "ownership per PI" )
  {
    owner_view<xag_network> ntk{ xag, 2u, { 1u, 2u, 1u, 2u }, true };
    CHECK( ntk.is_local( ntk.get_node( f0 ) ) == 1u );
    CHECK( ntk.is_local( ntk.get_node( f1 ) ) == 0u );
  }
}

TEST_CASE( "Ownership of nodes added to the view", "[ownership_view]" )
{
  xag_network xag;
  const auto a0 = xag.create_pi();
  const auto a1 = xag.create_pi();
  const auto b0 = xag.create_pi();
  const auto b1 = xag.create_pi();
  xag.create_po( xag.create_xor( a0, b0 ) );

  owner_view<xag_network> ntk{ xag, 2u };

  const auto g_a = ntk.create_and( a0, a1 );
  const auto g_b = ntk.create_and( b0, b1 );
  const auto g_ab = ntk.create_and( g_a, g_b );

  CHECK( ntk.is_local( ntk.get_node( g_a ) ) == 1u );
  CHECK( ntk.is_local( ntk.get_node( g_b ) ) == 2u );
  CHECK( ntk.is_local( ntk.get_node( g_ab ) ) == 0u );
}
