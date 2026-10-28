// Missing comma between y and a on line 5.
module bad_syntax(a,y);
    input a;
    output y;
    buf g1(y a);
endmodule
