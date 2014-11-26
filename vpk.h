/*
 * VPKLib v1
 * author: Derek Morris (Penguinwizzard)
 */

#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>

#define NAMELENGTH (256)
//#define VPKLIB_DUBUG 1

// The base header type for casting
typedef struct {
	uint32_t signature;
	uint32_t version;
	uint32_t tree_length;
} vpkheader_generic;

// VPK V1 has a fairly slim header which is well-known.
typedef struct {
	uint32_t signature;
	uint32_t version;
	uint32_t tree_length;
} vpkheader_v1;

// VPK V2 has a somewhat larger header with a few unknown values.
typedef struct {
	uint32_t signature;
	uint32_t version;
	uint32_t tree_length;

	uint32_t unknown1;
	uint32_t footerlength;
	uint32_t unknown3;
	uint32_t unknown4;
} vpkheader_v2;

// VPK has a primary data structure which is a tree of vpkdirecotryentry
// elements, all of which are of this form (and followed by preload_bytes
// of preloaded file info)
typedef struct __attribute__((__packed__)) {
	uint32_t CRC;
	uint16_t preload_bytes;
	uint16_t archive_index;
	uint32_t entry_offset;
	uint32_t entry_length;
	uint16_t terminator;
} vpkdirectoryentry;

// Used internally for sorting purposes
typedef struct {
	char* fullname;
	vpkdirectoryentry* entry;
} vpkentry;

// A vpk handle, used so that applications using the library can use it
// in a re-entrant manner.
typedef struct {
	unsigned char* buffer;
	unsigned int buflen;
	int version;
	vpkheader_generic* header; // NULL if version==0
	int entrycount;
	vpkentry* entries;
} vpk;

int traverse_for_read(unsigned char* buffer, unsigned int buflen, vpkentry* entries);
int vpkentry_comp(const void* elem1, const void* elem2);

// Given a FILE*, read the vpk _dir information described by that FILE*
vpk* readvpk(FILE* vpk_file_pointer) {
	vpk* ret = (vpk*)malloc(sizeof(vpk));
	vpkheader_generic* vpkheader = (vpkheader_generic*)malloc(sizeof(vpkheader_generic));
	fseek(vpk_file_pointer, 0, SEEK_SET);
	// we need to handle version 0 properly
	if(
		(fread((unsigned char*)vpkheader, sizeof(vpkheader_generic), 1, vpk_file_pointer) == 0)
	  )
	{
		free(ret);
		free(vpkheader);
		return NULL;
	}
	//handle vpkv0
	if(
		(vpkheader->signature != 0x55aa1234) ||
		(vpkheader->version != 1 && vpkheader->version != 2)
	  )
	{
		ret->version = 0;
		free(vpkheader);
	} else {
		ret->version = vpkheader->version;
	}
	//go back to the start
	fseek(vpk_file_pointer, 0, SEEK_SET);
	switch(ret->version) {
		case 0:
			ret->header = NULL;
			//we have to consider basically the whole file potentially being part of the tree
			fseek(vpk_file_pointer, 0, SEEK_END);
			ret->buflen = (unsigned int)ftell(vpk_file_pointer);
			fseek(vpk_file_pointer, 0, SEEK_SET);
			break;
		case 1:
			//already done
			fseek(vpk_file_pointer, sizeof(vpkheader_v1), SEEK_SET);
			ret->header = vpkheader;
			ret->buflen = ret->header->tree_length;
			break;
		case 2:
			//type 2 is a little bigger header
			free(vpkheader);
			vpkheader = (vpkheader_generic*)malloc(sizeof(vpkheader_v2));
			if(fread((unsigned char*)vpkheader, sizeof(vpkheader_v2), 1, vpk_file_pointer) == 0) {
				free(ret);
				free(vpkheader);
				return NULL;
			}
			ret->header = vpkheader;
			ret->buflen = ret->header->tree_length;
			break;
		default:
			free(vpkheader);
			free(ret);
			printf("VPKLIB: VERSION ERROR\n");
			return NULL;
	}
	// at this point the seek in the file is set to the end of the header
	ret->buffer = (unsigned char*)malloc(ret->buflen);
	fread(ret->buffer, ret->buflen, 1, vpk_file_pointer);

	// get the number of entries
	ret->entrycount = traverse_for_read(ret->buffer, ret->buflen,NULL);

	ret->entries = (vpkentry*)malloc((ret->entrycount)*sizeof(vpkentry));

	// do another traversal to get the actual data filled in
	traverse_for_read(ret->buffer, ret->buflen, ret->entries);
	qsort(ret->entries,ret->entrycount,sizeof(vpkentry),vpkentry_comp);
	return ret;
}

// Helper function for readvpk; traverses the vpk's tree and returns
// the number of elements in the tree, and if entries is non-null also
// copies pointers into entries
int traverse_for_read(unsigned char* buffer, unsigned int buflen, vpkentry* entries) {
	// ok, now we've read the tree in (at least; there may be a little more read).
	// all that's left is to parse it
	// this is the tree read routine
	unsigned int index = 0;
	unsigned int level = 0;
	unsigned char* curext;
	unsigned char* curpath;
	unsigned char curpathfixed[NAMELENGTH];
	int count = 0;
	while(true) {
		if(index >= buflen) {
			break;
		}
		if(level < 0) {
			break;
		}
		if(buffer[index]=='\0') {
			level--;
			index++;
			continue;
		}
		switch(level) {
			case 0: //ext level
				curext = (unsigned char*)&(buffer[index]);
				level++;
				while(buffer[index] != '\0')
					index++;
				index++;
				break;
			case 1: //path level
				curpath = (unsigned char*)&(buffer[index]);
				level++;
				while(buffer[index] != '\0')
					index++;
				index++;
				//fix path
				snprintf((char*)curpathfixed,NAMELENGTH,"%s",curpath);
				//remove leading spaces
				int i,j;
				for(i=0;i<NAMELENGTH;i++) {
					if(curpathfixed[i]!=' ')
						break;
				}
				for(j=0;j<NAMELENGTH-i;j++) {
					curpathfixed[j]=curpathfixed[j+i];
				}
				//remove trailing space
				for(i=0;i<NAMELENGTH;i++) {
					if(curpathfixed[i]=='\0')
						break;
				}
				for(j=i-1;j>=0;j--) {
					if(curpathfixed[j]!=' ')
						break;
					curpathfixed[j]='\0';
				}
				//add trailing /
				for(i=0;i<NAMELENGTH;i++) {
					if(curpathfixed[i]=='\0')
						break;
				}
				if(i!=0 && curpathfixed[i-1]!='/' && i<NAMELENGTH-1) {
					curpathfixed[i]='/';
					curpathfixed[i+1]='\0';
				}
				break;
			case 2: //file level
				//throwaway due to switch conventions
				if(entries != NULL) {
					entries[count].fullname = (char*)malloc(NAMELENGTH*sizeof(char));
				}
				unsigned char* filename = (unsigned char*)&(buffer[index]);
				while(buffer[index] != '\0')
					index++;
				index++;
				vpkdirectoryentry* filedata = (vpkdirectoryentry*)(&(buffer[index]));
				index += sizeof(vpkdirectoryentry);
				index += filedata->preload_bytes;
				if(entries != NULL) {
					snprintf(entries[count].fullname,NAMELENGTH,"%s%s.%s",curpathfixed,filename,curext);
					entries[count].entry = filedata;
				}
#ifdef VPKLIB_DEBUG
				printf("%s%s.%s : %d %d\n",curpath,filename,curext,filedata->CRC,filedata->entry_length);
#endif
				count++;
				break;
		}
	}
	return count;
}

// Used for qsort-ing vpkentrys
int vpkentry_comp(const void* elem1, const void* elem2) {
	vpkentry* a = (vpkentry*)elem1;
	vpkentry* b = (vpkentry*)elem2;
	return strncmp(a->fullname,b->fullname,NAMELENGTH);
}

// Clean up properly
void freevpk(vpk* vpkobj) {
	free(vpkobj->buffer);
	if(vpkobj->version != 0) {
		free(vpkobj->header);
	}
	int i;
	for(i=0;i<vpkobj->entrycount;i++) {
		free(vpkobj->entries[i].fullname);
	}
	free(vpkobj);
}


// analysis functions

// Dump the filenames in the VPK
void printvpkfilenames(vpk* v) {
	int i;
	for(i=0;i<v->entrycount;i++) {
		printf("%s\n",v->entries[i].fullname);
	}
}

// Good info for text-based versioning
void printvpkversioningdata(vpk* v) {
	int i;
	for(i=0;i<v->entrycount;i++) {
		printf("%s CRC:%010x size:%d\n",v->entries[i].fullname,v->entries[i].entry->CRC,v->entries[i].entry->preload_bytes + v->entries[i].entry->entry_length);
	}
}

// Print out a file-level diff of two VPKs
void printdiff(vpk* a, vpk* b) {
	int leftindex = 0;
	int rightindex = 0;
	while(leftindex < a->entrycount && rightindex < b->entrycount) {
		int cmp = strncmp(a->entries[leftindex].fullname,b->entries[rightindex].fullname,NAMELENGTH);
		if(cmp == 0) { // same spot!
			if(a->entries[leftindex].entry->CRC != b->entries[rightindex].entry->CRC)
				printf("M %s\n",a->entries[leftindex].fullname);
			leftindex++;
			rightindex++;
		} else if(cmp < 0) {
			printf("> %s\n",a->entries[leftindex].fullname);
			leftindex++;
		} else { // cmp > 0
			printf("< %s\n",b->entries[rightindex].fullname);
			rightindex++;
		}	
	}
}
