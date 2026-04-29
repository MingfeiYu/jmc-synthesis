// Benchmark "/tmp/tmp" written by ABC on Sun Apr 19 19:39:09 2026

module \/tmp/tmp  ( 
    n1, n2, n3, n4, n5, n6, n7, n8, n9, n10,
    po0, po1, po2, po3  );
  input  n1, n2, n3, n4, n5, n6, n7, n8, n9, n10;
  output po0, po1, po2, po3;
  wire new_n11_, new_n12_, new_n13_, new_n14_, new_n15_, new_n16_;
  assign new_n11_ = n1 & n7;
  assign new_n12_ = n2 & n8;
  assign new_n13_ = n3 & n9;
  assign new_n14_ = n4 & n10;
  assign new_n15_ = new_n13_ & new_n14_;
  assign new_n16_ = n5 & n6;
  assign po0 = ~new_n11_;
  assign po1 = ~new_n12_;
  assign po2 = ~new_n15_;
  assign po3 = ~new_n16_;
endmodule


