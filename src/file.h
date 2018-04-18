#ifndef FILE_H
#define FILE_H

typedef struct File {
	char *filename;
	char *string;
	unsigned long long size;
} File;

File *read_file(char *filename, File *f) {
	FILE *file = fopen(filename, "r");

	if (file == NULL) {
		printf("%s not found!\n", filename);
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	unsigned long long length = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *file_string = (char *)malloc(length + 1);
	length = fread(file_string, 1, length, file);
	file_string[length] = 0;

	fclose(file);

	f->filename = filename;
	f->string = file_string;
	f->size = length;

	return f;
}

#endif
