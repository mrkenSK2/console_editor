#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define BUFFER_SIZE 10
#define UTF8_MAX_BYTE 6
#define MBCHAR_NULL 0
#define MBCHAR_NOT_FILL -1
#define MBCHAR_ILLIEGAL -2

typedef unsigned char* mbchar;
typedef unsigned long long unum;

/* divided by \n, single link */
struct line {
    unum byte_count;
    unsigned char string[BUFFER_SIZE];
    struct line *next;
};

/* list of line, double link */
struct text {
    unum height;
	struct line *line;
    struct text *prev;
    struct text *next;
};

struct cursor {
    unum position_x;
    unum position_y;
};

struct context {
	char *filename;
	struct text *text;
    struct cursor cursor;
};

/* panel size */
struct view_size {
    unsigned int width;
    unsigned int height;
};

/* header, white color part*/
struct context_header {
    struct view_size view_size;
    // maybe filename
    unsigned char *message;
};

/* prototype declaration */
void clear(void);
struct line *line_insert(struct line *current);
void line_add_char(struct line *head, mbchar mc);
struct text *text_insert(struct text *current);
struct text *text_malloc(void);
void calculatotion_height(struct text *head, unsigned int max_width);
mbchar mbchar_malloc(void);
void mbchar_free(mbchar mbchar);
mbchar mbcher_zero_clear(mbchar mbchar);
int mbchar_size(mbchar mbchar, unsigned int len);
unsigned int safed_mbchar_size(mbchar mbchar);
int isLineBreak(mbchar mbchar);
unsigned int mbchar_width(mbchar mbchar) ;
unum string_width(unsigned char *message) ;
struct text *file_read(const char *filename);
void context_read_file(struct context *context, char *filename);
void render_header(struct context_header context);
void render(struct context context);
unsigned int print_one_mbchar(unsigned char *str);
void trim_print(unsigned char *message, unsigned int max_width);
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
        context.cursor.position_x = 0;
        context.cursor.position_y = 0;
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
    while (current->byte_count >= BUFFER_SIZE - safed_mbchar_size(mc)) {
        if (current->next)
			current = current->next;
		else
            current = line_insert(current);
    }
    unsigned int offset = 0;
    while (offset < safed_mbchar_size(mc)) {
        current->string[current->byte_count] = mc[offset];
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
 * calculatotion_height
 * calc view height
 * total_width is per line
 */
void calculatotion_height(struct text *head, unsigned int max_width) {
    static unsigned int prev_width = 0;
    if (prev_width == max_width)
        // cache
        return;
    prev_width = max_width;
	struct text *current_text = head;
	struct line *current_line = head->line;
    
    unsigned int i;
    while (current_text) {
        unum total_width = 0;
        current_line = current_text->line;
        while (current_line) {
            i = 0;
            while (i < current_line->byte_count) {
                total_width += mbchar_width(&current_line->string[i]);
                i += safed_mbchar_size(&current_line->string[i]);
            }
            current_line = current_line->next;
        }
        current_text->height = total_width / max_width + 1;
        current_text = current_text->next;
    }
}

/*
 * mbchar_malloc
 * malloc size of multi byte
 * return memory
 */
mbchar mbchar_malloc(void) {
    return (mbchar)malloc(sizeof(unsigned char) * UTF8_MAX_BYTE);
}

/*
 * mbchar_free
 * free multi byte
 */
void mbchar_free(mbchar mbchar) {
    free(mbchar);
    mbchar = NULL;
}

/*
 * mbchar_zero_clear
 * fill mbchar with \0
 */
mbchar mbcher_zero_clear(mbchar mbchar) {
    unsigned int i = UTF8_MAX_BYTE;
    while(i--)
        mbchar[i] = '\0';
    return mbchar;
}

/*
 * mbchar_size
 * return size of multi byte char
 */
int mbchar_size(mbchar mbchar, unsigned int len) {
    if (len < 1 || UTF8_MAX_BYTE < len)
        return MBCHAR_ILLIEGAL;
    if (len == 1 && mbchar[0] == 0x00)
        return MBCHAR_NULL;
    // length of mbchar is determined by number of head 1 in byte
    unsigned int head_one_bits = 0;
    while (head_one_bits < 8) {
        if ((mbchar[0] >> (7 - head_one_bits) & 0x01) == 1)
            head_one_bits++;
        else
            break;
    }
    if (head_one_bits == 0)
        // if head 1 bit is 0, that is 1 byte char
        // for compare to len
        head_one_bits++;
    if (head_one_bits > len)
        return MBCHAR_NOT_FILL;
    if (head_one_bits == len) {
        switch(len) {
        case 1:
            return len;
            break;
        case 2:
            if (mbchar[0] & 0x1e)
                return len;
            break;
        case 3:
            if (mbchar[0] & 0x0f || mbchar[1] & 0x20)
                return len;
            break;
        case 4:
            if (mbchar[0] & 0x07 || mbchar[1] & 0x30)
                return len;
            break;
        case 5:
            if (mbchar[0] & 0x03 || mbchar[1] & 0x38)
                return len;
            break;
        case 6:
            if (mbchar[0] & 0x01 || mbchar[1] & 0x3c)
                return len;
            break;
        }
    }
    return MBCHAR_ILLIEGAL;
}

/*
 * safed_mbchar_size
 * uses only head_one_bits
 * return size of multi byte char
 */
unsigned int safed_mbchar_size(mbchar mbchar) {
    unsigned int head_one_bits = 0;
    while (head_one_bits < 8) {
        if ((mbchar[0] >> (7 - head_one_bits) & 0x01) == 1)
            head_one_bits++;
        else
            break;
    }
    if (head_one_bits == 0)
        head_one_bits++;
    return head_one_bits;
}

/*
 * isLineBreak
 * return 1 if char is \n
 */
int isLineBreak(mbchar mbchar) {
    if (mbchar[0] == '\n')
        return 1;
    return 0;
}

/*
 * mbchar_width
 * return char display width(multi is const 2)
 */
unsigned int mbchar_width(mbchar mbchar) {
    if (safed_mbchar_size(mbchar) > 1)
        return 2;
    else
        return 1;
}

/*
 * string_width
 * return string display width
 */
unum string_width(unsigned char *message) {
    unsigned int width = 0;
    long max_byte = strlen((char *)message);
    unsigned int i = 0;
    while(i < max_byte) {
        width += mbchar_width(&message[i]);
        i += safed_mbchar_size(&message[i]);
    }
    return width;
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
struct text *file_read(const char *filename) {
	FILE* fp;
	
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "file open error\n");
		exit(EXIT_FAILURE);
	}

    struct text *head = text_malloc();
    struct text *current_text = head;
    struct line *current_line = head->line;

	mbchar buf = mbchar_malloc();
    unsigned char c;
    mbcher_zero_clear(buf);
    unsigned int len = 0;
    int mbsize;
    while (1) {
        c = (unsigned char)fgetc(fp);
        if (feof(fp))
            break;
        buf[len] = c;
        mbsize = mbchar_size(buf, len + 1);
        if (mbsize > 0) {
            line_add_char(current_line, buf);
            if (isLineBreak(buf)) {
                current_text = text_insert(current_text);
                current_line = current_text->line;
            }
            mbcher_zero_clear(buf);
            len = 0;
        } else if (mbsize == MBCHAR_NOT_FILL) {
            // not valid
            len++;
        } else {
			mbcher_zero_clear(buf);
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
    backcolor_white(1);
    printf(" ");
	trim_print(context.message, context.view_size.width - 2);
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
    context_header.message = (unsigned char *)context.filename;
    context_header.view_size = view_size;
    
    clear();
    calculatotion_height(context.text, view_size.width);
    render_header(context_header);
    debug_print_text(context);
}

/*
 * print_one_mbchar
 * output one mbchar
 */
unsigned int print_one_mbchar(unsigned char *str) {
    unsigned int bytes = safed_mbchar_size(str);
    for (unsigned int i = 0; i < bytes; i++) {
        printf("%c", str[i]);
    }
    return bytes;
}

/*
 * trim_print
 * add space to tail
 */
void trim_print(unsigned char *message, unsigned int max_width) {
    unum messsage_width = string_width(message);
    if (messsage_width <= max_width) {
        printf("%s",message);
        unum i = max_width - messsage_width;
        while(i-- > 0)
            printf(" ");
    } else {
        unum wrote_bytes = 0;
        unum wrote_width = 0;
        while (max_width - wrote_width - mbchar_width(&message[wrote_bytes]) > 2) {
            wrote_width += mbchar_width(&message[wrote_bytes]);
            wrote_bytes += print_one_mbchar(&message[wrote_bytes]);
        }
        printf("...");
    }
}

void debug_print_text(struct context context) {
    struct text *current_text = context.text;
	struct line *current_line = context.text->line;
    
    unum i;
    while (current_text) {
        current_line = current_text->line;
        printf("#%llu# ", current_text->height);
        while (current_line) {
            i = 0;
            printf("[%llu]",current_line->byte_count);
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
