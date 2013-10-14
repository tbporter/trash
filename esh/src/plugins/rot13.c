#include <stdbool.h>
#include <stdio.h>

#include "../esh.h"

static void rot13er(char*);

static bool 
init_plugin(struct esh_shell *shell)
{
    printf("Plugin 'rot13' initialized...\n");
    return true;
}

static bool rot13(struct esh_command *cmd){
	if (strcmp(cmd->argv[0], "rot13"))
        return false;

    char** p = cmd->argv;
    int argc = 0;

    while(*p){
    	argc++;
    	*p++;
    }
    if(argc < 2){
    	printf("Not enough arguments.\n");
    	return true;
    }
    else if(argc == 2){
    	rot13er(cmd->argv[1]);
    	printf("%s\n",cmd->argv[1]);
    }
    else if(argc == 4){
    	if (!strcmp(cmd->argv[1], "-n")){
			//Check if it's a proper non-zero number
    		int Nrot = atoi(cmd->argv[2]);
    		if(Nrot <= 0){
    			printf("Need a positive number.\n");
    			return true;
    		}

    		for(; Nrot>0; Nrot--){
    			rot13er(cmd->argv[3]);
    		}
    		printf("%s\n",cmd->argv[3]);
    	} else {
    		printf("Invalid flag.\n");
    	}
    }
    else{
    	printf("Invalid\n");
    }
    return true;
}

static void rot13er(char* string){
	int i;
	for(i = 0; string[i] != '\0'; i++){
		if(string[i] >= 'a' && string[i] <= 'm')
			string[i] += 13;
		else if(string[i] >= 'n' && string[i] <= 'z')
			string[i] -= 13;
		else if(string[i] >= 'A' && string[i] <= 'M')
			string[i] += 13;
		else if(string[i] >= 'N' && string[i] <= 'Z')
			string[i] -= 13;
	}
}
struct esh_plugin esh_module = {
    .rank = 10,
    .init = init_plugin,
    .process_builtin = rot13
    
};
