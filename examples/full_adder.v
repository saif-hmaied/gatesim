module full_adder(a,b,cin,cout,sum);
    input cin,a,b;
    output cout,sum;
    wire w1,w2,w3;
    xor g1(w1,a,b);
    xor g2(sum,cin,w1);
    and g3(w2,a,b);
    and g4(w3,w1,cin);
    or g5(cout,w3,w2);
endmodule
