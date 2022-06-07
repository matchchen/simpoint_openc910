/* ============================================================================
  A Verilog main() program that calls a local serial port co-simulator.
  =============================================================================*/
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include <verilated.h>
#include "verilated_vcd_c.h"
#include "Vtop.h"
#include "Vtop__Dpi.h"   //auto created by the verilator from the rtl that support dpi


/*
  =========================================================================
    // DPI EXPORTS
    // DPI export at /var/cpu_testbench/t_head/simpoint_openc910/sim_verilator/rtl/../../openc910/smart_run/logical/mem/f_spsram_large.v:404:10
    extern void simutil_load_data(const char* file);
    // DPI export at /var/cpu_testbench/t_head/simpoint_openc910/sim_verilator/rtl/../../openc910/smart_run/logical/mem/f_spsram_large.v:372:10
    extern void simutil_load_inst(const char* file);
    // DPI export at /var/cpu_testbench/t_head/simpoint_openc910/sim_verilator/rtl/../../openc910/smart_run/logical/mem/f_spsram_large.v:309:10
    extern void simutil_load_inst_data(const char* file1, const char* file2);
    // DPI export at /var/cpu_testbench/t_head/simpoint_openc910/sim_verilator/rtl/../../openc910/smart_run/logical/mem/f_spsram_large.v:524:18
    extern int simutil_read_memory(int mem_index, int addr_index, char* val);
    // DPI export at /var/cpu_testbench/t_head/simpoint_openc910/sim_verilator/rtl/../../openc910/smart_run/logical/mem/f_spsram_large.v:440:18
    extern int simutil_write_memory(int mem_index, int addr_index, char val);

  ============================================================================
*/

int main(int argc,  char ** argv)
{
    printf("Built with %s %s.\n", Verilated::productName(),
    Verilated::productVersion());
    printf("Recommended: Verilator 4.0 or later.\n");

    if(argc < 2 || argv[1] == "?")
    {
        printf("usage: %s --vmem vmem_file \r\n", argv[0]);
        printf("\tExample: %s --vmem hello_world.vmem \r\n", argv[0]);
        printf("usage: %s --bin bin_file \r\n", argv[0]);
        printf("\tExample: %s --bin hello_world.bin \r\n", argv[0]);
        printf("usage: %s --segment inst_mem_part data_mem_part  [Assume: data segement started from 0x00040000]\r\n", argv[0]);
        printf("\tExample: %s --segment inst.pat data.pat \r\n", argv[0]);
        return -1;
    }

    unsigned char ucLoadType = 0;
    long instrNum = 0;
    unsigned long debugLogIntv = 0; //ticks
    if(strncasecmp(argv[1],"--bin",strlen("--bin")) == 0)
    {
        ucLoadType = 1;  //filename = argv[2]
    }
    else if(strncasecmp(argv[1],"--vmem",strlen("--vmem")) == 0)
    {
        ucLoadType = 2;  //filename = argv[2]
    }
    else if(strncasecmp(argv[1],"--segment",strlen("--segment")) == 0)
    {
        ucLoadType = 3; //filename1 = argv[2]   filename2 = argv[3]
    }
    else
    {
        printf("usage: %s --vmem vmem_file \r\n", argv[0]);
        printf("usage: %s --bin bin_file \r\n", argv[0]);
        printf("usage: %s --segment inst_mem_part data_mem_part \r\n", argv[0]);
        return -1;
    }

    if((ucLoadType < 3 && argc > 3) || (ucLoadType == 3 && argc > 4)) // run instrution number
    {
        for (int i = (ucLoadType/3 + 3); i < argc; i++)
        {
            if (strncasecmp(argv[i],"--debug-intv",strlen("--debug-intv")) == 0)
            {
                debugLogIntv = atoi(argv[i+1]);
                printf("Set debug log interval:%ld ticks\r\n",debugLogIntv);
                i++;
            }
            else if (strncasecmp(argv[i],"--i",strlen("--i")) == 0)
            {
                instrNum = atoi(argv[i+1]);
                printf("Set instructions: %s;%d \r\n",argv[i+1],instrNum);
                i++;
            }
            else if ((strncasecmp(argv[i],"+vcd",strlen("+vcd")) == 0) || (strncasecmp(argv[i],"+trace",strlen("+trace")) == 0))
            {
            }
            else
            {
                printf("usage: %s --bin bin_file --i <n> --debug-intv <t> +vcd +trace \r\n", argv[0]);
                printf("\tExample: %s --bin hello_world.bin --i 1000 --debug-intv 1000 +vcd +trace \r\n", argv[0]);
                return -1;
            }
        }
    }
    // call commandArgs first!
    VerilatedContext* contextp = new VerilatedContext;
    Verilated::commandArgs(argc, argv);

    // Set debug level, 0 is off, 9 is highest presently used
    Verilated::debug(0);
    // Randomization reset policy
    Verilated::randReset(2);
    Verilated::mkdir("./log");

    // Instantiate our design
    Vtop * ptTB = new Vtop;

    // Tracing (vcd)
    VerilatedVcdC * m_trace = NULL;
    const char* flag_vcd = Verilated::commandArgsPlusMatch("vcd");
    if (flag_vcd && 0==strcmp(flag_vcd, "+vcd"))
    {
        Verilated::traceEverOn(true); // Verilator must compute traced signals
        m_trace = new VerilatedVcdC;
        ptTB->trace(m_trace, 1); // Trace 99 levels of hierarchy
        m_trace->open("./log/tb.vcd");
    }

    FILE * debug_fd = NULL;
    if (debugLogIntv > 0)
    {
        debug_fd = fopen("./log/tb.debug", "w");
    }

    int m_cpu_tickcount = 0;

    // Note that if the DPI task or function accesses any register or net within the RTL,
    // it will require a scope to be set. This can be done using the standard functions within svdpi.h,
    // after the module is instantiated, but before the task(s) and/or function(s) are called.
    // For example, if the top level module is instantiated with the name “dut”
    // and the name references within tasks are all hierarchical (dotted) names with respect to that top level module,
    // then the scope could be set with
    // svSetScope(svGetScopeFromName("TOP.dut"));

    int recordStatus = 0; //0:not start 1:running 2:ending
    unsigned long cycleCnt = 0;//simutil_read_mcycle();
    unsigned long instrCnt = 0;//simutil_read_minstret();
    time_t now = time(0); //system time , seconds
    unsigned long mepcValue = 0;//simutil_read_mepc()
    unsigned long mcauseValue = 0;//simutil_read_mcause()
    unsigned long curPC0 = 0;//simutil_read_rtu_pad_retire0_pc()
    unsigned long curPC1 = 0;//simutil_read_rtu_pad_retire1_pc()
    unsigned long curPC2 = 0;//simutil_read_rtu_pad_retire2_pc()

    //it is time to load the memory
    // tb.x_soc.x_axi_slave128.x_f_spsram_large
    svSetScope(svGetScopeFromName("TOP.top.x_soc.x_axi_slave128.x_f_spsram_large"));

    switch (ucLoadType)
    {
        case 1:  //load bin, argv2
        {
            unsigned int  mem_index=0, addr_index=0;
            unsigned char buffer[16] = {0};
            size_t iread_bytes = 0;
            // open the bin file to read
            // you can read the bin file in liuux with: xxd ./work/hello_world.bin
            FILE * fp;
            fp = fopen(argv[2], "rb");  // r for read, b for binary
            if(fp == NULL)
            {
                printf("open file (%s) failed, please check it \r\n", argv[2]);
                exit(-1);
            }

            printf("\t********* Wipe memory to 0 *********\r\n");
            for (long long i=0;i<0x4000000;i++) //64MB*16
            {
                for(mem_index=0; mem_index < 16; mem_index++)
                {
                    simutil_write_memory(mem_index, i, 0);
                }
            }

            printf("start to load file (%s) into memory \r\n", argv[2]);
            fseek(fp, 0, SEEK_SET);
            iread_bytes = fread(buffer, sizeof(unsigned char), 16, fp); // read 16 bytes to our buffer
            // write the byte to the memory
            while(iread_bytes == 16)
            {
                for(mem_index=0; mem_index < iread_bytes; mem_index++)
                {
                    simutil_write_memory(mem_index, addr_index, buffer[mem_index]);
                }
                addr_index ++;
                iread_bytes = fread(buffer, sizeof(unsigned char), 16, fp); // read another 16 bytes to our buffer
            }
            printf("loaded bytes (0x%llx), leave bytes(%d)\r\n",addr_index*16,iread_bytes);
            printf("loaded file (%s) into memory successfully\r\n", argv[2]);
        }
        break;

        case 2:  //load vmem, argv2
        {
            simutil_load_inst(argv[2]);
        }
        break;

        case 3:  //load segment, inst, argv2 & argv3
        {
            simutil_load_inst_data(argv[2], argv[3]);
        }
        break;

        default:
        {
            return -1;
        }
        break;
    }

    ptTB->clk = 0;
    int reseting = 1;

    while(!contextp->gotFinish())
    {
        if (reseting)
        {
            for (int i=1;i<43;i++) //wait 43 clock to reset
            {
                ptTB->clk = !ptTB->clk;
                ptTB->eval();
            }
            reseting = 0;
        }
//        contextp->timeInc(1);  // 1 timeprecision period passes...


        // Toggle a fast (time/2 period) clock
        ptTB->clk = !ptTB->clk;
        ptTB->eval();
        if(m_trace)
        {
	        m_trace->dump(m_cpu_tickcount*2);   //  Tick every 
	    }

        if (instrNum > 0){
            svSetScope(svGetScopeFromName("TOP.top.x_soc.x_cpu_sub_system_axi.x_rv_integration_platform.x_cpu_top.x_ct_top_0.x_ct_hpcp_top"));

            if (recordStatus == 0){
                instrCnt = simutil_read_minstret();
                if (instrCnt > 0){ //sample from some instructions retired
                    cycleCnt = simutil_read_mcycle();
                    now = time(0);
                    printf("Start: cycleCnt=%ld,instrCnt=%ld,now=%ld\r\n",cycleCnt,instrCnt,now);
                    recordStatus = 1;
                }
            }else if(recordStatus == 1){    
                if (m_cpu_tickcount % 1000 == 0){ //sample per 1000 tick
                    instrCnt = simutil_read_minstret();
                    if (instrCnt >= instrNum){
                        cycleCnt = simutil_read_mcycle();
                        now = time(0);
                        printf("\r\nEnd: cycleCnt=%ld,instrCnt=%ld,now=%ld\r\n",cycleCnt,instrCnt,now);
                        recordStatus = 2;
                    }                
                }
            }
        }

        if (debugLogIntv > 0)
        {
            if (m_cpu_tickcount % debugLogIntv == 0)
            {
                svSetScope(svGetScopeFromName("TOP.top.x_soc.x_cpu_sub_system_axi.x_rv_integration_platform.x_cpu_top.x_ct_top_0.x_ct_core.x_ct_cp0_top.x_ct_cp0_regs"));
                mepcValue = simutil_read_mepc();
                mcauseValue = simutil_read_mcause();
                svSetScope(svGetScopeFromName("TOP.top.x_soc.x_cpu_sub_system_axi.x_rv_integration_platform.x_cpu_top.x_ct_top_0.x_ct_core"));
                curPC0 = simutil_read_rtu_pad_retire0_pc();
                curPC1 = simutil_read_rtu_pad_retire1_pc();
                curPC2 = simutil_read_rtu_pad_retire2_pc();
                now = time(0);
                //printf("now=%ld,tickcount=%ld,mepc=0x%llx,mcause=0x%llx,retire0_pc=0x%llx,retire1_pc=0x%llx,retire2_pc=0x%llx\r\n", now,m_cpu_tickcount,mepcValue,mcauseValue,curPC0,curPC1,curPC2);
                fprintf(debug_fd, "now=%ld,tickcount=%ld,mepc=0x%llx,mcause=0x%llx,retire0_pc=0x%llx,retire1_pc=0x%llx,retire2_pc=0x%llx\r\n", now,m_cpu_tickcount,mepcValue,mcauseValue,curPC0,curPC1,curPC2);
            }
        }

        // Toggle a fast (time/2 period) clock
        ptTB->clk = !ptTB->clk;
        ptTB->eval();
        if(m_trace)
        {
            m_trace->dump(m_cpu_tickcount*2+1);   // Trailing edge dump
            m_trace->flush();
        }

        m_cpu_tickcount++;
    }

    if (debugLogIntv > 0)
    {
       fclose(debug_fd);
    }

    if(m_trace)
    {
        m_trace->flush();
        m_trace->close();
    }


#if VM_COVERAGE
    VerilatedCov::write("log/coverage.dat");
#endif // VM_COVERAGE

    delete ptTB;

    exit(0);
}


