// w is read by g1, but no gate drives it.
module bad_undriven(a,y);
    input a;
    output y;
    wire w;
    and g1(y,a,w);
endmodule
