//
// spi_loader_san.v
//
// Receptor SPI e decodificador dedicado de canal SDC (Target 3)
// para injeção direta de ROM na RAM da FPGA sem uso de SD Card.
//

module spi_loader_san (
    input             clk,
    input             rstn,
    
    // Interface SPI (conectada ao mcu_spi)
    input             data_strobe,
    input             data_start,
    input [7:0]       data_in,
    output reg [7:0]  data_out,
    
    input             spi_ss, // Pino CS_FPGA do SPI (GP17)
    
    // Informações da imagem (ROM)
    output reg [31:0] image_size,
    output reg        image_mounted,
    
    // Interface IOCTL para escrita direta na RAM
    output reg        ioctl_download,
    output reg [22:0] ioctl_addr,
    output reg [7:0]  ioctl_data,
    output reg        ioctl_wr
);

reg [7:0] command;
reg [3:0] byte_cnt;

always @(posedge clk) begin
    // Limpa o pulso de escrita de dados a cada ciclo por padrão
    ioctl_wr <= 1'b0;
    
    // Incrementa o endereço no ciclo seguinte ao pulso de escrita
    if (ioctl_wr) begin
        ioctl_addr <= ioctl_addr + 1'd1;
    end
    
    // Se o pino CS estiver inativo (High) ou reset geral ativo, reseta o estado de download
    if (!rstn || spi_ss) begin
        ioctl_download <= 1'b0;
        command <= 8'hFF;
        byte_cnt <= 4'd15;
    end else if (data_strobe) begin
        if (data_start) begin
            command <= data_in;
            byte_cnt <= 4'd0;
            data_out <= 8'h00; // Resposta genérica padrão
            
            // CMD 8: Inicia download de ROM e reseta ponteiro de endereço para 0
            if (data_in == 8'd8) begin
                ioctl_download <= 1'b1;
                ioctl_addr <= 23'd0;
            end
        end else begin
            // CMD 4: INSERTED (Receber tamanho da imagem de 4 bytes)
            if (command == 8'd4) begin
                if (byte_cnt == 4'd0) begin
                    // Byte 0: Drive ID (normalmente 0). Ignoramos.
                end
                if (byte_cnt == 4'd1) image_size[31:24] <= data_in;
                if (byte_cnt == 4'd2) image_size[23:16] <= data_in;
                if (byte_cnt == 4'd3) image_size[15:8]  <= data_in;
                if (byte_cnt == 4'd4) begin
                    image_size[7:0] <= data_in;
                    image_mounted <= 1'b1; // Pulsa indicando montagem
                end
            end
            
            // CMD 8: ROM STREAM (Bytes sequenciais do arquivo ROM)
            if (command == 8'd8) begin
                ioctl_data <= data_in;
                ioctl_wr <= 1'b1;
            end
            
            if (byte_cnt != 4'd15) byte_cnt <= byte_cnt + 4'd1;
        end
    end else begin
        // Limpa o pulso de montagem no próximo ciclo de clock
        image_mounted <= 1'b0;
    end
end

endmodule
