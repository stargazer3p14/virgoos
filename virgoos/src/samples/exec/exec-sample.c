/*
 *	exec-sample.c
 *
 *	Sample application that shows usage of exec() facilities.
 */

#include "sosdef.h"
#include "config.h"
#include "errno.h"
#include "taskman.h"

void	app_entry()
{
//	char	filename[256] = "/bin/ls";
	char	filename[256] = "./testapp";
	char	**argv, **envp;
	int	rv;

	argv = malloc(sizeof(char*) * 3);
	argv[0] = filename;
	argv[1] = "-l";
	argv[2] = NULL;

	envp = malloc(sizeof(char*) * 3);
	envp[0] = "PATH=/bin";
	envp[1] = "HELLO=world";
	envp[2] = NULL;

	set_running_task_options(OPT_TIMESHARE);

	printf("%s(): Running %s\n", __func__, filename);
	rv = execve(filename, argv, envp);
	printf("%s(): execve() returned %d\n", __func__, rv);

	while (1)
		sleep(1);
}
