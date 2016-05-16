#ifdef _ENCODING
#define MAP(a, b) {a, #b,
#define _(a, b, c) {a, b, c}},
/* uint since hex */
struct opcDict {
	uint32_t opc;
	char *mnem;
	int args[3];
};

struct functDict {
	uint32_t funct;
	char *mnem;
	int args[3];
};
#endif

#ifdef _DECODING
#define MAP(a, b) {a, b},
#define _(a, b, c)
struct opcDict {
	uint32_t opc;
	void (*impl)(void);
};

static void addi(void);
static void addiu(void);
static void andi(void);
static void andi(void);
static void beq(void);
static void bgtz(void);
static void blez(void);
static void bltz(void);
static void bne(void);
static void j(void);
static void jal(void);
static void lb(void);
static void lui(void);
static void lw(void);
static void ori(void);
static void sb(void);
static void slti(void);
static void sltiu(void);
static void sw(void);
static void xori(void);

struct functDict {
	uint32_t funct;
	void (*impl)(void);
};

static void add(void);
static void and(void);
static void addu(void);
static void div(void);
static void divu(void);
static void jr(void);
static void mfhi(void);
static void mflo(void);
static void mthi(void);
static void mtlo(void);
static void mult(void);
static void or(void);
static void sll(void);
static void sllv(void);
static void slt(void);
static void sltu(void);
static void sra(void);
static void srl(void);
static void srlv(void);
static void sub(void);
static void subu(void);
static void syscall(void);
static void xor(void);
#endif

enum operand_alias { NIL, RS, RT, RD, IMM=16, SHAMT=5, TARGET=26 };

/* code smells...
 *	1) macro black-magic, but no azkaban
 *	2) static definition in a header?!
 *	3) storage-allocated in a header
 *	4) hard-coded external data
 * ...not so hard
 *	1) text-editor copy/paste on the other hand, is a dementor's kiss
 *	2) & 3) structure polymorphic and read-only
 *	4) data is not magic but frames the whole program */

#define DICT_OPC 19
static const struct opcDict opcDict[DICT_OPC] =
{
	MAP(0x01, bltz)  	_(RS, IMM, NIL)
	MAP(0x02, j)    	_(TARGET, NIL, NIL)
	MAP(0x03, jal)  	_(TARGET, NIL, NIL)
	MAP(0x04, beq)  	_(RS, RT, IMM)
	MAP(0x05, bne)  	_(RS, RT, IMM)
	MAP(0x06, bgtz)  	_(RS, IMM, NIL)
	MAP(0x07, blez)  	_(RS, IMM, NIL)
	MAP(0x08, addi)		_(RT, RS, IMM)
	MAP(0x09, addiu)	_(RT, RS, IMM)
	MAP(0x0a, slti) 	_(RT, RS, IMM)
	MAP(0x0b, sltiu) 	_(RT, RS, IMM)
	MAP(0x0c, andi) 	_(RT, RS, IMM)
	MAP(0x0d, ori)  	_(RT, RS, IMM)
	MAP(0x0e, xori) 	_(RT, RS, IMM)
	MAP(0x0f, lui)  	_(RT, IMM, NIL)
	MAP(0x20, lb)   	_(RT, IMM, RS)
	MAP(0x23, lw)   	_(RT, IMM, RS)
	MAP(0x28, sb)   	_(RT, IMM, RS)
	MAP(0x2b, sw)   	_(RT, IMM, RS)
};

#define DICT_FUNCT 23
static const struct functDict functDict[DICT_FUNCT] =
{
	MAP(0x00, sll) 		_(RD, RT, SHAMT)
	MAP(0x02, srl) 		_(RD, RT, SHAMT)
	MAP(0x03, sra) 		_(RD, RT, SHAMT)
	MAP(0x04, sllv) 	_(RD, RT, RS)
	MAP(0x06, srlv) 	_(RD, RT, RS)
	MAP(0x08, jr)  		_(RS, NIL, NIL)
	MAP(0x0c, syscall)	_(NIL, NIL, NIL)
	MAP(0x10, mfhi)  	_(RD, NIL, NIL)
	MAP(0x11, mflo)  	_(RD, NIL, NIL)
	MAP(0x12, mthi)  	_(RS, NIL, NIL)
	MAP(0x13, mtlo)  	_(RS, NIL, NIL)
	MAP(0x18, mult)  	_(RS, RT, NIL)
	MAP(0x1a, div)		_(RS, RT, NIL)
	MAP(0x1b, divu)		_(RS, RT, NIL)
	MAP(0x20, add)		_(RD, RS, RT)
	MAP(0x21, addu)		_(RD, RS, RT)
	MAP(0x22, sub) 		_(RD, RS, RT)
	MAP(0x23, subu)		_(RD, RS, RT)
	MAP(0x24, and)		_(RD, RS, RT)
	MAP(0x25, or)  		_(RD, RS, RT)
	MAP(0x26, xor) 		_(RD, RS, RT)
	MAP(0x2a, slt) 		_(RD, RS, RT)
	MAP(0x2b, sltu) 	_(RD, RS, RT)
};

#ifdef MAP
 #undef MAP
#endif
#ifdef _
 #undef _
#endif

