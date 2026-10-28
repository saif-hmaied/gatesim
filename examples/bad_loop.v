// g1 needs w2, and g2 needs w1: a combinational loop.
module bad_loop(a,y);
    input a;
    output y;
    wire w1,w2;
    and g1(w1,a,w2);
    not g2(w2,w1);
    buf g3(y,w1);
endmodule
