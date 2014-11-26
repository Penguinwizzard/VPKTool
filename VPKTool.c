#include "vpk.h"

int main(int argc, char** argv) {
	if(argc < 3) {
		printf("Please provide a file argument");
	}
	FILE* vpkfile = fopen(argv[1],"r");
	vpk* v = readvpk(vpkfile);
	
	printvpkversioningdata(v);

	freevpk(v);
	fclose(vpkfile);
	return 0;
}
