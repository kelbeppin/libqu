
#include <assert.h>
#include "../src/qu_array.c"

//------------------------------------------------------------------------------

static int test_read(void *arg)
{
	qx_file *file = qx_fopen("data.bin");
	assert(file != NULL);

	unsigned char a[4], b[4], c[4], d[4];

	qx_fread(a, 4, file);
	assert(a[0] == 0xde && a[1] == 0xad && a[2] == 0xca && a[3] == 0xfe);

	qx_fread(b, 4, file);
	assert(b[0] == 0x20 && b[1] == 0x22 && b[2] == 0x02 && b[3] == 0x24);

	qx_fread(c, 4, file);
	assert(c[0] == 'u' && c[1] == 't' && c[2] == 'a' && c[3] == 'r');

	qx_fread(d, 4, file);
	assert(d[0] == 'a' && d[1] == 'b' && d[2] == 'i' && d[3] == 'n');

	printf("0x%02x 0x%02x 0x%02x 0x%02x\n", a[0], a[1], a[2], a[3]);
	printf("0x%02x 0x%02x 0x%02x 0x%02x\n", b[0], b[1], b[2], b[3]);
	printf("0x%02x 0x%02x 0x%02x 0x%02x\n", c[0], c[1], c[2], c[3]);
	printf("0x%02x 0x%02x 0x%02x 0x%02x\n", d[0], d[1], d[2], d[3]);

	qx_fclose(file);

	return 0;
}

//------------------------------------------------------------------------------

struct test
{
	int (*function)(void *);
	void *data;
};

int main(int argc, char *argv[])
{
	struct test tests[] = {
		{ test_read, NULL },
		{ NULL },
	};

	int current = 0;

	while (true) {
		if (!tests[current].function) {
			break;
		}

		printf("\n");
		printf("*** TEST #%d ***\n", current);
		printf("\n");

		if (!tests[current].function(tests[current].data)) {
			current++;
			continue;
		}

		return 1;
	}

	return 0;
}
