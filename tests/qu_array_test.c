
#include <assert.h>
#include "../src/qu_array.c"

//------------------------------------------------------------------------------

#define TOTAL_ELEMENTS 1024

struct element
{
	uint32_t guard;
	int index;
	int32_t i32[32];
	float f32[32];
};

static qu_array *array = NULL;
static int32_t id_array[TOTAL_ELEMENTS];

static void element_init(struct element *element, int index)
{
	element->guard = 0xDEADCAFE;
	element->index = index;

	for (int i = 0; i < 32; i++) {
		// 0: [32, 31, 30, 29, 28, 27, ...]
		// 1: [64, 63, 62, 61, 60, 59, ...]
		// 2: [96, 95, 94, 93, 92, 91, ...]

		element->i32[i] = index * 32 + (32 - i);
	}

	for (int i = 0; i < 32; i++) {
		// 0: [0.f, 8.f, 16.f, 24.f, 32.f, ...]
		// 1: [8.f, 16.f, 24.f, 32.f, 40.f, ...]
		// 2: [16.f, 24.f, 32.f, 40.f, 48.f, ...]

		element->f32[i] = index * 8.f + i * 8.f;
	}
}

static void element_check(struct element *element)
{
	assert(element->guard == 0xDEADCAFE);

	for (int i = 0; i < 32; i++) {
		assert(element->i32[i] == (element->index * 32 + (32 - i)));
	}

	for (int i = 0; i < 32; i++) {
		assert(element->f32[i] == (element->index * 8.f + i * 8.f));
	}

	printf("Element #%d is valid.\n", element->index);
}

static void element_dtor(void *ptr)
{
	struct element *element = (struct element *) ptr;

	element_check(element);

	printf("Element #%d is destroyed.\n", element->index);
}

static int test_create(void *arg)
{
	array = qu_create_array(sizeof(struct element), element_dtor);

	if (!array) {
		return -1;
	}

	assert(array);
	assert(array->element_size == sizeof(struct element));
	assert(array->dtor == element_dtor);

	printf("Created array.\n");

	return 0;
}

static int test_add(void *arg)
{
	for (int i = 0; i < TOTAL_ELEMENTS; i++) {
		printf("Inserting element #%d...\n", i);

		struct element element;
		element_init(&element, i);

		id_array[i] = qu_array_add(array, &element);

		assert(id_array[i] != 0);

		printf(":: Received identifier: #%d -> 0x%08x (%d)\n",
			   i, id_array[i], id_array[i]);
	}

	return 0;
}

static int test_get(void *arg)
{
	for (int i = 0; i < TOTAL_ELEMENTS; i++) {
		struct element *element = qu_array_get(array, id_array[i]);
		element_check(element);
	}

	return 0;
}

static int test_remove(void *arg)
{
	for (int i = 0; i < 16; i++) {
		qu_array_remove(array, id_array[i]);
	}

	for (int i = 0; i < 16; i++) {
		int32_t old_id = id_array[i];

		struct element element;
		element_init(&element, i);

		id_array[i] = qu_array_add(array, &element);

		assert(id_array[i] != old_id);
	}

	return 0;
}

static int test_destroy(void *arg)
{
	qu_destroy_array(array);
	qu_destroy_array(NULL);

	printf("Destroyed array.\n");

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
		{ test_create, NULL },
		{ test_add, NULL },
		{ test_get, NULL },
		{ test_remove, NULL },
		{ test_destroy, NULL },
		{ NULL },
	};

	qu_platform_initialize();

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
