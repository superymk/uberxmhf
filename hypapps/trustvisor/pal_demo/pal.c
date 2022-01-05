#include "pal.h"

void begin_pal_c() {}

int my_pal(uint32_t arg1, uint32_t *arg2) {
	for (uint32_t i = 0; i < arg1; i++) {
		for (uint32_t j = 0; j < *arg2; j++) {
			;
		}
	}
	return arg1 + ((*arg2)++);
}

void end_pal_c() {}
