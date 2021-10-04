#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "argmanip.h"

char** manipulate_args(int argc, const char* const* argv, int(* const manip)(int))
{
	//make a copy of the argument list
	char** args = malloc((argc+1) * sizeof(char*));
	//call malloc once for overall array where each element is string of type char *
	for (int i = 0; i < argc; ++i) {
		//malloc each element for argv[i]
		int len = strlen(argv[i]);
		//call malloc for each element of that array, where each holds the manipulated version of each argument 
		args[i] = malloc( (len+1) * sizeof(char));
		for (int j = 0; j < len; ++j) {
			//copy each string char by char, passing through the manip function as you progress
			args[i][j] = manip(argv[i][j]);
		}
		args[i][len] = '\0';
	}
	args[argc] = '\0';
	return args;

}

void free_copied_args(char** args, ...)
{
	//for each argument list, malloc everything 
	//first free all indivual strings
	//then free overall array
	va_list arguments;
	va_start(arguments, args);
	
	char** argToFree;
	argToFree = args;

	while (argToFree != NULL)
	{
		//Gather how big argToFree is
		int len = 0;
		while (1)
		{
			if (argToFree[len] == NULL) {
				
				break;
			}
			len++;
		}

		for (int i = 0; i < len; ++i) {
			if(argToFree[i] == NULL)
				break;
			free(argToFree[i]);
		}
		free(argToFree);
		argToFree = va_arg(arguments, char**);
		if (argToFree == NULL)
			break;
	}

	va_end(arguments);
}