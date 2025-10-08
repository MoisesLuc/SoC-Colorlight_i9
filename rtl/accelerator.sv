module accelerator (
    input logic clk, rst,
    input logic signed [31:0] a [8],
    input logic signed [31:0] b [8],
    input logic start,

    output logic done,
    output logic signed [63:0] result
);

    // Variáveis internas
    // logic signed [63:0] temp;                    // Acumulador temporário
    // logic signed [31:0] A_v [7:0];               // Vetor A interno
    // logic signed [31:0] B_v [7:0];               // Vetor B interno

    logic [7:0]  index, index_next;                  // Index para o vetor;
    logic signed [63:0] acc, acc_next;              // Acumulador da soma
    logic signed [63:0] prod;                       // Produto de cada par

    // Sinais para a máquina de estado
    typedef enum logic [1:0] {
        IDLE, 
        CALC, 
        DONE
    } state_type;
    state_type state, state_next;


    always_ff @(posedge clk, posedge rst) begin
        if (rst) begin
            state <= IDLE;
            index <= 0;
            acc   <= 0;
        end else begin
            state <= state_next;
            index <= index_next;
            acc   <= acc_next;
        end
    end

    assign prod = a[index] * b[index];

    always_comb begin
        state_next = state;
        index_next = index;
        acc_next   = acc;
        done       = 1'b0;

        case (state) 
            IDLE: begin
                if (start) begin
                    state_next = CALC;
                    index_next = 1'b0;
                    acc_next   = 1'b0;
                end
            end

            CALC: begin
                acc_next   = acc + prod;
                index_next = index + 1'b1;

                if (index == 4'd7)
                    state_next = DONE;
            end

            DONE: begin
                done = 1'b1;
                if (!start)      
                    state_next = IDLE;      // Retorna ao IDLE apenas se start for desativado
            end

            default: begin
                state_next = IDLE;
            end
        endcase
    end

    assign result = acc;

endmodule