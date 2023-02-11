#include <unistd.h>
#include <stdio.h>
#include <string.h>

char *output_path = "../romfs";

int main(int argc, char **argv)
{
	FILE *file = fopen(output_path, "wb");

	int opt;
	while((opt = getopt(argc, argv, "o:d:")) != -1) {
		switch(opt) {
		case 'd':
			printf("d: %s\n", optarg);
			break;
		case 'o':
			printf("o: %s\n", optarg);
			break;
		}
	}

	char *test = "hello";
	fwrite(test, sizeof(char), strlen(test) + 1, file);

	fclose(file);

	return 0;
}
