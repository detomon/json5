#include "test.h"

int main (int argc, char const * argv [])
{
	json5_tokenizer tknzr;

	if (json5_tokenizer_init (&tknzr) != 0) {
		fprintf (stderr, "Failed to initialize tokenizer\n");
		return RESULT_ERROR;
	}

	unsigned char const * string = "😐";

	for (unsigned char const * c = string; *c; c ++) {
		if (json5_tokenizer_put_byte (&tknzr, *c) < 0) {
			fprintf (stderr, "Failed to put byte: %d\n", *c);
		}
	}

	return RESULT_PASS;
}
