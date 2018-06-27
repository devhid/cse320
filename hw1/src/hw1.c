#include "hw1.h"
#include "strlib.h"
#include <stdlib.h>

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 */

/**
 * @brief Returns the little or big endian byte order of the given value.
 * The byte order is based on the third least significant bit of the global_options integer.
 *
 * @return The big-endian byte order of value if third lsb of global_options is 1, little endian otherwise.
**/
int endian(int options, int value) {
    switch((options & 0x00000004) >> 2) {
        case 1:
            return (
            ((value >> 24) & 0x000000FF) |
            ((value >> 8)  & 0x0000FF00) |
            ((value << 8)  & 0x00FF0000) |
            ((value << 24) & 0xFF000000));
        default:
            return value;
    }
}

/**
 * @brief Sign extends the given value.
 * This function will sign extend a 16-bit integer into a 32-bit signed integer.
 * The 16 most significant bits will be 1 if the 16th least significant bit is 1, 0 otherwise.
 *
 * @return The sign-extended version of the given integer.
**/
int sign_extend(int value) {
    int temp = value & 0x0000FFFF; // Get the 16-bit unsigned version.
    int mask = 0x00008000; // Get the sign bit.

    // If the sign bit = 1, then OR with 0xFFFF0000, else just leave it.
    return (mask & temp) ? temp | 0xFFFF0000 : temp;
}

/**
 * @brief Validates the given address.
 * An address is valid if it is divisible by 4096 and is at most 8 characters in length.
 * In addition, the address must also only contain characters [0-9] U [A-F] U [a-f]
 *
 * @return 1 if the address is valid, 0 if not.
**/
int validate_address(char *address) {
    // Check if the base address is at least 1 character
    // and at most 8 characters in length.
    if(length(address) < 1 || length(address) > 8) {
        return 0;
    }

    for(int i = 0; i < length(address); i++) {
        // Check if the input is between 0 and 9.
        if(address[i] >= '0' && address[i] <= '9') {
            continue;
        }

        if(address[i] >= 'A' && address[i] <= 'F') {
            continue;
        }

        if(address[i] >= 'a' && address[i] <= 'f') {
            continue;
        }

        return 0;
    }

    unsigned int multiple = 15;
    char *ptr;

    // Check if it's a multiple of 4096.
    return (strtol(address, &ptr, 16) % 4096) == 0;
    /*return
        *address == 0 || (
        (address[length(address) - 1] & multiple) == 0 &&
        (address[length(address) - 2] & multiple) == 0 &&
        (address[length(address) - 3] & multiple) == 0);*/
}

/**
 * @brief Validates the argument specified for the -e flag.
 * Endianness can only be big or little specified by characters, 'b' and 'l'.
 * This function will check if the argument is one character in length
 * and is either of those letters.
 *
 * @param type The argument that is specified after the -e flag.
 * @return 1 if the argument of length 1 and is either 'b' or 'l' and 0 if not.
**/
int validate_endian(char *type) {
    if(length(type) != 1) {
        return 0;
    }

    if(*type != 'b' && *type != 'l') {
        return 0;
    }

    return 1;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 1 if validation succeeds and 0 if validation fails.
 * Upon successful return, the selected program options will be set in the
 * global variable "global_options", where they will be accessible
 * elsewhere in the program.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 1 if validation succeeds and 0 if validation fails.
 * Refer to the homework document for the effects of this function on
 * global variables.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected options.
 */
int validargs(int argc, char **argv)
{
    char h_flag[3] = "-h";
    char a_flag[3] = "-a";
    char d_flag[3] = "-d";
    char e_flag[3] = "-e";
    char b_flag[3] = "-b";

    int pos;
    int num_args;

    num_args = argc - 1;

    //+ If no flags are provided, return failure and display usage.
    if(num_args == 0) {
        return 0;
    }

    //+ Check if the -h flag is there and in the first position.
    for(pos = 1; pos < argc; pos++) {
        if(equals(argv[pos], h_flag)) {
            if(pos == 1) {
                global_options = 0x1;
                return 1;
            }
            return 0;
        }
    }

    //+ Make sure the first positional argument is -a or -d.
    if(!(equals(argv[1], a_flag) || equals(argv[1], d_flag))) {
        return 0;
    }

    //+ At this point, if they only specify the -a or -d flag,
    // then the input is valid.
    if(num_args == 1) {
        if(equals(argv[1], a_flag)) {
            global_options = 0x0;
        } else {
            global_options = 0x2;
        }

        return 1;
    }

    //+ Regardless of whether we have -a or -d,
    // If the number of arguments is not between [1,5] and even, input is invalid.
    if(num_args != 3 && num_args != 5) {
        return 0;
    }

    //+ Handle assembly mode flags.
    if(equals(argv[1], a_flag)) {
        // Make sure that -a and -d are not used in combination.
        for(pos = 2; pos < argc; pos++) {
            if(equals(argv[pos], d_flag)) {
                return 0;
            }
        }

        global_options = 0x0;
    }

     //+ Handle disassembly mode flags.
    if(equals(argv[1], d_flag)) {
        // Make sure that -a and -d are not used in combination.
        for(pos = 2; pos < argc; pos++) {
            if(equals(argv[pos], a_flag)) {
                return 0;
            }
        }

        global_options = 0x2;
    }

    //+ If there are three arguments, make sure the 3rd argument
    // is either an "-b" or a "-e".
    if(num_args == 3) {
        if(!(equals(argv[2], b_flag) || equals(argv[2], e_flag))) {
            return 0;
        }
    }

    // If there are five arguments, make sure...
    if(num_args == 5) {
        // The 3rd argument is either a "-b" or "-e".
        if(!(equals(argv[2], b_flag) || equals(argv[2], e_flag))) {
            return 0;
        }

        // The 5th argument is either a "-b" or "-e".
        else if(!(equals(argv[4], b_flag) || equals(argv[4], e_flag))) {
            return 0;
        }

        // If the 3rd argument is a "-b", the 5th argument should be "-e".
        else if(equals(argv[2], b_flag) && !equals(argv[4], e_flag)) {
            return 0;
        }

        // If the 3rd argument is a "-e", the 5th argument should be "-b".
        else if(equals(argv[2], e_flag) && !equals(argv[4], b_flag)) {
            return 0;
        }
    }

    // Validate the optional parameters.
    if(num_args == 3) {
        if(equals(argv[2], b_flag)) {
            if(!validate_address(argv[3])) {
                return 0;
            }

            char *ptr;

            global_options = global_options | strtol(argv[3], &ptr, 16);

            return 1;
        }

        else if(equals(argv[2], e_flag)) {
            if(!validate_endian(argv[3])) {
                return 0;
            }

            // Change 3rd sb of global_options to 1 if user
            // wants big-endian.
            if(*argv[3] == 'b') {
                global_options = global_options | 4;
            }

            return 1;
        }

        else { return 0; }
    }

    if(num_args == 5) {
        if(equals(argv[2], b_flag)) {
            if(!validate_address(argv[3])) {
                return 0;
            }

            if(!validate_endian(argv[5])) {
                return 0;
            }

            if(*argv[5] == 'b') {
                global_options = global_options | 4;
            }

            char *ptr;

            global_options = global_options | strtol(argv[3], &ptr, 16);

            return 1;
        }

        else if(equals(argv[4], b_flag)) {
            if(!validate_address(argv[5])) {
                 return 0;
            }

            if(!validate_endian(argv[3])) {
                return 0;
            }

            if(*argv[3] == 'b') {
                global_options = global_options | 4;
            }

            char *ptr;

            global_options = global_options | strtol(argv[5], &ptr, 16);

            return 1;
        }

        else if(equals(argv[2], e_flag)) {
            if(!validate_endian(argv[3])) {
                return 0;
            }

            if(!validate_address(argv[5])) {
                 return 0;
            }

            if(*argv[3] == 'b') {
                global_options = global_options | 4;
            }

            char *ptr;

            global_options = global_options | strtol(argv[5], &ptr, 16);

            return 1;
        }

        else if(equals(argv[4], e_flag)) {
            if(!validate_endian(argv[5])) {
                return 0;
            }

            if(!validate_address(argv[3])) {
                return 0;
            }

            if(*argv[5] == 'b') {
                global_options = global_options | 4;
            }

            char *ptr;

            global_options = global_options | strtol(argv[3], &ptr, 16);

            return 1;
        }

        else { return 0; }
    }

    return 1;
}

/**
 * @brief Computes the binary code for a MIPS machine instruction.
 * @details This function takes a pointer to an Instruction structure
 * that contains information defining a MIPS machine instruction and
 * computes the binary code for that instruction.  The code is returne
 * in the "value" field of the Instruction structure.
 *
 * @param ip The Instruction structure containing information about the
 * instruction, except for the "value" field.
 * @param addr Address at which the instruction is to appear in memory.
 * The address is used to compute the PC-relative offsets used in branch
 * instructions.
 * @return 1 if the instruction was successfully encoded, 0 otherwise.
 * @modifies the "value" field of the Instruction structure to contain the
 * binary code for the instruction.
 */
int encode(Instruction *ip, unsigned int addr) {
    // The value that will found from encoding the instruction.
    int value = 0x00000000;

    // Get the info struct for the instruction.
    Instr_info *info = ip->info;

    // Get information about the instruction like its opcode, type and sources.
    Opcode opcode = info->opcode;
    Type type     = info->type;
    Source *srcs  = info->srcs;

    // Get the instructions register values, arguments, and extra field.
    char *regs = ip->regs;
    int *args  = ip->args;
    int extra  = ip->extra;

    // If the type is NTYP, return 0 because it isn't an instruction.
    if(type == NTYP) {
        return 0;
    }

    // Set default binary opcode to be 0 (SPECIAL).
    unsigned int binary_opcode = 0;

    // Check for BCOND opcodes.
    if(opcode == OP_BLTZ || opcode == OP_BGEZ || opcode == OP_BLTZAL || opcode == OP_BGEZAL) {
        binary_opcode = 1;
    } else {
        // Check if it's in regular op code.
        for(int i = 0; i < 64; i++) {
            if(opcodeTable[i] == opcode) {
                binary_opcode = i;
            }
        }
    }

    value = (value | (binary_opcode & 0x0000003F)) << 26;

    // Declare other value bits to fill.
    unsigned int v5_0   = 0;
    unsigned int v25_6 = 0;
    unsigned int v10_6 = 0;
    unsigned int v25_0 = 0;
    unsigned int v15_0 = 0;

    // Fill in value for bits 5:0.
    if(binary_opcode == 0) {
        for(int i = 0; i < 64; i++) {
            if(specialTable[i] == opcode) {
                v5_0 = i;

                value = value | v5_0;
                break;
            }
        }
    }

    for(int i = 0; i < 3; i++) {
        if(srcs[i] == EXTRA) {

            if(opcode == OP_BREAK) {
                v25_6 = args[i];

                value = value | ( (0x00000000 | v25_6) << 6 );
            } else {
                switch(type) {
                    case RTYP:
                        v10_6 = args[i];

                        value = value | ( (0x00000000 | v10_6) << 6 );
                        break;
                    case ITYP:
                        v15_0 = extra;
                        if(opcode == OP_BEQ || opcode == OP_BGEZ || opcode == OP_BGEZAL || opcode == OP_BGTZ
                            || opcode == OP_BLEZ || opcode == OP_BLTZ || opcode == OP_BLTZAL || opcode == OP_BNE) {

                            v15_0 = (v15_0 - addr - 4) >> 2;
                        }

                        value = value | (v15_0 & 0xFFFF); // Undo the sign extension with bit masking.

                        // Handle bits 20:16 for special BCOND instructions.
                        switch(opcode) {
                            case OP_BLTZ:
                                value = value | ( (0x00000000 | 0) << 16);
                                break;
                            case OP_BGEZ:
                                value = value | ( (0x00000000 | 1) << 16);
                                break;
                            case OP_BLTZAL:
                                value = value | ( (0x00000000 | 16) << 16);
                                break;
                            case OP_BGEZAL:
                                value = value | ( (0x00000000 | 17) << 16);
                            default:
                                break;
                        }

                        break;
                    case JTYP:
                        // Check if the 4 MSB of addr are equal to 4 MSB of Jump Address.
                        // If not, return error.
                        if( (addr & 0xF0000000) != (extra & 0xF0000000) ) {
                            return 0;
                        }

                        v25_0 = (extra - ((addr - 4) & 0xF0000000)) >> 2;

                        value = value | (v25_0 & 0x2FFFFFF);
                        break;
                    default:
                        return 0;
                }
            }
        }

        else if(srcs[i] == RS) {
            value = value | ( (0x00000000 | args[i]) << 21 );
        }

        else if(srcs[i] == RT) {
            value = value | ( (0x00000000 | args[i]) << 16 );
        }

        else if(srcs[i] == RD) {
            value = value | ( (0x00000000 | args[i]) << 11 );
        }

        else {
            continue;
        }
    }

    ip->value = value;

    return 1;
}

/**
 * @brief Decodes the binary code for a MIPS machine instruction.
 * @details This function takes a pointer to an Instruction structure
 * whose "value" field has been initialized to the binary code for
 * MIPS machine instruction and it decodes the instruction to obtain
 * details about the type of instruction and its arguments.
 * The decoded information is returned by setting the other fields
 * of the Instruction structure.
 *
 * @param ip The Instruction structure containing the binary code for
 * a MIPS instruction in its "value" field.
 * @param addr Address at which the instruction appears in memory.
 * The address is used to compute absolute branch addresses from the
 * the PC-relative offsets that occur in the instruction.
 * @return 1 if the instruction was successfully decoded, 0 otherwise.
 * @modifies the fields other than the "value" field to contain the
 * decoded information about the instruction.
 */
int decode(Instruction *ip, unsigned int addr) {
    // Integers to represent bits for Opcode, RS, RT, RD, and Function
    unsigned int v31_26 = 0xFC000000; // XXXX XX00 0000 0000 0000 0000 0000 0000 // Opcode (26)
    unsigned int v25_21 = 0x03E00000; // 0000 00XX XXX0 0000 0000 0000 0000 0000 // RS (21)
    unsigned int v20_16 = 0x001F0000; // 0000 0000 000X XXXX 0000 0000 0000 0000 // RT (16)
    unsigned int v15_11 = 0x0000F800; // 0000 0000 0000 0000 XXXX X000 0000 0000 // RD (11)
    unsigned int v5_0   = 0x0000003F; // 0000 0000 0000 0000 0000 0000 00XX XXXX // Function

    // EXTRA bits
    unsigned int v25_6  = 0x03FFFFC0; // 0000 00XX XXXX XXXX XXXX XXXX XX00 0000
    unsigned int v10_6  = 0x000007C0; // 0000 0000 0000 0000 0000 0XXX XX00 0000
    unsigned int v15_0  = 0x0000FFFF; // 0000 0000 0000 0000 XXXX XXXX XXXX XXXX
    unsigned int v25_0  = 0x03FFFFFF; // 0000 00XX XXXX XXXX XXXX XXXX XXXX XXXX

    // Grab the binary form of the instruction.
    unsigned int value = ip->value;

    // Grab the 6-digit opcode from the value.
    // We shift the value 26 bits to the right to get the opcode.
    // Example: 1111 1100 0000 0000 0000 0000 0000 0000 >> 26 = 0x111111
    unsigned int binary_opcode = (value & v31_26) >> 26;

    // Get the Opcode type from the table using the actual opcode bits.
    Opcode opcode = opcodeTable[binary_opcode];

    // Handle opcode on a case by case basis.
    switch(opcode) {
        case ILLEGL:
            return 0;
        // If the opcode is of type SPECIAL, look at bits 5:0 and use that as index for specialTable.
        // Store the opcode from specialTable as the opcode.
        case SPECIAL:
            binary_opcode = (value & v5_0);
            opcode = specialTable[binary_opcode];

            ip->info = &instrTable[opcode];
            ip->info->opcode = opcode;
            break;

        // If the opcode is of type BCOND, check bits 20:16 and set its opcode based on the value.
        case BCOND:
            binary_opcode = (value & v20_16) >> 16;
            switch(binary_opcode) {
                case 0:
                    opcode = OP_BLTZ;
                    break;
                case 1:
                    opcode = OP_BGEZ;
                    break;
                case 16:
                    opcode = OP_BLTZAL;
                    break;
                case 17:
                    opcode = OP_BGEZAL;
                    break;
                default:
                    return 0;
            }

            ip->info = &instrTable[opcode];
            ip->info->opcode = opcode;
            break;
        default:
            // If the opcode is none of the above types, store it regularly.
            ip->info = &instrTable[opcode];
            ip->info->opcode = opcode;

            break;
    }

    // Grab the info struct from the instruction.
    Instr_info *info = ip->info;

    // If the instruction type is NTYP, return 0 because it isn't an instruction.
    if(info->type == NTYP) {
        return 0;
    }

    // Fill out the regs array with RS, RT, and RD.
    char *regs = ip->regs;
    regs[0] = ((value & v25_21) >> 21);
    regs[1] = ((value & v20_16) >> 16);
    regs[2] = ((value & v15_11) >> 11);

    // Get the sources for the specific instruction.
    Source *srcs = info->srcs;

    // Get the args for the specific instruction.
    int *args = ip->args;

    for(int i = 0; i < 3; i++) {
        switch(srcs[i]) {
            case RS:
                args[i] = (int) regs[0];
                break;
            case RT:
                args[i] = (int) regs[1];
                break;
            case RD:
                args[i] = (int) regs[2];
                break;
            case EXTRA:
                if(opcode == OP_BREAK) {
                    args[i] = ((value & v25_6) >> 6);
                }

                else if(info->type == RTYP) {
                    args[i] = ((value & v10_6) >> 6);
                }

                else if(info->type == ITYP) {
                    int signed_value = sign_extend( value & v15_0 );

                    if(opcode == OP_BEQ || opcode == OP_BGEZ || opcode == OP_BGEZAL || opcode == OP_BGTZ
                        || opcode == OP_BLEZ || opcode == OP_BLTZ || opcode == OP_BLTZAL || opcode == OP_BNE) {
                        signed_value = (signed_value << 2) + addr + 4;
                    }

                    args[i] = signed_value;
                }

                else if(info->type == JTYP) {
                    args[i] = (int) ( ( (value & v25_0) << 2 ) | ( (addr + 4) & 0xF0000000 ) );
                }

                ip->extra = args[i];

                break;
            case NSRC:
                args[i] = 0;

                break;
            default:
                break;
        }
    }

    return 1;
}
