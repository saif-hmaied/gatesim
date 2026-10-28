// y is driven by two gates: a short circuit.
module bad_multidriver(a,b,y);
    input a,b;
    output y;
    and g1(y,a,b);
    or g2(y,a,b);
endmodule
