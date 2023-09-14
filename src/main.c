#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define BUFFER_SIZE 10

typedef char* mbchar;

/* divided by \n, single link */
struct line {
    int byte_count;
    char string[BUFFER_SIZE];
    struct line *next;
};

/* list of line, double link */
struct text {
	struct line *line;
    struct text *prev;
    struct text *next;
};

struct context {
	char *filename;
	struct text *text;
};

/* panel size */
struct view_size {
    int width;
    int height;
};

/* header, white color part*/
struct context_header {
    struct view_size view_size;
    // maybe filename
    char *message;
};

/* prototype declaration */
void clear(void);
struct line *line_insert(struct line *current);
void line_add_char(struct line *head, mbchar mc);
struct text *text_insert(struct text *current);
struct text *text_malloc(void);
mbchar mbchar_malloc(void);
void mbchar_free(mbchar mbchar);
mbchar mbcher_zero_clear(mbchar mbchar);
int mbchar_size(mbchar mbchar);
int isLineBreak(mbchar mbchar);
struct text *file_read(char *filename);
void context_read_file(struct context *context, char *filename);
void render_header(struct context_header context);
void render(struct context context);
void debug_print_text(struct context context);
struct view_size console_size(void);
void backcolor_white(int bool);

int main(int argc, char *argv[]) {
	if (argc != 2) {
        fprintf(stderr, "illegal args\n");
        exit(EXIT_FAILURE);
	} else {
        struct context context;
        context_read_file(&context, argv[1]);
        render(context);
        exit(EXIT_SUCCESS);
    }
}

/*
 * line_insert
 * malloc new_line and insert next to current
 * return　new_line
 */
struct line *line_insert(struct line *current) {
	struct line* new_line = (struct line *)malloc(sizeof(struct line));
    new_line->next = current->next;
    current->next = new_line;
    new_line->byte_count = 0;
    return new_line;
}

/*
 * line_add_char
 * set string to head
 */
void line_add_char(struct line *head, mbchar mc) {
    struct line *current = head;
    // rest is none
    while (current->byte_count >= BUFFER_SIZE - mbchar_size(mc)) {
        if (current->next)
			current = current->next;
		else
            current = line_insert(current);
    }
    int offset = 0;
    while (offset < mbchar_size(mc)) {
        current->string[current->byte_count + offset] = mc[offset];
        current->byte_count++;
        offset++;
    }
}

/*
 * text_insert
 * malloc new_text and insert next to current
 * return　new_text
 */
struct text *text_insert(struct text *current) {
    struct text *new_text = (struct text *)malloc(sizeof(struct text));
    if (current->next) {
        current->next->prev = new_text;
        new_text->next = current->next;
    } else {
        new_text->next = NULL;
    }
    if (current) {
        current->next = new_text;
    }
    new_text->prev = current;
    new_text->line = (struct line *)malloc(sizeof(struct line));
    new_text->line->next = NULL;
    new_text->line->byte_count = 0;
    return new_text;
}

/*
 * text_malloc
 * malloc head
 * return head
 */
struct text *text_malloc(void) {
    struct text *head = (struct text *)malloc(sizeof(struct text));
    head->prev = NULL;
    head->next = NULL;
    head->line = (struct line *)malloc(sizeof(struct line));
    head->line->next = NULL;
    head->line->byte_count = 0;
    return head;
}

/*
 * mbchar_malloc
 * malloc size of multi byte
 * return memory
 */
mbchar mbchar_malloc(void) {
    return (mbchar)malloc(sizeof(char) * MB_CUR_MAX);
}

/*
 * mbchar_free
 * free multi byte
 */
void mbchar_free(mbchar mbchar) {
    return free(mbchar);
}

/*
 * mbchar_zero_clear
 * fill mbchar with \0
 */
mbchar mbcher_zero_clear(mbchar mbchar) {
    int i = MB_CUR_MAX;
    while(i--)
        mbchar[i] = '\0';
    return mbchar;
}

/*
 * mbchar_size
 * return size of multi byte char
 */
int mbchar_size(mbchar mbchar) {
    return mblen(mbchar, MB_CUR_MAX);
}

/*
 * is_LineBreak
 * return 1 if char is \n
 */
int isLineBreak(mbchar mbchar) {
    if (mbchar[0] == '\n')
        return 1;
    return 0;
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
struct text *file_read(char *filename) {
	FILE* fp;
	
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "file open error\n");
		exit(EXIT_FAILURE);
	}

    struct text *head = text_malloc();
    struct text *current_text = head;
    struct line *current_line = head->line;

	mbchar buf = mbchar_malloc();
    char c;
    mbcher_zero_clear(buf);
    int len = 0;
    while(1) {
        c = (char)fgetc(fp);
        if (feof(fp))
            break;
        buf[len] = c;
        if (mbchar_size(buf) > 0) {
            line_add_char(current_line, buf);
            if (isLineBreak(buf)) {
                current_text = text_insert(current_text);
                current_line = current_text->line;
            }
            mbcher_zero_clear(buf);
            len = 0;
        } else if (mbchar_size(buf) <= 0) {
            // not valid
            len++;
        }
    }
    mbchar_free(buf);
    fclose(fp);
    return head;
}

/*
 * context_read_file
 * store contents of filename to the members of struct context
 */
void context_read_file(struct context *context, char *filename) {
    context->filename = (char *)malloc(sizeof(filename));
    strcpy(context->filename, filename);
    context->text = file_read(context->filename);
}

/*
 * render_header
 * output header with white background, width is windowsize
 */
void render_header(struct context_header context) {
    // space_num of space are addeed to the tail of message
    int space_num = context.view_size.width - strlen(context.message);
    backcolor_white(1);
    printf("%s",context.message);
    while(space_num-- > 0)
		printf(" ");
    backcolor_white(0);
    printf("\n");
}

/*
 * output contents of context
 */
void render(struct context context) {
    struct view_size view_size = console_size();
    struct context_header context_header;
    context_header.message = context.filename;
    context_header.view_size = view_size;
    
    clear();
    render_header(context_header);
    debug_print_text(context);
}

void debug_print_text(struct context context) {
    struct text *current_text = context.text;
	struct line *current_line = context.text->line;
    
    int i;
    while (current_text) {
        current_line = current_text->line;
        while (current_line) {
            i = 0;
            printf("[%d]",current_line->byte_count);
            while (i < current_line->byte_count) {
                printf("%c", current_line->string[i]);
                i++;
            }
            current_line = current_line->next;
            if (current_line)
                printf(" -> ");
        }
        current_text = current_text->next;
        if (current_text)
            printf("-------\n");
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
