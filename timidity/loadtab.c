#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "timidity.h"
#include "common.h"
#include "tables.h"
#include "controls.h"

int load_table(char *file)
{
  FILE *fp;
  char tmp[1024];
  char *value;
  int  i = 0;
	
	if((fp = fopen(file, "r")) == NULL){
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Can't read %s %s\n", file, strerror(errno));
		return -1;
	}

	while(fgets(tmp, sizeof(tmp), fp)){
		if(! strchr(tmp, '#')){
			if((value = strtok(tmp, ", \n")) != NULL){
				freq_table[i++] = atoi(value);
				if(i == 128){
					fclose(fp);
					return 0;
				}
				while((value = strtok(NULL, ", \n")) != NULL){
					freq_table[i++] = atoi(value);
					if(i == 128){
						fclose(fp);
						return 0;
					}
				}
			}
		}
	}
	fclose(fp);
	return 0;
}
