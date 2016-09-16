#include <stdlib.h>
#include <stdio.h>
#include "array.h"

TEMPLATE_ARRAY(char);
TEMPLATE_ARRAY_TYPEDEF(char, string);
TEMPLATE_ARRAY_OBJ(string);


void out_of_memory() {
	puts("Ran out of memory.");
	exit(1);
}


ret_string read_line(FILE *file, BOOL *eof)
BEGIN
	int ch;
	ARRAY_INIT(char, line, 0, 20);	// initial size of 0, reserve 20

	while((ch = fgetc(file)) != EOF && ch != '\n') {
		if(!string_add(line, ch))
			out_of_memory();
	}

	if(!string_add(line, '\0'))
		out_of_memory();

	*eof = (ch == EOF);
	RETURN(line);
END


ret_array_string read_file(const char *filename)
BEGIN
	BOOL eof = FALSE;
	ARRAY_INIT_NULL(char, line);
	ARRAY_INIT(string, lines, 0, 0);

	FILE *file = fopen(filename, "r");

	if(!file)
		RETURN(lines);

	while(!eof) {
		string_assign(line, read_line(file, &eof));

		if(!array_string_add(lines, line))
			out_of_memory();
	}

	fclose(file);
	RETURN(lines);
END


int main()
BEGIN
	ARRAY_INIT_NULL(string, lines);
	ARRAY_INIT_NULL(char, line);

	array_string_assign(lines, read_file("LICENSE"));

	size_t line_count = array_string_size(lines);

	for(size_t i = 0; i < line_count; i++) {
		string_assign(line, array_string_get(lines, i));
		printf("%03d:%s\n", (int)i, string_raw(line));
	}

	RETURN_BASIC(0);
END

