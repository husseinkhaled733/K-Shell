#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

char **parse_input(char *input);
void shell();
void execute_shell_builtin(char **parameters);
void execute_command(char **parameters,int background);
void cd(char **parameters);
void echo(char **parameters);
int export(char **parameters);
void on_child_exit(int sig);
void write_to_log_file(char *phrase);
void setup_environment();
char **evaluate_expression(char **parameters,int *background);//to return correct expression in case of export and background processes

#define MAX_ARGS 100


void write_to_log_file(char *phrase){
    FILE *ptr;
    ptr= fopen("result.txt","a");
    if(ptr==NULL){
        printf("failed to open file\n");
        return;
    }
    fprintf(ptr, "%s\n",phrase);
    fclose(ptr);
}

//for zombie processes
void on_child_exit(int sig){
    int status;
    pid_t pid;

    pid = waitpid((pid_t)-1, &status, WNOHANG);
    while(pid>0){
        if (WIFEXITED(status)) {
            write_to_log_file("Child process was terminated");
        }
        pid = waitpid((pid_t)-1, &status, WNOHANG);
    }
}




void echo(char **parameters){
    for (int j = 1; parameters[j] != NULL; j++) {
        char* result=parameters[j];
        if(result[0]=='"'){
            result=result+1;
        }
        if(result[strlen(result)-1]=='"'){
            result[strlen(result)-1]='\0';
        }
        printf("%s ", result);
    }
    printf("\n");
}



void cd(char **parameters){
    if(parameters[1]==NULL||strcmp(parameters[1],"~")==0){
        const char* homedir = getenv("HOME");

        if (homedir != NULL) {
            if (chdir(homedir) != 0) {
                perror("chdir");
            }
        } else {
            fprintf(stderr, "cd: could not find home directory\n");
        }
    }
    else if (parameters[1] != NULL) {
        if (chdir(parameters[1]) != 0) {
            perror("chdir");
        }
    }
}

int export(char **parameters) {
    char *name, *value;

    value = strchr(parameters[1], '=');
    if (value == NULL) {
        printf("Error: invalid input\n");
        return -1;
    }
    *value = '\0';
    name = parameters[1];
    value++;
    if(value[0]=='"'){
        value++;
    }
    if(value[strlen(value)-1]=='"'){
        value[strlen(value)-1]='\0';
    }
    char re[1000]="";
    strcat(re,value);
    for (int j = 2; parameters[j] != NULL; j++) {
        char* result=parameters[j];
        if(result[0]=='"'){
            result=result+1;
        }
        if(result[strlen(result)-1]=='"'){
            result[strlen(result)-1]='\0';
        }
        strcat(re," ");
        strcat(re,result);
    }


    if (setenv(name, re, 1) != 0) {
        printf("Error: unable to set environment variable\n");
        return -1;
    }

    return 0;
}



char **parse_input(char *input) {
    char *token = NULL, *input_copy=NULL;
    const char *delim = " \t\n\r\f\v";  // whitespace characters
    int argc = 0, i = 0;
    char **argv = NULL;

    input_copy= strdup(input);

    token = strtok(input_copy, delim);
    while (token) {
        token = strtok(NULL, delim);
        argc++;
    }

    if (argc > MAX_ARGS) {
        printf("Too many arguments\n");
        free(input_copy);
        return NULL;
    }

    argv = malloc(sizeof(char *) * (argc + 1));


    token = strtok(input, delim);
    while (token) {
        argv[i++] = strdup(token);
        token = strtok(NULL, delim);
    }
    argv[i] = NULL;


    if (argc == 0) {
        printf("No command entered\n");
        free(input_copy);
        free(argv);
        return NULL;
    }

    return argv;
}




void shell(){
    do {
        char input[256];
        int background=0;
        printf("K-> ");
        fgets(input, sizeof(input), stdin);

        input[strcspn(input, "\n")] = '\0';

        char **parameters = NULL, **command=NULL;

        parameters = parse_input(input);

        command=evaluate_expression(parameters,&background);


        if (parameters == NULL) {
            continue;
        }

        //exit
        if(strcmp(command[0],"exit")==0){
            exit(0);
        }


        //shell built_in
        if(strcmp(command[0],"cd")==0||strcmp(command[0],"export")==0||strcmp(command[0],"echo")==0){
            execute_shell_builtin(command);
        }

        //executable command
        else{
            execute_command(command,background);
        }
        for (int i = 0; parameters[i]!=NULL ; ++i) {
            free(parameters[i]);
        }
        free(parameters);
        for (int i = 0; command[i]!=NULL ; ++i) {
            free(command[i]);
        }
        free(command);
    } while (1);
}

void execute_shell_builtin(char **parameters){
    if(strcmp(parameters[0],"cd")==0){
        cd(parameters);
    }
    else if(strcmp(parameters[0],"export")==0){
        export(parameters);
    }
    else{
        echo(parameters);
    }
}


void execute_command(char **parameters,int background){

    pid_t pid=fork();
    if(pid ==-1){
        perror("error");
        exit(1);
    }
    else if(pid==0){
        int error=execvp(parameters[0],parameters);
        if(error==-1){
            perror("error");
            exit(1);
        }
    }
    else{
        if(!background){
            int status;
            pid_t result = waitpid(pid, &status, 0);
            if (result == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }
            else if(WIFEXITED(status)){
                int statuscode= WEXITSTATUS(status);
                if(statuscode!=0){
                    perror("error");
                }
                else{
                    write_to_log_file("Child process was terminated");
                }
            }
        }
    }
}


//setting current directory as home
void setup_environment() {
    char* p[]={"cd",NULL};
    cd(p);
}

char **evaluate_expression(char **parameters,int *background) {
    char** argv=NULL;
    const char *delim = " \t\n\r\f\v";  // whitespace characters
    int j=0;
    argv = malloc(sizeof(char *) * (MAX_ARGS + 1));
    for (int i = 0; parameters[i]!=NULL ; ++i) {
        char* result=parameters[i];
        if(result[0]=='"'){
            result=result+1;
        }
        if(result[strlen(result)-1]=='"'){
            result[strlen(result)-1]='\0';
        }
        if(strchr(result,'$')!=NULL){
            char *envvariable= NULL;
            if(result[0]=='$'){
                envvariable= strdup(result);
                envvariable++;
            }
            else{
                envvariable=strtok(result,"$");
                argv[j++]= strdup(envvariable);
                envvariable=strtok(NULL,"$");
            }


            char* ex_value= getenv(envvariable);

            char *tokenizer;
            char re[1000]="";
            for(tokenizer = strtok(ex_value, delim); tokenizer != NULL; tokenizer = strtok(NULL, delim)){
                argv[j++]= strdup(tokenizer);
                strcat(re,tokenizer);
                strcat(re," ");

            }
            if (setenv(envvariable, re, 1) != 0) {
                printf("Error: unable to set environment variable\n");
            }
        }
        else if(strcmp(result,"&")==0){
            *background=1;
        }
        else{
            argv[j++]= strdup(result);
        }
    }
    argv[j]=NULL;
    return argv;
}

int main() {
    signal(SIGCHLD,on_child_exit);
    setup_environment();
    shell();
    return 0;
}

