#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define BUFFER_SIZE 100

struct string {
	char str[BUFFER_SIZE];
	struct string *prev;
	struct string *next;
};

struct context {
	char *filename;
	struct string *filestr;
};

/* panel size */
struct view_size {
    int width;
    int height;
};

/* prototype declaration */
void clear(void);
struct string *insert(struct string *current);
struct string *file_read(char *filename);
void context_set_filename(struct context *context, char *filename);
void render(struct context context);
struct view_size console_size(void);
void backcolor_white(int bool);

int main(int argc, char *argv[]) {
	struct string* head = (struct string *)malloc(sizeof(struct string));
	head->prev = NULL;
	head->next = NULL;
	if (argc != 2) {
        fprintf(stderr, "illegal args\n");
        exit(EXIT_FAILURE);
	} else {
        struct context context;
        context_set_filename(&context, argv[1]);
        render(context);
        exit(EXIT_SUCCESS);
    }
}

/*
 * insert
 * malloc new_str and insert next to current
 * return　new_str
 */
struct string *insert(struct string *current) {
	struct string* new_str = (struct string *)malloc(sizeof(struct string));
	if (current->next) {
		current->next->prev = new_str;
		new_str->next = current->next;
	} else {
		new_str->next = NULL;
	}
	if (current) {
		current->next = new_str;
	}
	new_str->prev = current;
	return new_str;
}

/*
 * console_size
 * return console size
 */
struct view_size console_size(void) {
    struct view_size view_size;
    view_size.width = 0;
    view_size.height = 0;
    
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        fprintf(stderr, "ioctl error\n");
        exit(EXIT_FAILURE);
    } else {
        view_size.width = ws.ws_col;
        view_size.height = ws.ws_row;
    }
    return view_size;
}

/*
 * file_read
 * read filename to struct string
 */
struct string *file_read(char *filename) {
	FILE* fp;
	char buf[BUFFER_SIZE];
	
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "file open error\n");
		exit(EXIT_FAILURE);
	}
    struct string *head = (struct string *)malloc(sizeof(struct string));
    head->prev = NULL;
    head->next = NULL;

	struct string *current = head;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strcpy(current->str, buf);
		insert(current);
		current = current->next;
	}
	fclose(fp);
    return head;
}

/*
 * context_set_filename
 * store contents of filename to the members of struct context
 */
void context_set_filename(struct context *context, char *filename) {
    context->filename = (char *)malloc(sizeof(filename));
    strcpy(context->filename, filename);
    context->filestr = file_read(context->filename);
}

/*
 * output contents of context
 */
void render(struct context context) {
    clear();
    struct string *current = context.filestr;
	while (current) {
        printf("%s", current->str);
        current = current->next;
    }
	printf("\n");
}

/*
 * clear terminal
 */
void clear(void) {
    // 2J: 画面全体消去
    // H = 1;1H: カーソルを1行目1列目
    printf("\e[2J\e[H");
}

/*
 * if bool is 1, change background color white
 */
void backcolor_white(int bool) {
    if (bool)
        // white
        printf("\e[7m");
    else {
        // reset all color
        printf("\e[m");
    }
}
