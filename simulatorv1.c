#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <regex.h>

int registers[32];
int pc = 0;
char **instructionLines;
unsigned int *instructions;
int *data;
char **labels;

unsigned int parseInst(char* line);

unsigned int ifStage(int pCount, int *cycles, int clock);

int idStage(unsigned int instruction, int* decode, int *cycles, int clock);

int exStage(int* exec, int* decode, int *cycles, int clock);

int memStage(int* exec, int *cycles, int clock);

int wbStage(int* mem, int *cycles, int clock);

typedef enum {zero, at, v0, v1, a0, a1, a2, a3, t0, t1, t2, t3, t4, t5, t6, t7, s0, s1, s2, s3, s4, s5, s6, s7, t8, t9, k0, k1, gp, sp, fp, ra, ERR = -1} REGISTER;
	
const static struct{
	REGISTER reg;
	const char *str;
} convert [] = {
	{zero, "$zero"}, //r0
	{at, "$at"}, //r1
	{v0, "$v0"}, //r2
	{v1, "$v1"}, //r3
	{a0, "$a0"}, //r4
	{a1, "$a1"}, //r5
	{a2, "$a2"}, //r6
	{a3, "$a3"}, //r7
	{t0, "$t0"}, //r8
	{t1, "$t1"}, //r9
	{t2, "$t2"}, //r10
	{t3, "$t3"}, //r11
	{t4, "$t4"}, //r12
	{t5, "$t5"}, //r13
	{t6, "$t6"}, //r14
	{t7, "$t7"}, //r15
	{s0, "$s0"}, //r16
	{s1, "$s1"}, //r17
	{s2, "$s2"}, //r18
	{s3, "$s3"}, //r19
	{s4, "$s4"}, //r20
	{s5, "$s5"}, //r21
	{s6, "$s6"}, //r22
	{s7, "$s7"}, //r23
	{t8, "$t8"}, //r24
	{t9, "$t9"}, //r25
	{k0, "$k0"}, //r26
	{k1, "$k1"}, //r27
	{gp, "$gp"}, //r28
	{sp, "$sp"}, //r29
	{fp, "$fp"}, //r30
	{ra, "$ra"}  //r31
};

REGISTER
regToValue(char* str){
	int len = strlen(str) - 1;
	while(isspace(str[len]))
		len--;
	str[++len] = '\0';
	for(int i = 0;i < 32;i++)
		if(!strcmp (str, convert[i].str))
			return convert[i].reg;
	return ERR;
}
		
int main(int argc, char *argv[]){
	if(argc == 4){
		char *instSource = argv[1];
		char *datSource = argv[2];
		char *outSource = argv[3];
		
		FILE *instFile = fopen(instSource, "r");
		FILE *datFile = fopen(datSource, "r");
		FILE *outFile = fopen(outSource, "w");
		
		if(!instFile || !datFile || !outFile)
			perror("Could not open at least one file");
		else{
			char *datUpdateSource = "dataUpdate.txt";
			char *regSource = "registers.txt";
			
			FILE *datUpdateFile = fopen(datUpdateSource, "w");
			FILE *regFile = fopen(regSource, "w");
			if(!datUpdateFile || !regFile)
				perror("Could not open a file");
			else{
				char* line = NULL;
				size_t len = 0, bufSize = 32;
				int c = 0;
				data = malloc(bufSize * sizeof(unsigned int));
				while (getline(&line, &len, datFile) != -1){
					if(c >= bufSize){
						bufSize *= 2;
						int* temp = NULL;
						temp = realloc(data, bufSize * sizeof(unsigned int));
						if(!temp){
							perror("");
							return 0;
						}
						data = temp;
					}
					data[c] = strtol(line, NULL, 2);
					c++;
				}
				int dataSize = c;
				free(line);
				line = NULL;
				len = 0, bufSize = 32;
				c = 0;
				instructions = malloc(bufSize * sizeof(unsigned int));
				labels = malloc(bufSize * sizeof(char*));
				instructionLines = malloc(bufSize * sizeof(char*));
				for(int i = 0;i < bufSize;i++){
					labels[i] = NULL;
					instructionLines[i] = NULL;
				}
				while (getline(&line, &len, instFile) != -1){
					if(c >= bufSize){
						int oldSize = bufSize; 
						bufSize *= 2;
						unsigned int* temp = NULL;
						char **temp2 = NULL, **temp3 = NULL;
						temp = realloc(instructions, bufSize * sizeof(unsigned int));
						temp2 = realloc(labels, bufSize * sizeof(char*));
						temp3 = realloc(instructionLines, bufSize * sizeof(char*));
						if(!temp || ! temp2 || !temp3){
							perror("");
							return 0;
						}
						instructions = temp;
						labels = temp2;
						instructionLines = temp3;
						for(int i = oldSize;i < bufSize;i++){
							labels[i] = NULL;
							instructionLines[i] = NULL;
						}
					}
					char* instLine = line;
					while(isspace(instLine[0]))
						instLine++;
					int it = 0;
					while(it < strlen(instLine) && instLine[it] != ':'){
						it++;
					}
					if(instLine[it] == ':'){
						instLine[it] = '\0';
						labels[c] = malloc((it + 1) * sizeof(char));
						if(!labels[c]){
							perror("");
							return 0;
						}
						snprintf(labels[c], it + 1, "%s", instLine);
						instLine += it + 1;
					}
					while(isspace(instLine[0]))
						instLine++;
					it = 0;
					while(it < strlen(instLine) && !isspace(instLine[it]))
						it++;
					instLine[it] = '\t';
					instructionLines[c] = malloc(strlen(instLine) * sizeof(char));
					instructionLines[c] = strncpy(instructionLines[c], instLine, 												strlen(instLine) - 1);
					instructionLines[c][strlen(instLine) - 1] = '\0';
					for(int i = 0;i < strlen(instLine);i++){
						instLine[i] = tolower(instLine[i]);
					}
					instructions[c] = parseInst(instLine);
					//printf("%x\n", instructions[c]);
					c++;
				}
				int instSize = c;	
				free(line);
				
				for(int i = 0;i < 32;i++)
					registers[i] = 0;
				fprintf(outFile, "\t\t\t\t\t\t\tIF\tID\tEX\tMEM\tWB\n");
				int clock = 0, cont = 1;
				unsigned int instruction = 0;
				int decode[6];
				int exec[5];
				int mem[3];
				int instCycles[instSize][5];
				for(int i = 0;i < instSize;i++){
					for(int j = 0; j < 5;j++){
						instCycles[i][j] = 0;
					}
				}
				while(cont && clock < instSize){
					if(clock >= 4){
						wbStage(mem, instCycles[clock - 4], clock + 1);
					}
					if(clock >= 3){
						mem[0] = exec[4];
						mem[1] = memStage(exec, instCycles[clock - 3], clock + 1);
						mem[2] = exec[1];
					}
					if(clock >= 2){
						exStage(exec, decode, instCycles[clock - 2], clock + 1);
					}
					if(clock >= 1){
						cont = idStage(instruction, decode, instCycles[clock - 1], 									clock + 1);
					}
					instruction = ifStage(pc, instCycles[clock], clock + 1);
					clock++;
				}
				for(int i = 0;i < instSize;i++){
					char* label = "";
					if(labels[i])
						fprintf(outFile, "%s:", labels[i]);
					if(!labels[i] || strlen(labels[i]) < 5)
						fprintf(outFile, "\t");
					fprintf(outFile, "\t\t%s\t", instructionLines[i]);
					if(strlen(instructionLines[i]) < 20)
						fprintf(outFile, "\t");
					if(strlen(instructionLines[i]) < 11)
						fprintf(outFile, "\t");
					if(strlen(instructionLines[i]) < 5)
						fprintf(outFile, "\t");
					for(int j = 0; j < 5;j++){
						if(instCycles[i][j])
							fprintf(outFile, "%d\t", instCycles[i][j]);
					}
					fprintf(outFile, "\n");
				}
				for(int i = 0;i < dataSize;i++){
					unsigned int bin = data[i];
					for(int i = 31; i >= 0;i--)
						fprintf(datUpdateFile, "%d", ((bin>>i)%2));
					fprintf(datUpdateFile, "\n");
				}
				for(int i = 0;i < 32;i++){
					unsigned int bin = registers[i];
					fprintf(regFile, "r%d: ", i);
					for(int i = 31;i >= 0;i--)
						fprintf(regFile, "%d", ((bin>>i)%2));
					fprintf(regFile, "\n");
				}
				fclose(instFile);
				fclose(datFile);
				fclose(outFile);
				fclose(regFile);
				fclose(datUpdateFile);
				free(instructions);
				free(data);
				for(int i = 0;i < instSize;i++){
					if(labels[i]){
						free(labels[i]);
					}
					if(instructionLines[i]){
						free(instructionLines[i]);
					}
				}
				free(instructionLines);
				free(labels);
			}
		}
	}
	return 0;
}

unsigned int parseInst(char* line){	
	//logical lshift 26 bits
	unsigned int opShift = 67108864;
	//logical lshift 21 bits
	unsigned int rsShift = 2097152;
	//logical lshift 16 bits
	unsigned int rtShift = 65536;
	regex_t rFormat, iFormat;
	unsigned int t = regcomp(&rFormat, "(^add|^sub|^and|^or|^sll|^srl)",REG_EXTENDED);
	unsigned int inst = 0;
	if (t){
		perror("");
		return 0;
	}
	t = regcomp(&iFormat, "(^lw|^sw|^li|^addi|^subi|^andi|^ori)", REG_EXTENDED);
	if(t){
		perror("");
		return 0;
	}
	int iForm = regexec(&iFormat,line, 0, NULL, 0);
	regfree(&iFormat);
	unsigned int rForm = regexec(&rFormat,line, 0, NULL, 0);
	regfree(&rFormat);
	unsigned int opcode = 0;
	unsigned int rt = 0;
	unsigned int rs = 0;
	int sign = 1;
	//is i format instruction
	if(!iForm){
		unsigned short imm;
		//tokenize input line
		char *ops[4];
		//operation delimited by tab
		char *op = strtok(line, "\t");
		//store or load instruction
		if(!strcmp(op, "lw") || !strcmp(op, "sw") || !strcmp(op, "li")){
			//li
			if(!strcmp(op, "li")){
				for(int i = 0;i < 3;i++){
					if(!op)
						return 0;
					while(isspace(op[0]))
						op++;
					ops[i] = op;
					//operands delimited by ", "
					op = strtok(NULL, ",");
				}
				opcode = 8;
				rt = regToValue(ops[1]);
				int len = strlen(ops[2]) - 1;
				while(isspace(ops[2][len]))
					len--;
				int base = 10 + 6 * (ops[2][len] == 'h'); 
				imm = (short) strtol(ops[2], NULL, base);
			}
			else{
				//lw 
				if(!strcmp(op, "lw")){
					opcode = 35;
				}
				//sw
				else{
					opcode = 43;
				}
				if(!op)
						return 0;
				while(isspace(op[0]))
						op++;
				ops[0] = op;
				op = strtok(NULL, ",");
				if(!op)
						return 0;
				while(isspace(op[0]))
						op++;
				ops[1] = op;
				op = strtok(NULL, "(");
				if(!op)
						return 0;
				while(isspace(op[0]))
						op++;
				ops[3] = op;
				op = strtok(NULL, ")");
				if(!op)
						return 0;
				while(isspace(op[0]))
						op++;
				ops[2] = op;
				rs = regToValue(ops[2]);
				rt = regToValue(ops[1]);
				int len = strlen(ops[3]) - 1;
				while(isspace(ops[3][len]))
					len--;
				int base = 10 + 6 * (ops[3][strlen(ops[3]) - 1] == 'h'); 
				imm = (short) strtol(ops[3], NULL, base);
			}
		}
		//addi subi andi ori instruction
		else{
			for(int i = 0;i < 4;i++){
				if(!op)
					return 0;
				while(isspace(op[0]))
					op++;
				ops[i] = op;
				//operands delimited by ", "
				op = strtok(NULL, ",");
			}
			//addi
			if(!strcmp(ops[0], "addi"))
				opcode = 8;
			//subi
			else if(!strcmp(ops[0], "subi")){
				opcode = 8;
				sign = -1;
			}
			//andi
			else if(!strcmp(ops[0], "andi"))
				opcode = 12;
			//ori
			else
				opcode = 13;
			rs = regToValue(ops[2]);
			rt = regToValue(ops[1]);
			int len = strlen(ops[3]) - 1;
			while(isspace(ops[3][len]))
				len--;
			int base = 10 + 6 * (ops[3][len] == 'h');
			imm = (short) strtol(ops[3], NULL, base) * sign;
		}
		inst = opcode * opShift + rs * rsShift + rt *rtShift + imm;
		return inst;
	}
	//is r format instruction
	if(!rForm){
		//logical lshift 11 bits
		unsigned int rdShift = 2048;
		//logical lshift 6 bits
		unsigned int shamtShift = 64;
		//tokenize input line
		char *ops[4];
		//operation delimited by tab
		char *op = strtok(line, "\t");
		for(int i = 0;i < 4;i++){
			if(!op)
				return 0;
			while(isspace(op[0]))
				op++;
			ops[i] = op;
			//operands delimited by ", "
			op = strtok(NULL, ",");
		}
		//shift instruction
		unsigned int rd = regToValue(ops[1]);
		unsigned int shamt = 0;
		unsigned int func;
		if(!strcmp(ops[0], "sll") || !strcmp(ops[0], "srl")){
			//sll
			if(!strcmp(ops[0], "sll"))
				func = 0;
			//srl
			else
				func = 2;
			rt = regToValue(ops[2]);
			shamt = regToValue(ops[3]);
		}
		else{
			//add
			if(!strcmp(ops[0], "add"))
				func = 32;
			//sub
			else if(!strcmp(ops[0], "sub"))
				func = 34;
			//and
			else if(!strcmp(ops[0], "and"))
				func = 36;
			//or
			else
				func = 37;
			rs = regToValue(ops[2]);
			rt = regToValue(ops[3]);
		}
		inst = opcode * opShift + rs * rsShift + rt * rtShift 
				+ rd * rdShift + shamt * shamtShift + func;
		return inst;
	}
	//j format or hlt/bad instruction
	else if(iForm == REG_NOMATCH && rForm == REG_NOMATCH){
		return 0;
	}
	else{
		perror("");
		return 0;
	}
}

unsigned int ifStage(int pCount, int *cycles, int clock){
	unsigned int instruction = instructions[pCount];
	cycles[0] = clock;
	return instruction;
}

int idStage(unsigned int instruction, int* decode, int *cycles, int clock){
	//opcode 31 - 26
	decode[0] = instruction >> 26;
	//rs 25 - 21
	decode[1] = (instruction >> 21) % 32;
	//rt 20 - 16
	decode[2] = (instruction >> 16) % 32;
	//rd 15 - 11
	decode[3] = (instruction >> 11) % 32;
	//shamt 10 - 6
	decode[4] = (instruction >> 6) % 32;
	//funct 5 - 0
	decode[5] = instruction % 64;
	//immediate 15 - 0, short cast for sign conversion
	decode[6] = (short)instruction % 65536;
	cycles[1] = clock;
	pc++;
	return instruction; 
}

int exStage(int *exec, int* decode, int *cycles, int clock){
	int opcode = decode[0], rs = decode[1], rt = decode[2];
	int val = 0, dest = 0, memread = 0, memwrite = 0, regwrite = 0;
	//iformat
	if(opcode){
		int imm = decode[6];
		//addi, subi, li
		if(opcode == 8){
			//value to be written
			val = registers[rs] + imm;
			regwrite = 1;
			//register to write to
			dest = rt;
		}
		//lw
		else if(opcode == 35){
			//address to read from, unconverted
			val = registers[rs] + imm;
			memread = 1;
			regwrite = 1;
			//register to write to
			dest = rt;
		}
		//sw
		else{
			//value to be written
			val = registers[rt];
			//destination
			dest = registers[rs] + imm;
			memwrite = 1;
		}
	}
	//rformat
	else{
		int rd = decode[3], shamt = decode[4], func = decode[5];
		//sll
		if(func == 0){
			//value to be written
			val = registers[rs] << registers[shamt];
			regwrite = 1;
			dest = rd;
		}
		//srl
		else if(func == 2){
			val = registers[rs] >> registers[shamt];
			regwrite = 1;
			dest = rd;
		}
		//add
		else if(func == 32){
			val = registers[rs] + registers[rt];
			regwrite = 1;
			dest = rd;
		}
		//sub
		else if(func == 34){
			val = registers[rs] - registers[rt];
			regwrite = 1;
			dest = rd;
		}
		//and
		else if(func == 36){
			val = registers[rs] & registers[rt];
			regwrite = 1;
			dest = rd;
		}
		//or
		else{
			val = registers[rs] | registers[rt];
		}
	}
	exec[0] = val;
	exec[1] = dest;
	exec[2] = memread;
	exec[3] = memwrite;
	exec[4] = regwrite;
	cycles[2] = clock;
	return 0;
}

int memStage(int* exec, int *cycles, int clock){
	int val = exec[0];
	//memread, lw
	if(exec[2]){
		val = data[(exec[0] - 256) / 4]; 
	}
	//memwrite, sw
	if(exec[3]){
		data[(exec[1] - 256) / 4] = exec[0];
	}
	cycles[3] = clock;
	return val;
}

int wbStage(int* mem, int *cycles, int clock){
	if(mem[0]){
		registers[mem[2]] = mem[1];
	}
	cycles[4] = clock;
	return 0;
}
