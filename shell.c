/*
Jason Lonsigner - Operating Systems Homework 4, Linux Shell
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_LINE 80
#define MAX_PATH 80

//doubly linked list structure
struct Node {
	char *string; //stores a command
	int number; //stores order of occurance
	struct Node* next; //gets next node
	struct Node* prev; //gets previous node
};

void execute(char **args, int should_wait); //executes command by forking into a child process
void parse(char *args[], char * string); //parses and tokenizes user input
void print_list(struct Node *head); //prints a linked list at starting point head
void destroy_list(struct Node* head, struct Node* tail); //destroys the linked list
char *search_list(struct Node* head, const int number); //searches a list for an entryy with number 0, or returns the last element if not found
int add_to_history(struct Node** head, struct Node** tail, char *string); //adds an item to the linked list using double pointers (A DISGUSTING CODE SOLUTION)

int main(void) {
	char *args[MAX_LINE/2+1] = {}; //{} is important prevents errors due to garbage values
	char string[MAX_LINE] = {}; //used to reterive user input and for parsing
	char path[MAX_PATH] = {}; //stores the current working directory
	char string_val[MAX_LINE] = {}; //used to prevent adding !! and ! # into history

	struct Node* head = (struct Node*)malloc(sizeof(struct Node)); //head of the history list
	struct Node* currentNode = NULL; //used as tail of history list

	//main loop
	int should_wait = 0; //used for & command
	int should_run = 1; //if 0 the loop will end
	while(should_run) {
		getcwd(path, MAX_PATH); //update the current working directory variable
		if(should_wait==0) printf("%s>", path); //print the current working directory and the command prompt token >
		else if(should_wait==1)  {
			should_wait = 0; //reset shell to execute synchronously
			int wait_time = 2000; //time to wait before printing the command prompt again
			wait(&wait_time); //gives enough time to print below statement after the output of a background process
							 //hacky and defeats the point of using &, but looks cleaner without this wait....
							//commands output will print on top off the command prompt below
			printf("%s>", path); //print prompt now that previous async command has "probably finished executing"
		}
		fflush(stdout);	//Dr. Arsu, what does flush do??? Flush command input output so the OSH> isn't included in with user input???
		
		gets(string); //take in user input
		//check for & to tell if user wants to execute process in the background
		for(int i = 0; i < strlen(string); i++) {
			//if found set should_wait to false value of 1 and remove the & from the command before exec
			if(string[i]=='&') {
				string[i] = ' '; //remove & from user input
				should_wait = 1; //make exec run in background
			}
		}
		strcpy(string_val, string); //copy user input string into a secondary storage...does not work if when using just the string variable
		parse(args, string_val); //parse the arguments with this for checking for symbols !, !!, history...does not work if when using just the string variable
 
		//check for the symbols !, !!, history and skip adding them to the command history else insert command
		if(string[0] != '!' && strcmp(*args, "history")!=0) {
			if(add_to_history(&head, &currentNode, string)!= 0) printf("Error adding to history\n");
		}
		parse(args, string); //finally tokenize the actuall user input

		//exiting the shell has priority
		if(strcmp(*args, "exit")==0) {
			should_run = 0; //check if user wants to exit before forking
			continue; //jump to next iteration of the loop and exit
		}
		else if(strcmp(*args, "cd")==0) chdir(args[1]); //handle changing directory
		else if(strcmp(*args, "history")==0) print_list(head); //print command history
		else if(strcmp(*args, "!")==0)  {
			if(head->next==NULL) {
				printf("No comamnd history...\n");
				continue;
			}
			//MINOR BUG -> need to account for single ! passed in to prompt here, or else shell will crash

			//execute previous command with order of occurance n
			strcpy(string, search_list(head, atoi(args[1])));
			if(strcmp(string, "")!=0) {
				parse(args, string);
				execute(args, should_wait);
			} else printf("Number not found...\n");
		}
		else if(string[0] == '!' && string[1] != '!' && string[1] != ' ') {
			if(head->next==NULL) {
				printf("No comamnd history...\n");
				continue;
			}
			//execute previous command with order of occurance n...
			//the only difference from the previous is that this work without a space between ! and a number...
			//LOGICAL SHORTCOMMING-> this method prevents using numbers greater than 9... other method should be used
			strcpy(string, search_list(head, atoi(&string[1])));
			if(strcmp(string, "")!=0) {
				parse(args, string);
				execute(args, should_wait);
			} else printf("Number not found...\n");
		}
		else if(strcmp(*args, "!!")==0)  {
			if(head->next==NULL) {
				printf("No comamnd history...\n");
				continue;
			}
			//execute the second to last command...
			//Second to last as it is unlikely the user forgot the thing they just typed (Style choice / Breaking the rules)
			strcpy(string, search_list(head, 0));
			if(strcmp(string, "")!=0) {
				parse(args, string);
				execute(args, should_wait);
			} else printf("Number not found...\n");
		}
		else execute(args, should_wait); //execute requested program intered in by the user
	}
	destroy_list(head, currentNode); //clean up linked list
	return 0;
}

//adds a new node to the command history, returns -1 if invalid
int add_to_history(struct Node** head, struct Node** tail, char *string) {
	//if list is null then start the list
	if((*head)->string == NULL) {
		(*head)->string = (char *)malloc(sizeof(char)*MAX_LINE);
		(*head)->number = 1;
		strcpy((*head)->string, string);

		(*tail) = (*head);
		return 0; //return success
	} else {
		//add new node into the list
		(*tail)->next = (struct Node*)malloc(sizeof(struct Node));
		(*tail)->next->string = (char *)malloc(sizeof(char)*MAX_LINE);
		(*tail)->next->number = (*tail)->number + 1;
		strcpy((*tail)->next->string, string);

		(*tail)->next->prev = (*tail);
		(*tail) = (*tail)->next;
		return 0; //return success
	}

	return -1; //return error
}

//executes child process, if should_wait==0 then the command is synchronus, if should_wait==1 then it will execute in the background
void execute(char **args, int should_wait) {
	pid_t pid = fork(); //store process id for checking if fork is parent or child
	if(pid < 0) {
		//if zombie / orphan prompt with error then try to exit
		perror("Forking child process failed\n");
		exit(-1);
	} else if (pid == 0) {
		//if child process then execute command
		if(execvp(*args, args) < 0) {
			//if commandis not found or execution could not be called prompt with error then exit
			perror("Exec Failed");
			exit(-1);
		}
		exit(0); //redundant??? child exits premeturely???
				//added this backin after handling &....forgot why?
	} else { 
		//parent process
		int status;	//child process status on exit: 0 returned successfully, 1 is returned with error
		if(should_wait==0) waitpid(pid, &status, 0); //wait on child if & was passed and get return status <- which i never actually made a prompt for :(
	}
}

//tokenizes a string by sperating at spaces
void parse(char *args[], char string[]) {
	args[0] = strtok(string," "); //start tokenizing input string by splitting spaces
	int i = 0; while(args[i]!=NULL) args[++i] = strtok(NULL," "); //keep splitting string
}

//searches the command history linked list for a specific number
char *search_list(struct Node* head, const int number) {
	//if head is null then there is no command history
	if(head == NULL) {
		printf("No Command History...\n");
		return "";
	}

	//user a temporary pointer to the list to iterate through...
	//If desired command with order of occurance n is not found then the last command in the list is executed (Another Style Choice / Breaking Rules)
	struct Node* temp = head; //start at head of command history list
	if(number > 0) while(temp->number != number && temp->next != NULL) temp = temp->next; //loop till desired number is found or end of list is reached
	else while(temp->next->next != NULL) temp = temp->next; //finds last element in the list and returns that

	//only problem with this method of iteration is the temp still points to list so if any funny stuff happens in...
	//the background this can cause problems (Highly Unlikely / Impossible Though)???
	return temp->string; //return string of last command if desired number is not found
}

//prints the entire command history
void print_list(struct Node *head) {
	//if list is null return
	if(head == NULL) {
		printf("No Command History...\n");
		return;
	}
	
	//iterate over the list with temporary pointer and print out its members
	struct Node* temp = head;
	while(temp->next != NULL) {
		printf("Command #%d: %s\n", temp->number, temp->string);
		temp = temp->next;
	}
	temp = NULL; //temp no longer points to list
}

//destroys the linked list
void destroy_list(struct Node* head, struct Node* tail) {
	struct Node *temp; //used for iteration
	if(head == NULL) return; //base case 1
	if(head->next == NULL) { //base case 2
		free(head->string);
		free(head);
		return;
	}
	temp = head->next; //start as 2nd node in list
	
	//while temp is not last node free string then go to next node
	while(temp != tail) {
		free(temp->string);
		temp = temp->next;
	}

	//free head and tail
	free(head->string);
	free(head);
	free(tail->string);
	free(tail);
}
