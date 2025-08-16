// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and eyalabraham
// (Dragon 32 emu core) are thanked profusely.
//
// The Draco-DS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================


/********************************************************************
 * cpu.c
 *
 *  MC6809E CPU emulation module.
 *
 *  Resource MC6809E data sheet Motorola INC. 1984 DS9846-R2:
 *      Figure  4 - Programming model
 *      Figure 14 - Instruction flow chart
 *      Figure 17 - Cycle by cycle performance
 *
 *  Motorola 6809 and Hitachi 6309 Programmer's Reference:
 *  https://colorcomputerarchive.com/repo/Documents/Books/Motorola%206809%20and%20Hitachi%206309%20Programming%20Reference%20(Darren%20Atkinson).pdf
 *
 *  July 4, 2020
 *
 *******************************************************************/
#include    <nds.h>
#include    <string.h>

#include    "mc6809e.h"
#include    "mem.h"
#include    "cpu.h"

extern unsigned int debug[];

#define CPU_CYCLES_PER_LINE             57
#define CPU_CYCLES_PER_LINE_OVERCLOCK   (CPU_CYCLES_PER_LINE * 2)

/* -----------------------------------------
   Local definitions
----------------------------------------- */

/* MC6809E vector addresses, where
 * each vector is two bytes long.
 */
#define     VEC_RESET               0xfffe
#define     VEC_NMI                 0xfffc
#define     VEC_SWI                 0xfffa
#define     VEC_IRQ                 0xfff8
#define     VEC_FIRQ                0xfff6
#define     VEC_SWI2                0xfff4
#define     VEC_SWI3                0xfff2
#define     VEC_RESERVED            0xfff0

/* Indexed addressing post-byte bit fields
 */
#define     INDX_POST_5BIT_OFF      0x80
#define     INDX_POST_REG           0x60
#define     INDX_POST_INDIRECT      0x10
#define     INDX_POST_MODE          0x0f

/* Condition-code register bit
 */
#define     CC_FLAG_CLR             0
#define     CC_FLAG_SET             1

/* Word and Byte operations
 */
#define     GET_REG_HIGH(r)         ((uint8_t)(r >> 8))
#define     GET_REG_LOW(r)          ((uint8_t)r)
#define     SIG_EXTEND(b)           ((((uint8_t)b) & 0x80) ? (((uint16_t)b) | 0xff00):((uint16_t)b))

/* -----------------------------------------
   Module functions
----------------------------------------- */

/* CPU op-code processing.
 * These functions manipulate global variable,
 * CPU registers and CC flags.
 */
uint8_t adc(uint8_t acc, uint8_t byte);
uint8_t add(uint8_t acc, uint8_t byte);
void    addd(uint16_t word);
uint8_t and(uint8_t acc, uint8_t byte);
void    andcc(uint8_t byte);
uint8_t asl(uint8_t byte);
uint8_t asr(uint8_t byte);
void    bit(uint8_t acc, uint8_t byte);
uint8_t clr(void);
void    cmp(uint8_t arg, uint8_t byte);
void    cmp16(uint16_t arg, uint16_t word);
uint8_t com(uint8_t byte);
void    cwai(uint8_t byte);
void    daa(void);
uint8_t dec(uint8_t byte);
uint8_t eor(uint8_t acc, uint8_t byte);
void    exg(uint8_t regs);
uint8_t inc(uint8_t byte);
uint8_t lsr(uint8_t byte);
uint8_t neg(uint8_t byte);
uint8_t or(uint8_t acc, uint8_t byte);
void    orcc(uint8_t byte);
void    pshs(uint8_t push_list, int *cycles);
void    pshu(uint8_t push_list, int *cycles);
void    puls(uint8_t pull_list, int *cycles);
void    pulu(uint8_t pull_list, int *cycles);
uint8_t rol(uint8_t byte);
uint8_t ror(uint8_t byte);
void    rti(int *cycles);
uint8_t sbc(uint8_t acc, uint8_t byte);
void    sex(void);
uint8_t sub(uint8_t acc, uint8_t byte);
void    subd(uint16_t word);
void    swi(int swi_id);
void    tfr(uint8_t regs);
void    tst(uint8_t byte);

/* CPU op-code support functions
 */
void     branch(int instruction, int long_short, uint16_t effective_address);
int      get_eff_addr(int op_code);
uint16_t read_register(int reg);
void     write_register(int reg, uint16_t data);

/* Condition code register CC functions
 */
void    eval_cc_c(uint16_t value);
void    eval_cc_c16(uint32_t value);
void    eval_cc_z(uint16_t value);
void    eval_cc_z16(uint32_t value);
void    eval_cc_n(uint16_t value);
void    eval_cc_n16(uint32_t value);
void    eval_cc_v(uint8_t val1, uint8_t val2, uint16_t result);
void    eval_cc_v16(uint16_t val1, uint16_t val2, uint32_t result);
void    eval_cc_h(uint8_t val1, uint8_t val2, uint8_t result);

uint8_t get_cc(void);
void    set_cc(uint8_t value);


/* -----------------------------------------
   Module globals
----------------------------------------- */

/* MC6809E register file
 */
cpu_state_t cpu  __attribute__((section(".dtcm")));

struct cc_t
{
    int c;
    int v;
    int z;
    int n;
    int i;
    int h;
    int f;
    int e;
} cc __attribute__((section(".dtcm")));

int cycles_this_scanline    __attribute__((section(".dtcm"))) = 0;

#define     d       ((uint16_t)(((uint16_t)cpu.a << 8) + cpu.b))    // Accumulator D

/*------------------------------------------------
 * cpu_init()
 *
 *  Initialize the CPU for command execution at address.
 *  Function should be called once before cpu_run() or cpu_single_step().
 *
 *  param:  Start address
 *  return: 0- initialization ok, 1- Start address error
 */
int cpu_init(int address)
{
    /* Registers
     */
    cpu.x  = 0;
    cpu.y  = 0;
    cpu.u  = 0;
    cpu.s  = 0;
    cpu.pc = 0;
    cpu.a  = 0;
    cpu.b  = 0;

    cpu.dp = 0;
    set_cc(0);

    /* CPU state
     */
    cpu.nmi_armed       = 0;
    cpu.nmi_latched     = 0;
    cpu.halt_asserted   = 0;
    cpu.reset_asserted  = 0;
    cpu.irq_asserted    = 0;
    cpu.firq_asserted   = 0;
    cpu.int_latch       = 0;
    cpu.cpu_state       = CPU_HALTED;

    // And set the PC to where we want to start
    cpu.pc = address;

    return 0;
}

/*------------------------------------------------
 * cpu_halt()
 *
 *  Assert HALT state
 *
 *  param:  0- clear, 1- asserted
 *  return: Nothing
 */
void cpu_halt(int state)
{
    cpu.halt_asserted = state;
}

/*------------------------------------------------
 * cpu_reset()
 *
 *  Assert RESET state
 *
 *  param:  0- clear, 1- asserted
 *  return: Nothing
 */
void cpu_reset(int state)
{
    cpu.reset_asserted = state;
}

/*------------------------------------------------
 * cpu_nmi()
 *
 *  Trigger a Non Mask-able Interrupt (NMI) state
 *
 *  param:  0- clear, 1- asserted
 *  return: Nothing
 */
void cpu_nmi_trigger(void)
{
    cpu.nmi_latched = INT_NMI;
}

/*------------------------------------------------
 * cpu_firq()
 *
 *  Assert Fast IRQ (FIRQ) state
 *
 *  param:  0- clear, 1- asserted
 *  return: Nothing
 */
void cpu_firq(int state)
{
    cpu.firq_asserted = state;
}

/*------------------------------------------------
 * cpu_irq()
 *
 *  Assert IRQ state
 *
 *  param:  0- clear, 1- asserted
 *  return: Nothing
 */
void cpu_irq(int state)
{
    cpu.irq_asserted = state;
}

void cpu_check_reset(void)
{
    if ( cpu.reset_asserted )
    {
        cycles_this_scanline = 0;
        cc.f = CC_FLAG_SET;
        cc.i = CC_FLAG_SET;
        cpu.dp = 0;
        cpu.nmi_armed = 0;
        cpu.nmi_latched = 0;
        cpu.cpu_state = CPU_RESET;
        cpu.pc = (mem_read(VEC_RESET) << 8) + mem_read(VEC_RESET+1);
        cpu.reset_asserted = 0;
        cpu.cpu_state = CPU_EXEC;
    }
}

/*------------------------------------------------
 * cpu_run()
 *
 *  Start CPU.
 *  Function should be called periodically
 *  after an initialization by cpu_run_init().
 *
 *  param:  Nothing
 *  return: Nothing
 */
ITCM_CODE void cpu_run(void)
{
    int         eff_addr;
    uint8_t     operand8;
    uint16_t    operand16;
    int         op_code;

    int cycles_per_line = (sam_registers.mpu_rate) ? CPU_CYCLES_PER_LINE_OVERCLOCK : CPU_CYCLES_PER_LINE;

    while (1)
    {
        /* Latch interrupt requests - these can change between opcode processing so must be re-latched every CPU pass
         */
        int intr_latch = cpu.irq_asserted | cpu.firq_asserted | cpu.nmi_latched; // Latch all possible IRQs that might happen

        /* We get here if not in RESET and not HALTed.
         * If the CPU was put into SYNC mode by 'SYNC' or 'CWAI'
         * then this point will force the emulation to exit execution
         * and stay in wait mode, or if an interrupt was latched
         * then execution will proceed with op-code fetch.
         */
        if (cpu.cpu_state) // Something OTHER than CPU_EXEC
        {
            if ( cpu.cpu_state == CPU_SYNC )
            {
                if ( intr_latch & (INT_NMI | INT_FIRQ | INT_IRQ) )
                {
                    cycles_this_scanline = 0;
                    cpu.cpu_state = CPU_EXEC;
                }
                else
                {
                    return; // Waiting for an interrupt (masked or not)
                }
            }

            if (cpu.cpu_state == CPU_HALTED)
            {
                if ( !(cc.f) && (intr_latch & INT_FIRQ) )
                {
                    cpu.cpu_state = CPU_EXEC;
                    cpu.pc = (mem_read(VEC_FIRQ) << 8) + mem_read(VEC_FIRQ+1);
                }
                else if ( !(cc.i) && (intr_latch & INT_IRQ) )
                {
                    cpu.cpu_state = CPU_EXEC;
                    cpu.pc = (mem_read(VEC_IRQ) << 8) + mem_read(VEC_IRQ+1);
                }
                else return; // We're waiting for an unmasked interrupt
            }
        }

        if (intr_latch)
        {
            /* If an interrupt is received and it is enabled, then
             * setup stack frame and call interrupt service by
             * setting the PC to the vectors content.
             * Release CPU state to CPU_EXEC to let CPU emulation
             * start fetching and executing instructions.
             *
             * NMI signal is latched at any time and services here.
             * The NMI signal is transition driven.
             * The NMI latch/logic is cleared when it is acknowledged.
             * FIRQ and IRQ will be samples at each op-code cycle,
             * but if the IRQ/FIRQ signal was removed before sampling
             * then it will not be serviced.
             * The IRQ and FIRQ signal is level driven.
             */
            if ( cpu.nmi_armed && (intr_latch & INT_NMI) )
            {
                cpu.cpu_state = CPU_EXEC;
                cc.e = CC_FLAG_SET;
                cycles_this_scanline += 20;

                cpu.s--;
                mem_write(cpu.s, cpu.pc & 0xff);
                cpu.s--;
                mem_write(cpu.s, (cpu.pc >> 8) & 0xff);
                cpu.s--;
                mem_write(cpu.s, cpu.u & 0xff);
                cpu.s--;
                mem_write(cpu.s, (cpu.u >> 8) & 0xff);
                cpu.s--;
                mem_write(cpu.s, cpu.y & 0xff);
                cpu.s--;
                mem_write(cpu.s, (cpu.y >> 8) & 0xff);
                cpu.s--;
                mem_write(cpu.s, cpu.x & 0xff);
                cpu.s--;
                mem_write(cpu.s, (cpu.x >> 8) & 0xff);
                cpu.s--;
                mem_write(cpu.s, cpu.dp);
                cpu.s--;
                mem_write(cpu.s, cpu.b);
                cpu.s--;
                mem_write(cpu.s, cpu.a);
                cpu.s--;
                mem_write(cpu.s, get_cc());

                cpu.nmi_latched = 0;
                intr_latch &= ~INT_NMI;

                cc.f = CC_FLAG_SET;
                cc.i = CC_FLAG_SET;

                cpu.pc = (mem_read(VEC_NMI) << 8) + mem_read(VEC_NMI+1);
            }
            else if ( !(cc.f) && (intr_latch & INT_FIRQ) )
            {
                cpu.cpu_state = CPU_EXEC;
                cc.e = CC_FLAG_CLR;
                cycles_this_scanline += 10;

                cpu.s--;
                mem_write(cpu.s, cpu.pc & 0xff);
                cpu.s--;
                mem_write(cpu.s, (cpu.pc >> 8) & 0xff);
                cpu.s--;
                mem_write(cpu.s, get_cc());

                cc.f = CC_FLAG_SET;
                cc.i = CC_FLAG_SET;

                cpu.pc = (mem_read(VEC_FIRQ) << 8) + mem_read(VEC_FIRQ+1);
            }
            else if ( !(cc.i) && (intr_latch & INT_IRQ) )
            {
                cpu.cpu_state = CPU_EXEC;
                cc.e = CC_FLAG_SET;
                cycles_this_scanline += 20;

                cpu.s--;
                mem_write(cpu.s, cpu.pc & 0xff);
                cpu.s--;
                mem_write(cpu.s, (cpu.pc >> 8) & 0xff);
                cpu.s--;
                mem_write(cpu.s, cpu.u & 0xff);
                cpu.s--;
                mem_write(cpu.s, (cpu.u >> 8) & 0xff);
                cpu.s--;
                mem_write(cpu.s, cpu.y & 0xff);
                cpu.s--;
                mem_write(cpu.s, (cpu.y >> 8) & 0xff);
                cpu.s--;
                mem_write(cpu.s, cpu.x & 0xff);
                cpu.s--;
                mem_write(cpu.s, (cpu.x >> 8) & 0xff);
                cpu.s--;
                mem_write(cpu.s, cpu.dp);
                cpu.s--;
                mem_write(cpu.s, cpu.b);
                cpu.s--;
                mem_write(cpu.s, cpu.a);
                cpu.s--;
                mem_write(cpu.s, get_cc());

                cc.i = CC_FLAG_SET;

                cpu.pc = (mem_read(VEC_IRQ) << 8) + mem_read(VEC_IRQ+1);
            }
        }

        // Fetch the OP Code directly from memory
        op_code = mem_read_pc(cpu.pc++);

        // Process the Op-Code... handle the double-byte instructions as part of the normal case
        {
            /* 'operand8' will be operand byte, and for a 16-bit operand 'operand8'
             * will be the high order byte and low order byte should be read separately
             * and combined into 16-bit value.
             */
            cycles_this_scanline += machine_code[op_code].cycles;

            eff_addr = get_eff_addr(machine_code[op_code].mode);

            switch ( op_code )
            {
                case 0x11:
                {
                    op_code = mem_read_pc(cpu.pc++);

                    cycles_this_scanline += machine_code_11[op_code].cycles;

                    eff_addr = get_eff_addr(machine_code_11[op_code].mode);

                    switch ( op_code )
                    {
                        /* CMPU
                         */
                        case 0x83:
                        case 0x93:
                        case 0xa3:
                        case 0xb3:
                            operand8 = (uint8_t) mem_read(eff_addr);
                            eff_addr++;
                            operand16 = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                            cmp16(cpu.u, operand16);
                            break;

                        /* CMPS
                         */
                        case 0x8c:
                        case 0x9c:
                        case 0xac:
                        case 0xbc:
                            operand8 = (uint8_t) mem_read(eff_addr);
                            eff_addr++;
                            operand16 = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                            cmp16(cpu.s, operand16);
                            break;

                        /* SWI3
                         */
                        case 0x3f:
                            swi(3);
                            break;

                        default:
                            /* Exception: Illegal 0x11 op-code cpu_run()
                             */
                            if (debug[6] == 0) {debug[6] = op_code;}
                            cpu.cpu_state = CPU_EXCEPTION;
                    }
                }
                break;

                case 0x10:
                {
                    op_code = mem_read_pc(cpu.pc++);

                    cycles_this_scanline += machine_code_10[op_code].cycles;

                    eff_addr = get_eff_addr(machine_code_10[op_code].mode);

                    switch ( op_code )
                    {
                        /* CMPD
                         */
                        case 0x83:
                        case 0x93:
                        case 0xa3:
                        case 0xb3:
                            operand8 = (uint8_t) mem_read(eff_addr);
                            eff_addr++;
                            operand16 = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                            cmp16(d, operand16);
                            break;

                        /* CMPY
                         */
                        case 0x8c:
                        case 0x9c:
                        case 0xac:
                        case 0xbc:
                            operand8 = (uint8_t) mem_read(eff_addr);
                            eff_addr++;
                            operand16 = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                            cmp16(cpu.y, operand16);
                            break;

                        /* LDS
                         */
                        case 0xce:
                        case 0xde:
                        case 0xee:
                        case 0xfe:
                            operand8 = (uint8_t) mem_read(eff_addr);
                            eff_addr++;
                            cpu.s = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                            eval_cc_z16(cpu.s);
                            eval_cc_n16(cpu.s);
                            cc.v = CC_FLAG_CLR;
                            cpu.nmi_armed = 1;
                            break;

                        /* LDY
                         */
                        case 0x8e:
                        case 0x9e:
                        case 0xae:
                        case 0xbe:
                            operand8 = (uint8_t) mem_read(eff_addr);
                            eff_addr++;
                            cpu.y = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                            eval_cc_z16(cpu.y);
                            eval_cc_n16(cpu.y);
                            cc.v = CC_FLAG_CLR;
                            break;

                        /* STS
                         */
                        case 0xdf:
                        case 0xef:
                        case 0xff:
                            mem_write(eff_addr, (uint8_t) (cpu.s >> 8));
                            mem_write(eff_addr + 1, (uint8_t) (cpu.s));
                            eval_cc_z16(cpu.s);
                            eval_cc_n16(cpu.s);
                            cc.v = CC_FLAG_CLR;
                            break;

                        /* STY
                         */
                        case 0x9f:
                        case 0xaf:
                        case 0xbf:
                            mem_write(eff_addr, (uint8_t) (cpu.y >> 8));
                            mem_write(eff_addr + 1, (uint8_t) (cpu.y));
                            eval_cc_z16(cpu.y);
                            eval_cc_n16(cpu.y);
                            cc.v = CC_FLAG_CLR;
                            break;

                        /* LBRN
                         */
                        case 0x21:
                            // Long branch never
                            break;

                        /* Long conditional branches
                         */
                        case 0x22 ... 0x2f:
                            branch(op_code, 1, eff_addr);
                            break;

                        /* SWI2
                         */
                        case 0x3f:
                            swi(2);
                            break;

                        default:
                            /* Exception: Illegal 0x10 op-code cpu_run()
                             */
                             if (debug[6] == 0) {debug[6] = op_code;}
                            cpu.cpu_state = CPU_EXCEPTION;
                    }
                }
                break;
                /* ABX
                 */
                case 0x3a:
                    cpu.x += cpu.b;
                    break;

                /* ADCA
                 */
                case 0x89:
                case 0x99:
                case 0xa9:
                case 0xb9:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.a = adc(cpu.a, operand8);
                    break;

                /* ADCB
                 */
                case 0xc9:
                case 0xd9:
                case 0xe9:
                case 0xf9:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.b = adc(cpu.b, operand8);
                    break;

                /* ADDA
                 */
                case 0x8b:
                case 0x9b:
                case 0xab:
                case 0xbb:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.a = add(cpu.a, operand8);
                    break;

                /* ADDB
                 */
                case 0xcb:
                case 0xdb:
                case 0xeb:
                case 0xfb:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.b = add(cpu.b, operand8);
                    break;

                /* ADDD
                 */
                case 0xc3:
                case 0xd3:
                case 0xe3:
                case 0xf3:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    eff_addr++;
                    operand16 = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                    addd(operand16);
                    break;

                /* ANDA
                 */
                case 0x84:
                case 0x94:
                case 0xa4:
                case 0xb4:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.a = and(cpu.a, operand8);
                    break;

                /* ADDB
                 */
                case 0xc4:
                case 0xd4:
                case 0xe4:
                case 0xf4:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.b = and(cpu.b, operand8);
                    break;

                /* ANDCC
                 */
                case 0x1c:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    andcc(operand8);
                    break;

                /* ASL, ASLA, ASLB
                 * LSL, LSLA, LSLB
                 */
                case 0x08:
                case 0x68:
                case 0x78:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = asl(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                case 0x48:
                    cpu.a = asl(cpu.a);
                    break;

                case 0x58:
                    cpu.b = asl(cpu.b);
                    break;

                /* ASR, ASRA, ASRB
                 */
                case 0x07:
                case 0x67:
                case 0x77:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = asr(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                case 0x47:
                    cpu.a = asr(cpu.a);
                    break;

                case 0x57:
                    cpu.b = asr(cpu.b);
                    break;

                /* BITA
                 */
                case 0x85:
                case 0x95:
                case 0xa5:
                case 0xb5:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    bit(cpu.a, operand8);
                    break;

                /* BITB
                 */
                case 0xc5:
                case 0xd5:
                case 0xe5:
                case 0xf5:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    bit(cpu.b, operand8);
                    break;

                /* CLR, CLRA, CLRB
                 */
                case 0x0f:
                case 0x6f:
                case 0x7f:
                    operand8 = clr();
                    mem_write(eff_addr, operand8);
                    break;

                case 0x4f:
                    cpu.a = clr();
                    break;

                case 0x5f:
                    cpu.b = clr();
                    break;

                /* CMPA
                 */
                case 0x81:
                case 0x91:
                case 0xa1:
                case 0xb1:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cmp(cpu.a, operand8);
                    break;

                /* CMPB
                 */
                case 0xc1:
                case 0xd1:
                case 0xe1:
                case 0xf1:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cmp(cpu.b, operand8);
                    break;

                /* CMPX
                 */
                case 0x8c:
                case 0x9c:
                case 0xac:
                case 0xbc:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    eff_addr++;
                    operand16 = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                    cmp16(cpu.x, operand16);
                    break;

                /* COM, COMA, COMB
                 */
                case 0x03:
                case 0x63:
                case 0x73:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = com(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                case 0x43:
                    cpu.a = com(cpu.a);
                    break;

                case 0x53:
                    cpu.b = com(cpu.b);
                    break;

                /* CWAI
                 */
                case 0x3c:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cwai(operand8);
                    break;

                /* DAA
                 */
                case 0x19:
                    daa();
                    break;

                /* DEC, DECA, DECB
                 */
                case 0x0a:
                case 0x0b:
                case 0x6a:
                case 0x7a:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = dec(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                case 0x4a:
                    cpu.a = dec(cpu.a);
                    break;

                case 0x5a:
                    cpu.b = dec(cpu.b);
                    break;

                /* EORA
                 */
                case 0x88:
                case 0x98:
                case 0xa8:
                case 0xb8:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.a = eor(cpu.a, operand8);
                    break;

                /* EORB
                 */
                case 0xc8:
                case 0xd8:
                case 0xe8:
                case 0xf8:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.b = eor(cpu.b, operand8);
                    break;

                /* EXG
                 */
                case 0x1e:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    exg(operand8);
                    break;

                /* INC, INCA, INCB
                 */
                case 0x0c:
                case 0x6c:
                case 0x7c:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = inc(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                case 0x4c:
                    cpu.a = inc(cpu.a);
                    break;

                case 0x5c:
                    cpu.b = inc(cpu.b);
                    break;

                /* JMP
                 */
                case 0x0e:
                case 0x6e:
                case 0x7e:
                    cpu.pc = eff_addr;
                    break;

                /* JSR
                 */
                case 0x9d:
                case 0xad:
                case 0xbd:
                    cpu.s--;
                    mem_write(cpu.s, GET_REG_LOW(cpu.pc));
                    cpu.s--;
                    mem_write(cpu.s, GET_REG_HIGH(cpu.pc));
                    cpu.pc = eff_addr;
                    break;

                /* LDA
                 */
                case 0x86:
                case 0x96:
                case 0xa6:
                case 0xb6:
                    cpu.a = (uint8_t) mem_read(eff_addr);
                    eval_cc_z((uint16_t) cpu.a);
                    eval_cc_n((uint16_t) cpu.a);
                    cc.v = CC_FLAG_CLR;
                    break;

                /* LDB
                 */
                case 0xc6:
                case 0xd6:
                case 0xe6:
                case 0xf6:
                    cpu.b = (uint8_t) mem_read(eff_addr);
                    eval_cc_z((uint16_t) cpu.b);
                    eval_cc_n((uint16_t) cpu.b);
                    cc.v = CC_FLAG_CLR;
                    break;

                /* LDD
                 */
                case 0xcc:
                case 0xdc:
                case 0xec:
                case 0xfc:
                    cpu.a = (uint8_t) mem_read(eff_addr);
                    eff_addr++;
                    cpu.b = (uint8_t) mem_read(eff_addr);
                    eval_cc_z16(d);
                    eval_cc_n16(d);
                    cc.v = CC_FLAG_CLR;
                    break;

                /* LDU
                 */
                case 0xce:
                case 0xde:
                case 0xee:
                case 0xfe:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    eff_addr++;
                    cpu.u = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                    eval_cc_z16(cpu.u);
                    eval_cc_n16(cpu.u);
                    cc.v = CC_FLAG_CLR;
                    break;

                /* LDX
                 */
                case 0x8e:
                case 0x9e:
                case 0xae:
                case 0xbe:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    eff_addr++;
                    cpu.x = ((uint16_t) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                    eval_cc_z16(cpu.x);
                    eval_cc_n16(cpu.x);
                    cc.v = CC_FLAG_CLR;
                    break;

                /* LEA
                 */
                case 0x30:
                    cpu.x = eff_addr;
                    eval_cc_z16(cpu.x);
                    break;

                case 0x31:
                    cpu.y = eff_addr;
                    eval_cc_z16(cpu.y);
                    break;

                case 0x32:
                    cpu.s = eff_addr;
                    cpu.nmi_armed = 1;
                    break;

                case 0x33:
                    cpu.u = eff_addr;
                    break;

                /* LSR, LSRA, LSRB
                 */
                case 0x04:
                case 0x05:
                case 0x64:
                case 0x74:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = lsr(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                case 0x44:
                case 0x45:
                    cpu.a = lsr(cpu.a);
                    break;

                case 0x55:
                case 0x54:
                    cpu.b = lsr(cpu.b);
                    break;

                /* MUL
                 */
                case 0x3d:
                    operand16 = cpu.a * cpu.b;
                    cpu.a = GET_REG_HIGH(operand16);
                    cpu.b = GET_REG_LOW(operand16);
                    eval_cc_z16(operand16);
                    cc.c = (cpu.b & 0x80) ? CC_FLAG_SET : CC_FLAG_CLR;
                    break;

                /* NEG, NEGA, NEGB
                 */
                case 0x00:
                case 0x01:
                case 0x60:
                case 0x61:
                case 0x70:
                case 0x71:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = neg(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                case 0x40:
                    cpu.a = neg(cpu.a);
                    break;

                case 0x50:
                    cpu.b = neg(cpu.b);
                    break;

                /* NOP
                 */
                case 0x12:
                case 0x1B:
                    break;

                /* ORA, ORB
                 */
                case 0x8a:
                case 0x9a:
                case 0xaa:
                case 0xba:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.a = or(cpu.a, operand8);
                    break;

                case 0xca:
                case 0xda:
                case 0xea:
                case 0xfa:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.b = or(cpu.b, operand8);
                    break;

                /* ORCC
                 */
                case 0x1a:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    orcc(operand8);
                    break;

                /* PSHS, PSHU
                 */
                case 0x34:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    pshs(operand8, &cycles_this_scanline);
                    break;

                case 0x36:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    pshu(operand8, &cycles_this_scanline);
                    break;

                /* PULS, PULU
                 */
                case 0x35:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    puls(operand8, &cycles_this_scanline);
                    break;

                case 0x37:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    pulu(operand8, &cycles_this_scanline);
                    break;

                /* ROL, ROLA, ROLB
                 */
                case 0x09:
                case 0x69:
                case 0x79:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = rol(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                case 0x49:
                    cpu.a = rol(cpu.a);
                    break;

                case 0x59:
                    cpu.b = rol(cpu.b);
                    break;

                /* ROR, RORA, RORB
                 */
                case 0x06:
                case 0x66:
                case 0x76:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    operand8 = ror(operand8);
                    mem_write(eff_addr, operand8);
                    break;

                case 0x46:
                    cpu.a = ror(cpu.a);
                    break;

                case 0x56:
                    cpu.b = ror(cpu.b);
                    break;

                /* RTI
                 */
                case 0x3b:
                    rti(&cycles_this_scanline);
                    break;

                /* RTS
                 */
                case 0x39:
                     /* Restore PC and return
                      */
                     operand8 = mem_read(cpu.s);
                     cpu.s++;
                     cpu.pc = (uint16_t) operand8 << 8;
                     operand8 = mem_read(cpu.s);
                     cpu.s++;
                     cpu.pc += operand8;
                     break;

                /* SBCA
                 */
                case 0x82:
                case 0x92:
                case 0xa2:
                case 0xb2:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.a = sbc(cpu.a, operand8);
                    break;

                /* SBCB
                 */
                case 0xc2:
                case 0xd2:
                case 0xe2:
                case 0xf2:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.b = sbc(cpu.b, operand8);
                    break;

                /* SEX
                 */
                case 0x1d:
                    sex();
                    break;

                /* STA
                 */
                case 0x97:
                case 0xa7:
                case 0xb7:
                    mem_write(eff_addr, cpu.a);
                    eval_cc_z((uint16_t) cpu.a);
                    eval_cc_n((uint16_t) cpu.a);
                    cc.v = CC_FLAG_CLR;
                    break;

                /* STB
                 */
                case 0xd7:
                case 0xe7:
                case 0xf7:
                    mem_write(eff_addr, cpu.b);
                    eval_cc_z((uint16_t) cpu.b);
                    eval_cc_n((uint16_t) cpu.b);
                    cc.v = CC_FLAG_CLR;
                    break;

                /* STD
                 */
                case 0xdd:
                case 0xed:
                case 0xfd:
                    mem_write(eff_addr, cpu.a);
                    mem_write(eff_addr + 1, cpu.b);
                    eval_cc_z16(d);
                    eval_cc_n16(d);
                    cc.v = CC_FLAG_CLR;
                    break;

                /* STU
                 */
                case 0xdf:
                case 0xef:
                case 0xff:
                    mem_write(eff_addr, (uint8_t) (cpu.u >> 8));
                    mem_write(eff_addr + 1, (uint8_t) (cpu.u));
                    eval_cc_z16(cpu.u);
                    eval_cc_n16(cpu.u);
                    cc.v = CC_FLAG_CLR;
                    break;

                /* STX
                 */
                case 0x9f:
                case 0xaf:
                case 0xbf:
                    mem_write(eff_addr, (uint8_t) (cpu.x >> 8));
                    mem_write(eff_addr + 1, (uint8_t) (cpu.x));
                    eval_cc_z16(cpu.x);
                    eval_cc_n16(cpu.x);
                    cc.v = CC_FLAG_CLR;
                    break;

                /* SUBA
                 */
                case 0x80:
                case 0x90:
                case 0xa0:
                case 0xb0:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.a = sub(cpu.a, operand8);
                    break;

                /* SUBB
                 */
                case 0xc0:
                case 0xd0:
                case 0xe0:
                case 0xf0:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    cpu.b = sub(cpu.b, operand8);
                    break;

                /* SUBD
                 */
                case 0x83:
                case 0x93:
                case 0xa3:
                case 0xb3:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    eff_addr++;
                    operand16 = ((uint16_t ) operand8 << 8) + (uint16_t) mem_read(eff_addr);
                    subd(operand16);
                    break;

                /* SWI
                 */
                case 0x3f:
                    swi(1);
                    break;

                /* SYNC
                 *
                 * The SYNC instruction allows software to synchronize with an external hardware
                 * event (interrupt). When executed, SYNC stops executing instructions and waits
                 * for an interrupt. None of the CC flags are directly affected.
                 */
                case 0x13:
                    cpu.cpu_state = CPU_SYNC;
                    break;

                /* TFR
                 */
                case 0x1f:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    tfr(operand8);
                    break;

                /* TSTA
                 */
                case 0x4d:
                    tst(cpu.a);
                    break;

                /* TSTB
                 */
                case 0x5d:
                    tst(cpu.b);
                    break;

                /* TST
                 */
                case 0x0d:
                case 0x6d:
                case 0x7d:
                    operand8 = (uint8_t) mem_read(eff_addr);
                    tst(operand8);
                    break;

                /* BRA / LBRA
                 */
                case 0x20:
                case 0x16:
                    cpu.pc = eff_addr;
                    break;

                /* BRN
                 */
                case 0x21:
                    // Branch never
                    break;

                /* BSR / LBSR
                 */
                case 0x8d:
                case 0x17:
                    cpu.s--;
                    mem_write(cpu.s, GET_REG_LOW(cpu.pc));
                    cpu.s--;
                    mem_write(cpu.s, GET_REG_HIGH(cpu.pc));
                    cpu.pc = eff_addr;
                    break;

                /* Short conditional branches
                 */
                case 0x22 ... 0x2f:
                    branch(op_code, 0, eff_addr);
                    break;

                case 0x87:
                case 0xC7:
                    cc.n = CC_FLAG_SET;
                    cc.z = CC_FLAG_CLR;
                    cc.v = CC_FLAG_CLR;
                    break;

                case 0x02:
                case 0x62:
                case 0x72:
                    if (cc.c)   // If Carry Set... COM
                    {
                        operand8 = (uint8_t) mem_read(eff_addr);
                        operand8 = com(operand8);
                        mem_write(eff_addr, operand8);
                    }
                    else // Carry Clear... NEG
                    {
                        operand8 = (uint8_t) mem_read(eff_addr);
                        operand8 = neg(operand8);
                        mem_write(eff_addr, operand8);
                    }
                    break;

                default:
                    /* Exception: Illegal op-code cpu_run()
                     */
                    if (debug[7] == 0) {debug[7] = op_code;}
                    cpu.cpu_state = CPU_EXCEPTION;

            }
        }

        if (cycles_this_scanline >= cycles_per_line)
        {
            cycles_this_scanline -= cycles_per_line;
            break;
        }
    }
}

/*------------------------------------------------
 * adc()
 *
 *  Add with carry.
 *
 *  acc+byte+carry
 */
inline __attribute__((always_inline)) uint8_t adc(uint8_t acc, uint8_t byte)
{
    uint16_t result;

    result = (acc + byte + cc.c);

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(acc, byte, result);
    eval_cc_h(acc, byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * add()
 *
 *  Add.
 *
 *  acc+byte
 */
inline __attribute__((always_inline)) uint8_t add(uint8_t acc, uint8_t byte)
{
    uint16_t result;

    result = (acc + byte);

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(acc, byte, result);
    eval_cc_h(acc, byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * addd()
 *
 *  Add 16-bit operand to Acc-D
 *
 *  acc+word
 */
void addd(uint16_t word)
{
    uint16_t acc;
    uint32_t result;

    acc = (cpu.a << 8) + cpu.b;
    result = acc + word;

    cpu.a = result >> 8;
    cpu.b = result & 0xff;

    eval_cc_c16(result);
    eval_cc_z16(result);
    eval_cc_v16(acc, word, result);
    eval_cc_n16(result);
}

/*------------------------------------------------
 * and()
 *
 *  Logical AND accumulator with byte operand.
 *
 */
inline __attribute__((always_inline)) uint8_t and(uint8_t acc, uint8_t byte)
{
    uint8_t result;

    result = (acc & byte);

    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);
    cc.v = CC_FLAG_CLR;

    return result;
}

/*------------------------------------------------
 * andcc()
 *
 *  Logical AND condition-code register with operand.
 *
 */
void andcc(uint8_t byte)
{
    uint8_t temp_cc;

    temp_cc = get_cc();
    temp_cc &= byte;
    set_cc(temp_cc);
}

/*------------------------------------------------
 * asl()
 *
 *  Arithmetic shift left.
 *
 *  'byte' shift left, MSB into carry flag.
 */
inline __attribute__((always_inline)) uint8_t asl(uint8_t byte)
{
    uint16_t result;

    result = ((uint16_t) byte) << 1;

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(byte, byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * asr()
 *
 *  Arithmetic shift right.
 *
 *  'byte' shift right, MSB replicated b7, LSB into carry flag.
 */
inline __attribute__((always_inline)) uint8_t asr(uint8_t byte)
{
    uint8_t result;

    result = (byte >> 1) | (byte & 0x80);

    cc.c = byte & 0x01 ? CC_FLAG_SET : CC_FLAG_CLR;
    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);

    return result;
}

/*------------------------------------------------
 * bit()
 *
 *  Bit test accumulator with operand by AND
 *  without changing accumulator.
 *  Change flag bit appropriately.
 *
 *  acc AND byte
 */
inline __attribute__((always_inline))  void bit(uint8_t acc, uint8_t byte)
{
    uint8_t result;

    result = acc & byte;

    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);
    cc.v = CC_FLAG_CLR;
}

/*------------------------------------------------
 * clr()
 *
 *  Clear (zero) bits in operand.
 *  Change flag bit appropriately.
 *
 */
inline __attribute__((always_inline)) uint8_t clr(void)
{
    cc.c = CC_FLAG_CLR;
    cc.v = CC_FLAG_CLR;
    cc.z = CC_FLAG_SET;
    cc.n = CC_FLAG_CLR;

    return 0;
}

/*------------------------------------------------
 * cmp()
 *
 *  Compare 'arg' to 'byte' by subtracting
 *  the byte from the argument and updating the
 *  flags.
 *
 */
inline __attribute__((always_inline))  void cmp(uint8_t arg, uint8_t byte)
{
    uint16_t result;

    result = arg - byte;

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(arg, ~byte, result);
}

/*------------------------------------------------
 * cmp16()
 *
 *  Compare 'arg' to 'word' by subtracting
 *  the a 16-bit value from the argument and updating the
 *  flags.
 *
 */
inline __attribute__((always_inline)) void cmp16(uint16_t arg, uint16_t word)
{
    uint32_t result;

    result = arg - word;

    eval_cc_c16(result);
    eval_cc_z16(result);
    eval_cc_v16(arg, ~word, result);
    eval_cc_n16(result);
}

/*------------------------------------------------
 * com()
 *
 *  Complement (bit-wise NOT) a byte.
 *
 *  ~byte
 */
inline __attribute__((always_inline)) uint8_t com(uint8_t byte)
{
    uint8_t result;

    result = ~byte;

    cc.c = CC_FLAG_SET;
    cc.v = CC_FLAG_CLR;
    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);

    return result;
}

/*------------------------------------------------
 * cwai()
 *
 *  This instruction logically ANDs the contents of the Condition Codes register with the 8-
 *  bit value specified by the immediate operand. The result is placed back into the
 *  Condition Codes register. The E flag in the CC register is then set and the entire machine
 *  state is pushed onto the hardware stack (S). The CPU then halts execution and waits for
 *  an unmasked interrupt to occur. When such an interrupt occurs, the CPU resumes
 *  execution at the address obtained from the corresponding interrupt vector.
 *
 */
void cwai(uint8_t byte)
{
    uint8_t temp_cc;

    temp_cc = get_cc();
    temp_cc &= byte;
    temp_cc |= 0x80;
    set_cc(temp_cc);

    cpu.s--;
    mem_write(cpu.s, cpu.pc & 0xff);
    cpu.s--;
    mem_write(cpu.s, (cpu.pc >> 8) & 0xff);
    cpu.s--;
    mem_write(cpu.s, cpu.u & 0xff);
    cpu.s--;
    mem_write(cpu.s, (cpu.u >> 8) & 0xff);
    cpu.s--;
    mem_write(cpu.s, cpu.y & 0xff);
    cpu.s--;
    mem_write(cpu.s, (cpu.y >> 8) & 0xff);
    cpu.s--;
    mem_write(cpu.s, cpu.x & 0xff);
    cpu.s--;
    mem_write(cpu.s, (cpu.x >> 8) & 0xff);
    cpu.s--;
    mem_write(cpu.s, cpu.dp);
    cpu.s--;
    mem_write(cpu.s, cpu.b);
    cpu.s--;
    mem_write(cpu.s, cpu.a);
    cpu.s--;
    mem_write(cpu.s, temp_cc);

    cpu.cpu_state = CPU_HALTED;
}

/*------------------------------------------------
 * daa()
 *
 *  Decimal adjust accumulator A
 *
 */
void daa(void)
{
    uint16_t    temp;
    uint16_t    high_nibble;
    uint16_t    low_nibble;

    temp = cpu.a;
    high_nibble = temp & 0xf0;
    low_nibble = temp & 0x0f;

    if ( low_nibble > 0x09 || cc.h )
        temp += 0x06;

    if ( high_nibble > 0x80 && low_nibble > 0x09 )
        temp += 0x60;
    else if (high_nibble > 0x90 || cc.c)
        temp += 0x60;

    cpu.a = temp;

    eval_cc_c(temp);
    eval_cc_z(temp);
    eval_cc_n(temp);
    cc.v = CC_FLAG_CLR;
}

/*------------------------------------------------
 * dec()
 *
 *  Decrement the operand.
 *
 *  byte = byte - 1
 */
inline __attribute__((always_inline)) uint8_t dec(uint8_t byte)
{
    uint16_t result;

    result = byte - 1;

    eval_cc_v(byte, 0xfe, result);
    eval_cc_z(result);
    eval_cc_n(result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * eor()
 *
 *  Exclusive OR accumulator with operand.
 *
 *  acc ^ byte
 */
inline __attribute__((always_inline)) uint8_t eor(uint8_t acc, uint8_t byte)
{
    uint8_t result;

    result = acc ^ byte;

    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);
    cc.v = CC_FLAG_CLR;

    return result;
}

/*------------------------------------------------
 * exg()
 *
 *  Exchange like-sized registers.
 *
 *  NOTE: The function relies on the assembler to not mix
 *  8-bit registers with 16-bit register, otherwise
 *  results are unexpected.
 *  Check: if (((regs ^ (regs << 4)) & 0x80) == 0) {...}
 *
 */
inline __attribute__((always_inline)) void exg(uint8_t regs)
{
    int         src, dst;
    uint16_t    temp1, temp2;

    src = (int)((regs >> 4) & 0x0f);
    dst = (int)(regs & 0x0f);

    temp1 = read_register(src);
    temp2 = read_register(dst);

    write_register(dst, temp1);
    write_register(src, temp2);
}

/*------------------------------------------------
 * inc()
 *
 *  Increment the operand.
 *
 *  byte = byte + 1
 */
inline __attribute__((always_inline)) uint8_t inc(uint8_t byte)
{
    uint16_t result;

    result = byte + 1;

    eval_cc_v(byte, 1, result);
    eval_cc_z(result);
    eval_cc_n(result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * lsr()
 *
 *  Logic shift right.
 *
 *  'byte' shift right, MSB replicated with zero, LSB into carry flag.
 */
inline __attribute__((always_inline)) uint8_t lsr(uint8_t byte)
{
    uint8_t result;

    result = (byte >> 1) & 0x7f;

    cc.c = byte & 0x01 ? CC_FLAG_SET : CC_FLAG_CLR;
    eval_cc_z((uint16_t) result);
    cc.n = CC_FLAG_CLR;

    return result;
}

/*------------------------------------------------
 * neg()
 *
 *  Negate byte.
 *  Two's complement: ~byte+1
 *
 */
inline __attribute__((always_inline)) uint8_t neg(uint8_t byte)
{
    uint16_t result;

    result =  0 - byte;

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(0, ~byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * or()
 *
 *  Bit-wise logical OR between 'acc' and 'byte'
 *
 */
inline __attribute__((always_inline)) uint8_t or(uint8_t acc, uint8_t byte)
{
    uint8_t result;

    result = acc | byte;

    cc.v = CC_FLAG_CLR;
    eval_cc_z((uint16_t) result);
    eval_cc_n((uint16_t) result);

    return result;
}

/*------------------------------------------------
 * orcc()
 *
 *  Logical OR condition-code register with operand.
 *
 */
inline __attribute__((always_inline)) void orcc(uint8_t byte)
{
    uint8_t temp_cc;

    temp_cc = get_cc();
    temp_cc |= byte;
    set_cc(temp_cc);
}

/*------------------------------------------------
 * pshs()
 *
 *  Push registers onto stack S (systems)
 *  Modifies 's' stack register.
 *
 *  param:  Push-list operand, and command cycles to update if needed.
 *  return: Nothing
 */
void pshs(uint8_t push_list, int *cycles)
{
    (*cycles)++;

    if ( push_list & 0x80 )
    {
        (*cycles)++;
        cpu.s--;
        mem_write(cpu.s, cpu.pc & 0xff);
        cpu.s--;
        mem_write(cpu.s, (cpu.pc >> 8) & 0xff);
    }

    if ( push_list & 0x40 )
    {
        (*cycles)++;
        cpu.s--;
        mem_write(cpu.s, cpu.u & 0xff);
        cpu.s--;
        mem_write(cpu.s, (cpu.u >> 8) & 0xff);
    }

    if ( push_list & 0x20 )
    {
        (*cycles)++;
        cpu.s--;
        mem_write(cpu.s, cpu.y & 0xff);
        cpu.s--;
        mem_write(cpu.s, (cpu.y >> 8) & 0xff);
    }

    if ( push_list & 0x10 )
    {
        (*cycles)++;
        cpu.s--;
        mem_write(cpu.s, cpu.x & 0xff);
        cpu.s--;
        mem_write(cpu.s, (cpu.x >> 8) & 0xff);
    }

    if ( push_list & 0x08 )
    {
        cpu.s--;
        mem_write(cpu.s, cpu.dp);
    }

    if ( push_list & 0x04 )
    {
        cpu.s--;
        mem_write(cpu.s, cpu.b);
    }

    if ( push_list & 0x02 )
    {
        cpu.s--;
        mem_write(cpu.s, cpu.a);
    }

    if ( push_list & 0x01 )
    {
        cpu.s--;
        mem_write(cpu.s, (int) get_cc());
    }
}

/*------------------------------------------------
 * pshu()
 *
 *  Push registers onto stack U (user)
 *  Modifies 'u' stack register.
 *
 *  param:  Push-list operand, and command cycles to update if needed.
 *  return: Nothing
 */
void pshu(uint8_t push_list, int *cycles)
{
    (*cycles)++;

    if ( push_list & 0x80 )
    {
        (*cycles)++;
        cpu.u--;
        mem_write(cpu.u, cpu.pc & 0xff);
        cpu.u--;
        mem_write(cpu.u, (cpu.pc >> 8) & 0xff);
    }

    if ( push_list & 0x40 )
    {
        (*cycles)++;
        cpu.u--;
        mem_write(cpu.u, cpu.s & 0xff);
        cpu.u--;
        mem_write(cpu.u, (cpu.s >> 8) & 0xff);
    }

    if ( push_list & 0x20 )
    {
        (*cycles)++;
        cpu.u--;
        mem_write(cpu.u, cpu.y & 0xff);
        cpu.u--;
        mem_write(cpu.u, (cpu.y >> 8) & 0xff);
    }

    if ( push_list & 0x10 )
    {
        (*cycles)++;
        cpu.u--;
        mem_write(cpu.u, cpu.x & 0xff);
        cpu.u--;
        mem_write(cpu.u, (cpu.x >> 8) & 0xff);
    }

    if ( push_list & 0x08 )
    {
        cpu.u--;
        mem_write(cpu.u, cpu.dp);
    }

    if ( push_list & 0x04 )
    {
        cpu.u--;
        mem_write(cpu.u, cpu.b);
    }

    if ( push_list & 0x02 )
    {
        cpu.u--;
        mem_write(cpu.u, cpu.a);
    }

    if ( push_list & 0x01 )
    {
        cpu.u--;
        mem_write(cpu.u, get_cc());
    }
}

/*------------------------------------------------
 * puls()
 *
 *  Pull registers from stack S (systems)
 *  Modifies 's' stack register.
 *
 *  param:  Pull-list operand, and command cycles to update if needed.
 *  return: Nothing
 */
void puls(uint8_t pull_list, int *cycles)
{
    uint16_t    val;

    (*cycles)++;

    if ( pull_list & 0x01 )
    {
        val = mem_read(cpu.s);
        cpu.s++;
        set_cc((uint8_t) val);
    }

    if ( pull_list & 0x02 )
    {
        val = mem_read(cpu.s);
        cpu.s++;
        cpu.a = val;
    }

    if ( pull_list & 0x04 )
    {
        val = mem_read(cpu.s);
        cpu.s++;
        cpu.b = val;
    }

    if ( pull_list & 0x08 )
    {
        val = mem_read(cpu.s);
        cpu.s++;
        cpu.dp = val;
    }

    if ( pull_list & 0x10 )
    {
        (*cycles)++;
        val = mem_read(cpu.s) << 8;
        cpu.s++;
        val += mem_read(cpu.s);
        cpu.s++;
        cpu.x = val;
    }

    if ( pull_list & 0x20 )
    {
        (*cycles)++;
        val = mem_read(cpu.s) << 8;
        cpu.s++;
        val += mem_read(cpu.s);
        cpu.s++;
        cpu.y = val;
    }

    if ( pull_list & 0x40 )
    {
        (*cycles)++;
        val = mem_read(cpu.s) << 8;
        cpu.s++;
        val += mem_read(cpu.s);
        cpu.s++;
        cpu.u = val;
    }

    if ( pull_list & 0x80 )
    {
        (*cycles)++;
        val = mem_read(cpu.s) << 8;
        cpu.s++;
        val += mem_read(cpu.s);
        cpu.s++;
        cpu.pc = val;
    }
}

/*------------------------------------------------
 * pulu()
 *
 *  Pull registers from stack U (user)
 *  Modifies 'u' stack register.
 *
 *  param:  Pull-list operand, and command cycles to update if needed.
 *  return: Nothing
 */
void pulu(uint8_t pull_list, int *cycles)
{
    uint16_t    val;

    (*cycles)++;

    if ( pull_list & 0x01 )
    {
        val = mem_read(cpu.u);
        cpu.u++;
        set_cc((uint8_t) val);
    }

    if ( pull_list & 0x02 )
    {
        val = mem_read(cpu.u);
        cpu.u++;
        cpu.a = val;
    }

    if ( pull_list & 0x04 )
    {
        val = mem_read(cpu.u);
        cpu.u++;
        cpu.b = val;
    }

    if ( pull_list & 0x08 )
    {
        val = mem_read(cpu.u);
        cpu.u++;
        cpu.dp = val;
    }

    if ( pull_list & 0x10 )
    {
        (*cycles)++;
        val = mem_read(cpu.u) << 8;
        cpu.u++;
        val += mem_read(cpu.u);
        cpu.u++;
        cpu.x = val;
    }

    if ( pull_list & 0x20 )
    {
        (*cycles)++;
        val = mem_read(cpu.u) << 8;
        cpu.u++;
        val += mem_read(cpu.u);
        cpu.u++;
        cpu.y = val;
    }

    if ( pull_list & 0x40 )
    {
        (*cycles)++;
        val = mem_read(cpu.u) << 8;
        cpu.u++;
        val += mem_read(cpu.u);
        cpu.u++;
        cpu.s = val;
    }

    if ( pull_list & 0x80 )
    {
        (*cycles)++;
        val = mem_read(cpu.u) << 8;
        cpu.u++;
        val += mem_read(cpu.u);
        cpu.u++;
        cpu.pc = val;
    }
}

/*------------------------------------------------
 * rol()
 *
 *  Rotate left through Carry
 *
 */
inline __attribute__((always_inline)) uint8_t rol(uint8_t byte)
{
    uint16_t    result;

    result = (byte << 1);

    if ( cc.c )
        result |= 0x0001;
    else
        result &= 0xfffe;

    eval_cc_c(result);
    eval_cc_v(byte, byte, result);
    eval_cc_z(result);
    eval_cc_n(result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * ror()
 *
 *  Rotate right through Carry
 *
 */
inline __attribute__((always_inline)) uint8_t ror(uint8_t byte)
{
    uint16_t    result;


    result = byte;

    if ( cc.c )
        result |= 0x0100;
    else
        result &= 0xfeff;

    if ( byte & 0x01 )
        cc.c = CC_FLAG_SET;
    else
        cc.c = CC_FLAG_CLR;

    result = (result >> 1);

    eval_cc_z(result);
    eval_cc_n(result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * rti()
 *
 *  Return from interrupt
 *
 */
void rti(int *cycles)
{
    uint8_t byte;

    /* Restore CCR
     */
    byte = mem_read(cpu.s);
    cpu.s++;
    set_cc(byte);

    /* Restore registers if this is an extended
     * interrupt frame (IRQ, NMI, SWIx)
     */
    if ( cc.e )
    {
        cpu.a = mem_read(cpu.s);
        cpu.s++;
        cpu.b = mem_read(cpu.s);
        cpu.s++;
        cpu.dp = mem_read(cpu.s);
        cpu.s++;
        cpu.x = mem_read(cpu.s) << 8;
        cpu.s++;
        cpu.x += mem_read(cpu.s);
        cpu.s++;
        cpu.y = mem_read(cpu.s) << 8;
        cpu.s++;
        cpu.y += mem_read(cpu.s);
        cpu.s++;
        cpu.u = mem_read(cpu.s) << 8;
        cpu.s++;
        cpu.u += mem_read(cpu.s);
        cpu.s++;

        (*cycles) += 9;
    }

    /* Restore PC and return
     */
    byte = mem_read(cpu.s);
    cpu.s++;
    cpu.pc = (uint16_t) byte << 8;

    byte = mem_read(cpu.s);
    cpu.s++;
    cpu.pc += (uint16_t) byte;
}

/*------------------------------------------------
 * sbc()
 *
 *  Subtract with carry.
 *
 *  acc-byte-carry
 */
inline __attribute__((always_inline)) uint8_t sbc(uint8_t acc, uint8_t byte)
{
    uint16_t result;

    result = acc - byte - cc.c;

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(acc, ~byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * sex()
 *
 *  Sign extend Acc-B to Acc-A
 *
 */
inline __attribute__((always_inline)) void sex(void)
{
    if ( cpu.b & 0x80 )
        cpu.a = 0xff;
    else
        cpu.a = 0;

    cc.v = CC_FLAG_CLR;
    eval_cc_z((uint16_t) cpu.a);
    eval_cc_n((uint16_t) cpu.a);
}

/*------------------------------------------------
 * sub()
 *
 *  Subtract byte from Acc and set flags
 *
 */
inline __attribute__((always_inline)) uint8_t sub(uint8_t acc, uint8_t byte)
{
    uint16_t result;

    result = acc - byte;

    eval_cc_c(result);
    eval_cc_z(result);
    eval_cc_n(result);
    eval_cc_v(acc, ~byte, result);

    return (uint8_t) result;
}

/*------------------------------------------------
 * subd()
 *
 *  Subtract word from D accumulator and set flags
 *  Using 2's complement addition.
 *
 */
void subd(uint16_t word)
{
    uint16_t acc;
    uint32_t result;

    acc = (cpu.a << 8) + cpu.b;
    result = acc - word;

    cpu.a = result >> 8;
    cpu.b = result & 0xff;

    eval_cc_c16(result);
    eval_cc_z16(result);
    eval_cc_v16(acc, ~word, result);
    eval_cc_n16(result);
}

/*------------------------------------------------
 * swi()
 *
 *  Software interrupt.
 *  SWI type is input to the function:
 *  SWI=1, SWI2=2, SWI3=3
 *
 */
void swi(int swi_id)
{
    cc.e = CC_FLAG_SET;

    cpu.s--;
    mem_write(cpu.s, cpu.pc & 0xff);
    cpu.s--;
    mem_write(cpu.s, (cpu.pc >> 8) & 0xff);
    cpu.s--;
    mem_write(cpu.s, cpu.u & 0xff);
    cpu.s--;
    mem_write(cpu.s, (cpu.u >> 8) & 0xff);
    cpu.s--;
    mem_write(cpu.s, cpu.y & 0xff);
    cpu.s--;
    mem_write(cpu.s, (cpu.y >> 8) & 0xff);
    cpu.s--;
    mem_write(cpu.s, cpu.x & 0xff);
    cpu.s--;
    mem_write(cpu.s, (cpu.x >> 8) & 0xff);
    cpu.s--;
    mem_write(cpu.s, cpu.dp);
    cpu.s--;
    mem_write(cpu.s, cpu.b);
    cpu.s--;
    mem_write(cpu.s, cpu.a);
    cpu.s--;
    mem_write(cpu.s, get_cc());

    switch ( swi_id )
    {
        case 1:
            cc.i = CC_FLAG_SET;
            cc.f = CC_FLAG_SET;
            cpu.pc = (mem_read(VEC_SWI) << 8) + mem_read(VEC_SWI+1);
            break;

        case 2:
            cpu.pc = (mem_read(VEC_SWI2) << 8) + mem_read(VEC_SWI2+1);
            break;

        case 3:
            cpu.pc = (mem_read(VEC_SWI3) << 8) + mem_read(VEC_SWI3+1);
            break;

        default:
            /* Exception: Illegal SWI type swi()
             */
            cpu.cpu_state = CPU_EXCEPTION;
    }
}

/*------------------------------------------------
 * tfr()
 *
 *  Transfer value from source register to destination register
 *
 *  NOTE: The function relies on the assembler to not mix
 *  8-bit registers with 16-bit register, otherwise
 *  results are unexpected.
 *  Check: if (((regs ^ (regs << 4)) & 0x80) == 0) {...}
 *
 */
void tfr(uint8_t regs)
{
    int         src, dst;
    uint16_t    temp1;

    src = (int)((regs >> 4) & 0x0f);
    dst = (int)(regs & 0x0f);

    temp1 = read_register(src);
    write_register(dst, temp1);
}

/*------------------------------------------------
 * tst()
 *
 *  Test 8 bit operand and set V,Z,N flags.
 *
 */
void inline __attribute__((always_inline)) tst(uint8_t byte)
{
    eval_cc_z((uint16_t) byte);
    eval_cc_n((uint16_t) byte);
    cc.v = CC_FLAG_CLR;
}

/*------------------------------------------------
 * do_branch()
 *
 *  Helper function to do the actual branch
 *  by stacking the PC and changing to the target address.
 *
 *  param:  Long ('1') or short ('0') branch, 16-bit sign-extended offset,
 *          pointer to opcode cycles.
 *  return: Nothing
 */
void inline __attribute__((always_inline)) do_branch(int long_short, uint16_t effective_address)
{
    cpu.pc = effective_address;
    cycles_this_scanline += long_short;
}


/*------------------------------------------------
 * branch()
 *
 *  Implement conditional short branch and long branch.
 *  The branch opcodes for both short and long variants are
 *  identical except for the 0x10 byte prefix for long branches.
 *  Calling code resolves long from short and then calls this function
 *  to resolve branch condition and apply the offset.
 *  To use this function with short branches (8-bit signed offset),
 *  the branch offset must be sign-extended to 16-bit.
 *
 *  param:  Branch opcode, long ('1') or short ('0') branch, 16-bit sign-extended offset,
 *          pointer to opcode cycles.
 *  return: Nothing
 */
void inline __attribute__((always_inline)) branch(int instruction, int long_short, uint16_t effective_address)
{
    /* Parse the branch condition and apply
       offset if branch is taken.
     */
    switch ( instruction )
    {
        /* BHI / LBHI
         */
        case 0x22:
            if ( cc.c == CC_FLAG_CLR && cc.z == CC_FLAG_CLR )
                do_branch(long_short, effective_address);
            break;

        /* BLS / LBLS
         */
        case 0x23:
            if ( cc.c == CC_FLAG_SET || cc.z == CC_FLAG_SET )
                do_branch(long_short, effective_address);
            break;

        /* BHS / LBHS / BCC / LBCC
         */
        case 0x24:
            if ( cc.c == CC_FLAG_CLR )
                do_branch(long_short, effective_address);
            break;

        /* BLO / LBLO / BCS / LBCS
         */
        case 0x25:
            if ( cc.c == CC_FLAG_SET )
                do_branch(long_short, effective_address);
            break;

        /* BNE / LBNE
         */
        case 0x26:
            if ( cc.z == CC_FLAG_CLR )
                do_branch(long_short, effective_address);
            break;

        /* BEQ / LBEQ
         */
        case 0x27:
            if ( cc.z == CC_FLAG_SET )
                do_branch(long_short, effective_address);
            break;

        /* BVC / LBVC
         */
        case 0x28:
            if ( cc.v == CC_FLAG_CLR )
                do_branch(long_short, effective_address);
            break;

        /* BVS / LBVS
         */
        case 0x29:
            if ( cc.v == CC_FLAG_SET )
                do_branch(long_short, effective_address);
            break;

        /* BPL / LBPL
         */
        case 0x2a:
            if ( cc.n == CC_FLAG_CLR )
                do_branch(long_short, effective_address);
            break;

        /* BMI / LBMI
         */
        case 0x2b:
            if ( cc.n == CC_FLAG_SET )
                do_branch(long_short, effective_address);
            break;

        /* BGE / LBGE
         */
        case 0x2c:
            if ( cc.n == cc.v )
                do_branch(long_short, effective_address);
            break;

        /* BLT / LBLT
         */
        case 0x2d:
            if ( cc.n != cc.v )
                do_branch(long_short, effective_address);
            break;

        /* BGT / LBGT
         */
        case 0x2e:
            if ( cc.n == cc.v && cc.z == CC_FLAG_CLR )
                do_branch(long_short, effective_address);
            break;

        /* BLE / LBLE
         */
        case 0x2f:
            if ( cc.n != cc.v || cc.z == CC_FLAG_SET )
                do_branch(long_short, effective_address);
            break;

        /* Exception: Illegal branch code branch()
         *
         * There should be no exception here because this function is always
         * called from within a switch/case for a valid opcode range.
         */
        default:
            cpu.cpu_state = CPU_EXCEPTION;
    }
}

/*------------------------------------------------
 * get_eff_addr()
 *
 *  Calculate and return effective address.
 *  Resolve addressing mode, calculate effective address.
 *  Modifies 'pc' and appropriate index register.
 *
 *  param:  Command op code and command cycles and bytes count to update if needed.
 *  return: Effective Address, '0' if error
 */
inline __attribute__((always_inline)) int get_eff_addr(int mode)
{
    uint16_t    operand;
    uint16_t    effective_addr = 0;

    switch ( mode )
    {
        case ADDR_DIRECT:
            return ((cpu.dp << 8) + mem_read_pc(cpu.pc++));
            break;

        case ADDR_RELATIVE:
            operand = mem_read_pc(cpu.pc++);
            return (cpu.pc + SIG_EXTEND(operand));
            break;

        case ADDR_LRELATIVE:
            operand = (mem_read_pc(cpu.pc++) << 8);
            operand += mem_read_pc(cpu.pc++);
            return (cpu.pc + operand);
            break;

        case ADDR_INDEXED:
            uint16_t   *index_reg = 0;
            operand = mem_read_pc(cpu.pc++);

            switch ( operand & INDX_POST_REG )
            {
                case 0x00:
                    index_reg = &cpu.x;
                    break;

                case 0x20:
                    index_reg = &cpu.y;
                    break;

                case 0x40:
                    index_reg = &cpu.u;
                    break;

                case 0x60:
                    index_reg = &cpu.s;
                    break;
            }

            if ( index_reg == 0 )
            {
                cpu.cpu_state = CPU_EXCEPTION;
                break;
            }

            /* Check if 5-bit offset is in the post-byte
             * then process more index address bytes if not.
             */
            if ( operand & INDX_POST_5BIT_OFF )
            {
                switch ( operand & INDX_POST_MODE )
                {
                    case 0: // EA = ,index+ Auto post-increment by 1
                        effective_addr = *index_reg;
                        (*index_reg) += 1;
                        cycles_this_scanline += 2;
                        break;

                    case 1: // EA = ,index++ Auto post-increment by 2
                        effective_addr = *index_reg;
                        (*index_reg) += 2;
                        cycles_this_scanline += (operand & INDX_POST_INDIRECT) ? 6 : 3;
                        break;

                    case 2: // EA = ,-index Auto pre-decrement by 1
                        (*index_reg) -= 1;
                        effective_addr = *index_reg;
                        cycles_this_scanline += 2;
                        break;

                    case 3: // EA = ,--index Auto pre-decrement by 2
                        (*index_reg) -= 2;
                        effective_addr = *index_reg;
                        cycles_this_scanline += (operand & INDX_POST_INDIRECT) ? 6 : 3;
                        break;

                    case 4: // EA = 0,index Zero offset
                        effective_addr = *index_reg;
                        cycles_this_scanline += (operand & INDX_POST_INDIRECT) ? 3 : 0;
                        break;

                    case 5: // EA = B,index Acc-B with index
                        effective_addr = *index_reg + SIG_EXTEND(cpu.b);
                        cycles_this_scanline += (operand & INDX_POST_INDIRECT) ? 4 : 1;
                        break;

                    case 6: // EA = A,index Acc-A with index
                        effective_addr = *index_reg + SIG_EXTEND(cpu.a);
                        cycles_this_scanline += (operand & INDX_POST_INDIRECT) ? 4 : 1;
                        break;

                    case 8: // EA = 8-bit,index 8-bit offset
                        effective_addr = SIG_EXTEND(mem_read(cpu.pc));
                        cpu.pc++;
                        effective_addr += *index_reg;
                        cycles_this_scanline += (operand & INDX_POST_INDIRECT) ? 4 : 1;
                        break;

                    case 9: // EA = 16-bit,index 16-bit offset
                        effective_addr = (mem_read(cpu.pc) << 8);
                        cpu.pc++;
                        effective_addr += mem_read(cpu.pc);
                        cpu.pc++;
                        effective_addr += *index_reg;
                        cycles_this_scanline += (operand & INDX_POST_INDIRECT) ? 7 : 4;
                        break;

                    case 11: // EA = D,index Acc-D with index
                        effective_addr = *index_reg + d;
                        cycles_this_scanline += (operand & INDX_POST_INDIRECT) ? 7 : 4;
                        break;

                    case 12: // EA = 8-bit,pc PC relative
                        effective_addr = SIG_EXTEND(mem_read(cpu.pc));
                        cpu.pc++;
                        effective_addr += cpu.pc;
                        cycles_this_scanline += (operand & INDX_POST_INDIRECT) ? 4 : 1;
                        break;

                    case 13: // EA = 16-bit,pc PC relative
                        effective_addr = (mem_read(cpu.pc) << 8);
                        cpu.pc++;
                        effective_addr += mem_read(cpu.pc);
                        cpu.pc++;
                        effective_addr += cpu.pc;
                        cycles_this_scanline += (operand & INDX_POST_INDIRECT) ? 8 : 5;
                        break;

                    case 15: // EA = [addr] Extended Indirect will always be indirect.
                        effective_addr = (mem_read(cpu.pc) << 8);
                        cpu.pc++;
                        effective_addr += mem_read(cpu.pc);
                        cpu.pc++;
                        cycles_this_scanline += 5;
                        break;

                    default:
                        /* Exception: Illegal indexing mode get_eff_addr()
                         */
                        cpu.cpu_state = CPU_EXCEPTION;
                }

                /* Resolve indirect addresses
                 * Rely on assembler-generated code to reliably include the indirect bit
                 * i.e. not for auto inc/dec by one.
                 */
                if ( operand & INDX_POST_INDIRECT )
                {
                    effective_addr = (mem_read(effective_addr) << 8) + mem_read(effective_addr + 1);
                }
            }
            /* 5-bit offset is in the post-bytes
             */
            else
            {
                operand &= 0x001f;
                if ( operand & 0x0010 )
                    operand |= 0xfff0;  // Extend the sign of the 5-bit offset into 16-bit
                effective_addr = *index_reg + operand;
                cycles_this_scanline++;
            }
            break;

        case ADDR_EXTENDED:
            effective_addr = (mem_read_pc(cpu.pc++) << 8);
            effective_addr += mem_read_pc(cpu.pc++);
            break;

        case ADDR_IMMEDIATE:
            effective_addr = cpu.pc;
            cpu.pc += 1;
            break;

        case ADDR_LIMMEDIATE:
            effective_addr = cpu.pc;
            cpu.pc += 2;
            break;

        case ADDR_INHERENT:
            break;

        default:
            break;
    }

    return effective_addr;
}

/*------------------------------------------------
 * read_register()
 *
 *  Return value of a register.
 *  Register number is as defined for EXG and TFR op-codes.
 *
 *  param:  Register number
 *  return: Register content as uint16_t for all registers
 */
inline __attribute__((always_inline)) uint16_t read_register(int reg)
{
    uint16_t    temp;

    switch ( reg )
    {
        case 0:
            temp = d;
            break;

        case 1:
            temp = cpu.x;
            break;

        case 2:
            temp = cpu.y;
            break;

        case 3:
            temp = cpu.u;
            break;

        case 4:
            temp = cpu.s;
            break;

        case 5:
            temp = cpu.pc;
            break;

        case 8:
            temp = cpu.a;
            break;

        case 9:
            temp = cpu.b;
            break;

        case 10:
            temp = (uint16_t) get_cc();
            break;

        case 11:
            temp = cpu.dp;
            break;

        default:
            temp = 0;
            /* Exception: Illegal register read_register()
             */
            cpu.cpu_state = CPU_EXCEPTION;
    }

    return temp;
}

/*------------------------------------------------
 * write_register()
 *
 *  Write value to a register.
 *  Register number is as defined for EXG and TFR op-codes.
 *
 *  param:  Register number and data to write into it.
 *  return: Nothing
 */
void inline __attribute__((always_inline)) write_register(int reg, uint16_t data)
{
    switch ( reg )
    {
        case 0:
            cpu.a = (uint8_t)((data & 0xff00) >> 8);
            cpu.b = (uint8_t)(data & 0x00ff);
            break;

        case 1:
            cpu.x = data;
            break;

        case 2:
            cpu.y = data;
            break;

        case 3:
            cpu.u = data;
            break;

        case 4:
            cpu.s = data;
            cpu.nmi_armed = 1;
            break;

        case 5:
            cpu.pc = data;
            break;

        case 8:
            cpu.a = (uint8_t)(data & 0x00ff);
            break;

        case 9:
            cpu.b = (uint8_t)(data & 0x00ff);
            break;

        case 10:
            set_cc((uint8_t) data);
            break;

        case 11:
            cpu.dp = (uint8_t)(data & 0x00ff);
            break;

        default:
            /* Exception: Illegal register write_register()
             */
            cpu.cpu_state = CPU_EXCEPTION;
    }
}

/*------------------------------------------------
 * eval_cc_c()
 *
 *  Evaluate carry bit of 8-bit input value and set/clear CC.C flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_c(uint16_t value)
{
    cc.c = (value & 0x100) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_c16()
 *
 *  Evaluate carry bit of 16-bit input value and set/clear CC.C flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_c16(uint32_t value)
{
    cc.c = (value & 0x00010000) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_z()
 *
 *  Evaluate zero value of input and set/clear CC.Z flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_z(uint16_t value)
{
    cc.z = !(value & 0x00ff) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_z16()
 *
 *  Evaluate zero value of input and set/clear CC.Z flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_z16(uint32_t value)
{
    cc.z = !(value & 0x0000ffff) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_n()
 *
 *  Evaluate sign bit value of input and set/clear CC.N flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_n(uint16_t value)
{
    cc.n = (value & 0x0080) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_n16()
 *
 *  Evaluate sign bit value of input and set/clear CC.N flag.
 *
 *  param:  Input value
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_n16(uint32_t value)
{
    cc.n = (value & 0x00008000) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_v()
 *
 *  Evaluate overflow bit value of input and set/clear CC.V flag.
 *  Use the C(in) != C(out) method, note the C(out) shift to align the bit location
 *  for a bit-wise XOR.
 *  source: http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
 *  Ken Shirriff: https://www.righto.com/2012/12/
 *
 *  param:  Input operands and result
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_v(uint8_t val1, uint8_t val2, uint16_t result)
{
    cc.v = ((val1 ^ result) & (val2 ^ result) & 0x0080) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_v16()
 *
 *  Evaluate overflow bit value of input and set/clear CC.V flag.
 *  Use the C(in) != C(out) method, note the C(out) shift to align the bit location
 *  for a bit-wise XOR.
 *  source: http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
 *  Ken Shirriff: https://www.righto.com/2012/12/
 *
 *  param:  Input operands and result
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_v16(uint16_t val1, uint16_t val2, uint32_t result)
{
    cc.v = ((val1 ^ result) & (val2 ^ result) & 0x00008000) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * eval_cc_h()
 *
 *  Evaluate half carry bit and set/clear CC.H flag.
 *  source: https://retrocomputing.stackexchange.com/questions/11262/can-someone-explain-this-algorithm-used-to-compute-the-auxiliary-carry-flag
 *
 *  param:  Input operands and result
 *  return: Nothing
 */
inline __attribute__((always_inline)) void eval_cc_h(uint8_t val1, uint8_t val2, uint8_t result)
{
    /* Half carry in 6809 is only relevant/valid for additions ADD and ADC
     */
    cc.h = (((val1 ^ val2) ^ result) & 0x10) ? CC_FLAG_SET : CC_FLAG_CLR;
}

/*------------------------------------------------
 * get_cc()
 *
 *  Return value of CC register as a packed 8-bit value
 *
 *  param:  Nothing
 *  return: 8-bit value of CC register
 */
inline __attribute__((always_inline)) uint8_t get_cc(void)
{
    return (uint8_t) ((cc.e << 7) + (cc.f << 6) + (cc.h << 5) + (cc.i << 4) + \
                      (cc.n << 3) + (cc.z << 2) + (cc.v << 1) + cc.c );
}

/*------------------------------------------------
 * set_cc()
 *
 *  Set value of CC register from a packed 8-bit value
 *
 *  param:  8-bit value of CC register
 *  return: Nothing
 */
inline void set_cc(uint8_t value)
{
    cc.c = (value & 0x01) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.v = (value & 0x02) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.z = (value & 0x04) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.n = (value & 0x08) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.i = (value & 0x10) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.h = (value & 0x20) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.f = (value & 0x40) ? CC_FLAG_SET : CC_FLAG_CLR;
    cc.e = (value & 0x80) ? CC_FLAG_SET : CC_FLAG_CLR;
}
