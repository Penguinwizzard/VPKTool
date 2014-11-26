#include "vpk.h"

int main(int argc, char** argv) {
	if(argc < 2) {
		printf("Please provide a file argument\n");
		return 1;
	}
	FILE* vpkfile = fopen(argv[1],"r");
	vpk* v = readvpk(vpkfile);
	
	printvpkversioningdata(v);

	freevpk(v);
	fclose(vpkfile);
	return 0;
}
