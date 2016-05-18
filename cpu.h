/* Written by Pieter Kockx */
int32_t R[32];
int32_t PC;
int32_t LO;
int32_t HI;
int32_t bR;
extern union inst *IR;

void cpu(int, int, char);
void decode(void);

