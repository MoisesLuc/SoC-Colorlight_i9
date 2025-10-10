module accelerator (
    input logic clk, rst,
    input logic signed [31:0] a0,
    input logic signed [31:0] a1,
    input logic signed [31:0] a2,
    input logic signed [31:0] a3,
    input logic signed [31:0] a4,
    input logic signed [31:0] a5,
    input logic signed [31:0] a6,
    input logic signed [31:0] a7,

    input logic signed [31:0] b0,
    input logic signed [31:0] b1,
    input logic signed [31:0] b2,
    input logic signed [31:0] b3,
    input logic signed [31:0] b4,
    input logic signed [31:0] b5,
    input logic signed [31:0] b6,
    input logic signed [31:0] b7,
    input logic start,

    output logic done,
    output logic signed [63:0] result
);

    // Variáveis internas
    // logic signed [63:0] temp;                 // Acumulador temporário
    logic signed [31:0] A_v [7:0];               // Vetor A interno
    logic signed [31:0] B_v [7:0];               // Vetor B interno

    assign A_v[0] = a0; assign B_v[0] = b0;
    assign A_v[1] = a1; assign B_v[1] = b1;
    assign A_v[2] = a2; assign B_v[2] = b2;
    assign A_v[3] = a3; assign B_v[3] = b3;
    assign A_v[4] = a4; assign B_v[4] = b4;
    assign A_v[5] = a5; assign B_v[5] = b5;
    assign A_v[6] = a6; assign B_v[6] = b6;
    assign A_v[7] = a7; assign B_v[7] = b7;

    logic [7:0]  index, index_next;                 // Index para o vetor;
    logic signed [63:0] acc, acc_next;              // Acumulador da soma
    logic signed [63:0] prod;                       // Produto de cada par

    logic done_reg, done_next;                      // Registrador para guardar sinal de done

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
            done_reg <= 0;
        end else begin
            state <= state_next;
            index <= index_next;
            acc   <= acc_next;
            done_reg <= done_next;
        end
    end



    assign prod = A_v[index] * B_v[index];

    always_comb begin
        state_next = state;
        index_next = index;
        acc_next   = acc;
        done_next  = done_reg;

        case (state) 
            IDLE: begin
                done_next  = 0;
                if (start) begin
                    state_next = CALC;
                    index_next = 0;
                    acc_next   = 0;  
                end
            end

            CALC: begin
                acc_next   = acc + prod;
                index_next = index + 1'b1;

                if (index == 4'd7) begin
                    state_next = DONE;
                    done_next = 1;
                end
            end

            DONE: begin
                if (!start)      
                    state_next = IDLE;      // Retorna ao IDLE apenas se start for desativado
            end

            default: begin
                state_next = IDLE;
            end
        endcase
    end

    assign done = done_reg;
    assign result = acc;

endmodule