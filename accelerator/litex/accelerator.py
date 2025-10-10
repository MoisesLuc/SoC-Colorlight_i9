from migen import *
from litex.gen import *
from litex.soc.interconnect.csr import *

class Accelerator(LiteXModule):
    def __init__(self, platform):
        platform.add_source("rtl/accelerator.sv")
        
        self.start_csr = CSRStorage(1, name="start")
        self.a0 = CSRStorage(32, name="a0"); 
        self.a1 = CSRStorage(32, name="a1"); 
        self.a2 = CSRStorage(32, name="a2"); 
        self.a3 = CSRStorage(32, name="a3"); 
        self.a4 = CSRStorage(32, name="a4"); 
        self.a5 = CSRStorage(32, name="a5"); 
        self.a6 = CSRStorage(32, name="a6"); 
        self.a7 = CSRStorage(32, name="a7");

        self.b0 = CSRStorage(32, name="b0"); 
        self.b1 = CSRStorage(32, name="b1"); 
        self.b2 = CSRStorage(32, name="b2"); 
        self.b3 = CSRStorage(32, name="b3"); 
        self.b4 = CSRStorage(32, name="b4"); 
        self.b5 = CSRStorage(32, name="b5"); 
        self.b6 = CSRStorage(32, name="b6"); 
        self.b7 = CSRStorage(32, name="b7"); 
        
        self.done_csr = CSRStatus(1, name="done", description="Pulso de finalização do cálculo")
        self.result_hi_csr = CSRStatus(32, name="result_hi", description="Parte alta (bits 63-32) do resultado.")
        self.result_lo_csr = CSRStatus(32, name="result_lo", description="Parte baixa (bits 31-0) do resultado.")
        
        # Sinais Internos
        start_sig  = Signal()
        done_sig   = Signal()
        a_sig     = [Signal(32, name=f"a{i}_sig") for i in range(8)]
        b_sig     = [Signal(32, name=f"b{i}_sig") for i in range(8)]
        result_sig = Signal(64)

        # Instanciação do Módulo SystemVerilog
        self.specials += Instance("accelerator",
            i_clk=ClockSignal(),
            i_rst=ResetSignal(),
            i_start=start_sig,
            i_a0=a_sig[0], i_a1=a_sig[1], i_a2=a_sig[2], i_a3=a_sig[3],
            i_a4=a_sig[4], i_a5=a_sig[5], i_a6=a_sig[6], i_a7=a_sig[7],
            i_b0=b_sig[0], i_b1=b_sig[1], i_b2=b_sig[2], i_b3=b_sig[3],
            i_b4=b_sig[4], i_b5=b_sig[5], i_b6=b_sig[6], i_b7=b_sig[7],
            o_done=done_sig,
            o_result=result_sig,
        )
        
        # Lógica Combinacional para Conectar CSRs aos Sinais
        self.comb += [
            start_sig.eq(self.start_csr.storage),

            a_sig[0].eq(self.a0.storage), a_sig[1].eq(self.a1.storage),
            a_sig[2].eq(self.a2.storage), a_sig[3].eq(self.a3.storage),
            a_sig[4].eq(self.a4.storage), a_sig[5].eq(self.a5.storage),
            a_sig[6].eq(self.a6.storage), a_sig[7].eq(self.a7.storage),
 
            b_sig[0].eq(self.b0.storage), b_sig[1].eq(self.b1.storage),
            b_sig[2].eq(self.b2.storage), b_sig[3].eq(self.b3.storage),
            b_sig[4].eq(self.b4.storage), b_sig[5].eq(self.b5.storage),
            b_sig[6].eq(self.b6.storage), b_sig[7].eq(self.b7.storage),
            
            self.done_csr.status.eq(done_sig),
            # Conecta as duas partes do resultado de 64 bits aos CSRs de 32 bits
            self.result_lo_csr.status.eq(result_sig[0:32]),
            self.result_hi_csr.status.eq(result_sig[32:64]),
        ]
