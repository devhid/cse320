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

#include "hw1.h"
#include "debug.h"
#include "strlib.h"

#define BUFFER_SIZE 120

int main(int argc, char **argv)
{
    if(!validargs(argc, argv))
        USAGE(*argv, EXIT_FAILURE);
    debug("Options: 0x%X", global_options);
    if(global_options & 0x1) {
        USAGE(*argv, EXIT_SUCCESS);
    }

    // Input buffer for encode.
    char buffer[BUFFER_SIZE];

    // Input buffer for decode.
    int word = 0;

    // Address of instruction.
    unsigned int addr = global_options & 0xFFFFF000;

    // Grab the second lsb and check if it's 0 or 1 (assemble / disassemble).
    switch((global_options & 0x00000002) >> 1) {
        case 0:
            while(fgets(buffer, sizeof(buffer), stdin) != NULL) {

                int scan_value = 0;

                // Create an empty instruction to fill out.
                Instruction in = {0};

                // Loop through Instr_info table to get the values for that instruction.
                for(int i = 0; i < 64; i++) {
                    scan_value = sscanf(buffer, (&instrTable[i])->format, &(in.args[0]), &(in.args[1]), &(in.args[2]));

                    if(equals_n(buffer, (&instrTable[i])->format) || scan_value) {
                        in.info = &instrTable[i];

                        int arg_count = 0;
                        for(int j = 0; j < 3; j++) {
                            switch(in.info->srcs[j]) {
                                case RS:
                                    if(in.args[j] < 0 || in.args[j] > 31) {
                                        return EXIT_FAILURE;
                                    }

                                    in.regs[j] = in.args[j];
                                    arg_count++;
                                    break;
                                case RT:
                                    if(in.args[j] < 0 || in.args[j] > 31) {
                                        return EXIT_FAILURE;
                                    }

                                    in.regs[j] = in.args[j];
                                    arg_count++;
                                    break;
                                case RD:
                                    if(in.args[j] < 0 || in.args[j] > 31) {
                                        return EXIT_FAILURE;
                                    }

                                    in.regs[j] = in.args[j];
                                    arg_count++;
                                    break;
                                case EXTRA:
                                    in.extra = in.args[j];
                                    arg_count++;
                                    break;
                                default:
                                    continue;
                            }
                        }

                        // Checks whether the number of matched arguments equals the number of arguments the
                        // the instruction requires.
                        if(scan_value != arg_count && scan_value != -1) {
                            return EXIT_FAILURE;
                        }

                        break;
                    }
                }


                // If the instruction was not found, then it's an error so return 0.
                if(in.info == NULL) {
                    return EXIT_FAILURE;
                }

                if(encode(&in, addr)) {
                    int value = endian(global_options, in.value);

                    fwrite(&value, sizeof(int), 1, stdout);
                } else {
                    return EXIT_FAILURE;
                }

                // Increment address because of a new instruction.
                addr += 4;
            }

            break;
        case 1:
            while(fread(&word, sizeof(int), 1, stdin) > 0) {
                // Need to make a Instruction struct to pass in as parameter for decode().
                Instruction in = {0};
                in.value = endian(global_options, word);

                if(decode(&in, addr)) {
                    printf(in.info->format, in.args[0], in.args[1], in.args[2]);
                    printf("%s", "\n");
                } else {
                    return EXIT_FAILURE;
                }

                // Increment address because of a new instruction.
                addr +=4;
            }

            break;
        default:
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
