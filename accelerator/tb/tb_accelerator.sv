`timescale 1ns / 1ps // Define a unidade de tempo para a simulação

module tb_accelerator;
    localparam CLK_PERIOD = 10; // Período do clock: 10 ns (100 MHz)
    logic clk, rst;
    logic done, start;

    logic signed [31:0] a [8];
    logic signed [31:0] b [8]; 
    logic signed [63:0] result;

    // Variável para armazenar o resultado esperado, calculado no testbench
    logic signed [63:0] expected_result;

    accelerator uut (
        .clk(clk),
        .rst(rst), 
        .start(start),
        .a0(a[0]), .a1(a[1]), .a2(a[2]), .a3(a[3]),
        .a4(a[4]), .a5(a[5]), .a6(a[6]), .a7(a[7]),
        .b0(b[0]), .b1(b[1]), .b2(b[2]), .b3(b[3]),
        .b4(b[4]), .b5(b[5]), .b6(b[6]), .b7(b[7]),   
        .done(done),
        .result(result)
    );

    always begin
        clk = 1'b0;
        #10;
        clk = 1'b1;
        #10;
    end


    initial begin
        $display("--- Iniciando simulacao do Testbench para accelerator ---");

        rst   = 1'b0;
        start = 1'b0;
        for (int i = 0; i < 8; i++) begin
            a[i] = 0;
            b[i] = 0;
        end
        repeat (2) @(posedge clk); 

        rst = 1'b1;
        @(posedge clk);
        rst = 1'b0;
        @(posedge clk);

        // ---------------------------------------------------------------
        // TESTE 1: Valores positivos
        // ---------------------------------------------------------------
        $display("--- Iniciando Teste 1: Valores Positivos ---",);
        a[0]=1; a[1]=2; a[2]=3; a[3]=4; a[4]=5; a[5]=6; a[6]=7; a[7]=8;
        b[0]=10; b[1]=10; b[2]=10; b[3]=10; b[4]=1; b[5]=1; b[6]=1; b[7]=1;
        // Resultado esperado: 126
        
        run_and_check_test();

        // ---------------------------------------------------------------
        // TESTE 2: Valores negativos e positivos (mistos)
        // ---------------------------------------------------------------
        $display("--- Iniciando Teste 2: Valores Mistos ---",);
        a[0]=10; a[1]=-5; a[2]=100; a[3]=-1; a[4]=0; a[5]=20; a[6]=-2; a[7]=1;
        b[0]=2; b[1]=10; b[2]=-1; b[3]=20; b[4]=50; b[5]=-5; b[6]=4; b[7]=-8;
        // Resultado esperado: -266

        run_and_check_test();

        // ---------------------------------------------------------------
        // TESTE 3: Um vetor nulo
        // ---------------------------------------------------------------
        $display("--- Iniciando Teste 3: Vetor Nulo ---",);
        a[0]=15; a[1]=25; a[2]=35; a[3]=45; a[4]=55; a[5]=65; a[6]=75; a[7]=85;
        b[0]=0; b[1]=0; b[2]=0; b[3]=0; b[4]=0; b[5]=0; b[6]=0; b[7]=0;
        // Resultado esperado: 0
        
        run_and_check_test();

        // ---------------------------------------------------------------
        // Fim dos testes
        // ---------------------------------------------------------------
        $display("--- Todos os testes foram concluidos. ---",);
        $finish; 
    end

    task run_and_check_test();
        expected_result = 0;
        for (int i = 0; i < 8; i++) begin
            expected_result += a[i] * b[i];
        end
        $display("Resultado esperado = %d", expected_result);

        // Lógica de estímulo e verificação (sem alterações aqui)
        @(posedge clk);
        start = 1'b1;
        @(posedge clk);
        start = 1'b0;

        while (done == 1'b0) begin
            @(posedge clk);
        end

        @(posedge clk); 

        if (result == expected_result) begin
            $display(">> SUCESSO: Resultado (%d) corresponde ao esperado (%d). DONE = %b\n", result, expected_result, done);
        end else begin
            $display(">> FALHA: Resultado (%d) NAO corresponde ao esperado (%d). DONE = %b\n", result, expected_result, done);
        end
    endtask
endmodule