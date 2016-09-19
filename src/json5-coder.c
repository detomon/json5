/*
 * Copyright (c) 2016 Simon Schoenenberger
 * https://github.com/detomon/json5
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <string.h>
#include "json5-coder.h"

/**
 * Initialize a coder object
 */
int json5_coder_init (json5_coder * coder) {
	int res = 0;

	if ((res = json5_tokenizer_init (&coder -> tknzr)) != 0) {
		goto cleanup;
	}

	if ((res = json5_parser_init (&coder -> parser)) != 0) {
		goto cleanup;
	}

	return res;

	cleanup: {
		json5_coder_destroy (coder);

		return res;
	}
}

void json5_coder_destroy (json5_coder * coder) {
	json5_tokenizer_destroy (&coder -> tknzr);
	json5_parser_destroy (&coder -> parser);
}

static void json5_coder_reset (json5_coder * coder) {
	json5_tokenizer_reset (&coder -> tknzr);
	json5_parser_reset (&coder -> parser);
}

static int json5_coder_put_token (json5_token const * token, json5_coder * coder) {
	return json5_parser_put_tokens (&coder -> parser, token, 1);
}

int json5_coder_decode (json5_coder * coder, uint8_t const * string, size_t size, json5_value * out_value) {
	int res;

	json5_coder_reset (coder);

	if ((res = json5_tokenizer_put_chars (&coder -> tknzr, string, size, (json5_put_token_func) json5_coder_put_token, coder)) != 0) {
		return res;
	}

	if ((res = json5_tokenizer_put_chars (&coder -> tknzr, NULL, 0, (json5_put_token_func) json5_coder_put_token, coder)) != 0) {
		return res;
	}

	json5_value_set_null (out_value);
	*out_value = coder -> parser.value;
	memset (&coder -> parser.value, 0, sizeof (coder -> parser.value));

	return res;
}
