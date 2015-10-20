
#pragma test expect_ir("lit(\"x\": ref<int<4>,f,f>)")
int x;

#pragma test expect_ir("lit(\"y\": ref<real<4>,t,f>)")
const float y;

typedef enum { Bla, Alb } enum_t;
#pragma test expect_ir("lit(\"globalEnum\": ref<__insieme_enum<IMP_enum_t,Bla,Alb>,f,f>)")
enum_t globalEnum;

typedef struct { int x; } IAmTheTagType;
#pragma test expect_ir("lit(\"tt\": ref<struct IMP_IAmTheTagType { x: int<4>; },f,f>)")
IAmTheTagType tt;

int main() {
	globalEnum;
	y;
	tt;
	return x;
}
