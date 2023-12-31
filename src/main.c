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

enum CommandType {NONE, INSERT, DELETE, ENTER, UP, DOWN, LEFT, RIGHT, SAVE_OVERRIDE, EXIT};
enum ControlKeyFlag {NOT_CTRL, ALLOW_1, ALLOW_2};

/* divided by \n, single link */
struct line {
    unsigned int byte_count;
    unsigned int position_count;
    unsigned char string[BUFFER_SIZE];
    struct line *next;
};

/* list of line, double link */
struct text {
    unum width_count;
    unum position_count;
	struct line *line;
    struct text *prev;
    struct text *next;
};

struct cursor {
    unum position_x;
    unum position_y;
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

struct context_footer {
    struct view_size view_size;
    unsigned char *message;
};

// render_start_height for scroll
struct context {
	char *filename;
	struct text *text;
    struct cursor cursor;
    struct view_size view_size;
    unsigned int header_height;
    unsigned int body_height;
    unsigned int footer_height;
    unsigned int render_start_height;
};

struct command {
    enum CommandType command_key;
    mbchar command_value;
};

/* prototype declaration */
void clear(void);
struct line *line_insert(struct line *current);
void line_add_char(struct line *head, mbchar mc);
struct text *text_insert(struct text *current);
struct text *text_malloc(void);
void text_free(struct text* text);
void text_combine_next(struct text* current);
void text_divide(struct text *current_text, struct line *current, unsigned int byte, mbchar divide_char);
struct text *getTextFromPositionY(struct text *head, unum position_y);
struct line *getLineAndByteFromPositionX(struct line *head, unum position_x, unsigned int *byte);
mbchar get_tail(struct line *line);
void insert_mbchar(struct line *line, unsigned int byte, mbchar c);
void delete_mbchar(struct line *line, unsigned int byte);
void calculation_width(struct text *head, unsigned int max_width);
mbchar mbchar_malloc(void);
void mbchar_free(mbchar mbchar);
mbchar mbcher_zero_clear(mbchar mbchar);
int mbchar_size(mbchar mbchar, unsigned int len);
unsigned int safed_mbchar_size(mbchar mbchar);
int is_line_break(mbchar mbchar);
unsigned int mbchar_width(mbchar mbchar) ;
unum string_width(unsigned char *message) ;
struct text *file_read(const char *filename);
void context_read_file(struct context *context, char *filename);
void context_write_override_file(struct context *context);
void file_write(const char* filepath, struct text *head);
unsigned char get_single_byte_key(void);
void color_cursor(int bool);
mbchar keyboard_scan(mbchar *out);
struct command command_parse(mbchar key);
void vailidate_cursor_position(struct context *context);
void command_perform(struct command command, struct context *context);
void render_header(struct context_header context);
void render_footer(struct context_footer context);
void vailidate_render_position(struct context *context);
void render_setting(struct context *context);
void render(struct context context);
void render_body(struct context context);
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
        context.cursor.position_x = 1;
        context.cursor.position_y = 1;
        context.render_start_height = 0;
        mbchar key = mbchar_malloc();
        struct command cmd_none;
        cmd_none.command_key = NONE;
        command_perform(cmd_none, &context);
        while (1) {
            render_setting(&context);
            render(context);
            keyboard_scan(&key);
            struct command cmd = command_parse(key);
            command_perform(cmd, &context);
            // render(context);
        }
        mbchar_free(key);
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
    new_text->prev = current;
    new_text->next = NULL;
	if (current) {
        if (current->next) {
            current->next->prev = new_text;
			new_text->next = current->next;
        }
        current->next = new_text;
    }
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
 * text_free
 * free text and joint around
 */
void text_free(struct text* text) {
    struct text *prev = text->prev;
    struct text *next = text->next;
    prev->next = next;
    next->prev = prev;
    free(text);
}

/*
 * text_combine_next
 * combine beyond line
 */
void text_combine_next(struct text* current) {
    struct line *tail = current->line;
    while (tail->next)
        tail = tail->next;
    delete_mbchar(tail, tail->byte_count - safed_mbchar_size(get_tail(tail)));
    tail->next = current->next->line;
    text_free(current->next);
}

/*
 * text_divide
 * for enter
 */
void text_divide(struct text *current_text, struct line *current_line, unsigned int byte, mbchar divide_char) {
    text_insert(current_text);
    // next is no contents
    if (current_line->next) {
        free(current_text->next->line);
        current_text->next->line = current_line->next;
        current_line->next = NULL;
    }
    while (current_line->byte_count > byte) {
        mbchar tail = get_tail(current_line);
        current_line->byte_count -= safed_mbchar_size(tail);
        insert_mbchar(current_text->next->line, 0, tail);
    }
    line_add_char(current_line, divide_char);
}

/*
 * getTextFromPositionY
 * get pos_y of head line
 */
struct text *getTextFromPositionY(struct text *head, unum position_y) {
    unum i = position_y - 1;
    struct text *current_text = head;
    while (i-- > 0 && current_text)
        current_text = current_text->next;
	if (current_text)
        return current_text;
	return NULL;
}

/*
 * getLineAndByteFromPositionX
 * head maybe start
 */
struct line *getLineAndByteFromPositionX(struct line *head, unum position_x, unsigned int *byte) {
	unum i = position_x;
	struct line *current_line = head;
    while (current_line && i > current_line->position_count) {
        // before update
        i -= current_line->position_count;
        current_line = current_line->next;
    }
    if (current_line) {
        *byte = 0;
        while (i-- > 1)
            *byte += safed_mbchar_size(&current_line->string[*byte]);
        return current_line;
    }
    return NULL;
}

/*
 * get_tail
 * return last str
 */
mbchar get_tail(struct line *line) {
    unsigned int i = 0;
    // > so stop at tail pos 
    while (line->byte_count > i + safed_mbchar_size(&line->string[i]))
        i += safed_mbchar_size(&line->string[i]);
    return &line->string[i];
}

/*
 * insert_mbchar
 * insert arg byte size char c
 */
void insert_mbchar(struct line *line, unsigned int byte, mbchar c) {
    unsigned int s = safed_mbchar_size(c);
    // exist next and both line don't have mbchar area 
    if (line->byte_count + UTF8_MAX_BYTE >= BUFFER_SIZE)
        line_insert(line);
    // shortage of buf
    while (BUFFER_SIZE <= line->byte_count + s) {
        mbchar tail = get_tail(line);
        line->byte_count -= safed_mbchar_size(tail);
        insert_mbchar(line->next, 0, tail);
    }
    unsigned int move = line->byte_count;
    // slide char
    while (move > byte) {
        move--;
        line->string[move + s] = line->string[move];
    }
    
    unsigned int offset = 0;
    while (offset < s) {
        line->string[byte + offset] = c[offset];
        line->byte_count++;
        offset++;
    }
}

/*
 * delete_mbchar
 * delete byte pos mbchar
 */
void delete_mbchar(struct line *line, unsigned int byte) {
    unsigned int s = safed_mbchar_size(&line->string[byte]);
    unsigned int move = byte;
    while (move < line->byte_count) {
        line->string[move] = line->string[move + s];
        move++;
    }
    line->byte_count -= s;
}

/*
 * calculation_width
 * calc view height
 * total_width is per line
 */
void calculation_width(struct text *head, unsigned int max_width) {
    static unsigned int prev_width = 0;
    prev_width = max_width;
	struct text *current_text = head;
	struct line *current_line = head->line;
    
    unsigned int i;
    while (current_text) {
        unum total_width = 0;
        unum total_position = 0;
        current_line = current_text->line;
        while (current_line) {
            int line_position = 0;
            i = 0;
            while (i < current_line->byte_count) {
                int w = mbchar_width(&current_line->string[i]);
                total_width += w;
                line_position++;
                i += safed_mbchar_size(&current_line->string[i]);
            }
            current_line->position_count = line_position;
            total_position += line_position;
            current_line = current_line->next;
        }
        current_text->width_count = total_width;
        current_text->position_count = total_position;
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
 * is_line_break
 * return 1 if char is \n
 */
int is_line_break(mbchar mbchar) {
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
            if (is_line_break(buf)) {
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
 * file_write
 * write file from head
 */
void file_write(const char* filepath, struct text *head) {
    FILE *fp;
    if ((fp = fopen(filepath, "w")) == NULL) {
        printf("[error]can't open file\n");
        exit(EXIT_FAILURE);
    }
    
    struct text *current_text = head;
	struct line *current_line = head->line;
    unum i;
    while (current_text) {
        current_line = current_text->line;
        while (current_line) {
            i = 0;
            while (i < current_line->byte_count) {
                fputc(current_line->string[i], fp);
                i++;
            }
            current_line = current_line->next;
        }
        current_text = current_text->next;
    }
	fclose(fp);
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
 * context_write_override_file
 * call file_write
 */
void context_write_override_file(struct context *context) {
    file_write(context->filename, context->text);
}

/*
 * get_single_byte_key
 * if is_init, make non_canon, read and put back
 */
unsigned char get_single_byte_key(void) {
    static int is_init = 0;
    static struct termios term_org;
    static struct termios non_canon;
    if (!is_init) {
        tcgetattr(STDIN_FILENO, &term_org);
        non_canon = term_org;
        cfmakeraw(&non_canon);
    }
    unsigned char key;
    tcsetattr(STDIN_FILENO, 0, &non_canon);
    key = getchar();
    tcsetattr(STDIN_FILENO, 0, &term_org);
    return key;
}

/*
 * color_cursor
 * if bool, change cursor pink
 */
void color_cursor(int bool) {
    if (bool)
        // pink
        printf("\e[30m\e[45m");
    else
        printf("\e[m");
}

/*
 * keyboard_scan
 * store scan to arg out
 */
mbchar keyboard_scan(mbchar *out) {
    mbcher_zero_clear(*out);
    unsigned int i = 0;
    while (mbchar_size(*out, i) < 0) {
        *out[i] = get_single_byte_key();
        i++;
        if (mbchar_size(*out, i) == MBCHAR_ILLIEGAL) {
            mbcher_zero_clear(*out);
        }
    }
    return *out;
}

/*
 * command_parse
 * return kind of arg key
 */
struct command command_parse(mbchar key) {
    static enum ControlKeyFlag flag = 0;
    struct command cmd;
    cmd.command_key = NONE;
    cmd.command_value = key;
    if (key[0] == 0x1B && flag == NOT_CTRL)
        flag = ALLOW_1;
    else if (key[0] == 0x5B && flag == ALLOW_1)
        flag = ALLOW_2;
    else if (flag == ALLOW_2) {
        if (key[0] == 0x41)
            cmd.command_key = UP;
        else if (key[0] == 0x42)
            cmd.command_key = DOWN;
        else if (key[0] == 0x43)
            cmd.command_key = RIGHT;
        else if (key[0] == 0x44)
            cmd.command_key = LEFT;
        flag = NOT_CTRL;
    } else {
        if (key[0] == 0x11)
            cmd.command_key = EXIT;
        else if (key[0] == 0x7F)
            cmd.command_key = DELETE;
        else if (key[0] == 0x0D) {
            cmd.command_key = ENTER;
            cmd.command_value = (mbchar)"\n";
        }
        else if (key[0] == 0x20) {
            cmd.command_key = INSERT;
            cmd.command_value=(mbchar)" ";
        }
        else if (key[0] == 0x13)
            cmd.command_key = SAVE_OVERRIDE;
        else
            cmd.command_key = INSERT;
        flag = NOT_CTRL;
    }        
    return cmd;
}

/*
 * vailidate_cursor_position
 * regulate leftest, rightest
 */
void vailidate_cursor_position(struct context *context) {
    if (context->cursor.position_x < 1)
        context->cursor.position_x = 1;
    if (context->cursor.position_y < 1)
        context->cursor.position_y = 1;
    while (!getTextFromPositionY(context->text, context->cursor.position_y))
        context->cursor.position_y -= 1;
    
    unum max_x = getTextFromPositionY(context->text, context->cursor.position_y)->position_count - 1;
    if (context->cursor.position_x > max_x)
        context->cursor.position_x = max_x;
}


/*
 * command_perform
 * change state of arg context
 */
void command_perform(struct command command, struct context *context) {
    switch (command.command_key) {
    case UP:
        context->cursor.position_y -= 1;
        break;
    case DOWN:
        context->cursor.position_y += 1;
        break;
    case RIGHT:
        context->cursor.position_x += 1;
        break;
    case LEFT:
        context->cursor.position_x -= 1;
        break;
    case EXIT:
        exit(EXIT_SUCCESS);
        break;
    case INSERT:
        {
        unsigned int byte;
        struct text *head = getTextFromPositionY(context->text, context->cursor.position_y);
        struct line *line = getLineAndByteFromPositionX(head->line, context->cursor.position_x, &byte);
        insert_mbchar(line, byte, command.command_value);
        context->cursor.position_x += 1;
        }
        break;
    case DELETE:
        {
        if (context->cursor.position_x > 1) {
            unsigned int byte;
            struct text *head = getTextFromPositionY(context->text, context->cursor.position_y);
            struct line *line = getLineAndByteFromPositionX(head->line, context->cursor.position_x - 1, &byte);
            delete_mbchar(line, byte);
            context->cursor.position_x -= 1;
        } else if (context->cursor.position_y > 1) {
            // pos x is 1 and line is not top
            struct text *head = getTextFromPositionY(context->text, context->cursor.position_y - 1);
            text_combine_next(head);
            context->cursor.position_x = getTextFromPositionY(context->text, context->cursor.position_y - 1)->position_count;
            context->cursor.position_y -= 1;
        }
        }
        break;
    case ENTER:
        {
        unsigned int byte;
        struct text *head = getTextFromPositionY(context->text, context->cursor.position_y);
        struct line *line = getLineAndByteFromPositionX(head->line, context->cursor.position_x, &byte);
        text_divide(head, line, byte, command.command_value);
        context->cursor.position_x = 1;
        context->cursor.position_y += 1;
        }
        break;
    case SAVE_OVERRIDE:
        context_write_override_file(context);
        break;
    case NONE:
        break;
    }
    calculation_width(context->text, context->view_size.width);
    vailidate_cursor_position(context);
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

void render_footer(struct context_footer context) {
    backcolor_white(1);
    printf(" ");
    trim_print(context.message, context.view_size.width - 2);
    printf(" ");
    backcolor_white(0);
    printf("\n");
}

/* 
 * vailidate_render_position
 * match cursor_position and render_start_height
 */
void vailidate_render_position(struct context *context) {
    while (context->cursor.position_y <= context->render_start_height)
        context->render_start_height -= 1;
    while(context->cursor.position_y >= context->render_start_height + context->body_height)
        context->render_start_height += 1;
}

/*
 * render_setting
 * init context
 */
void render_setting(struct context *context) {
    struct view_size view_size = console_size();
    context->view_size = view_size;
    context->header_height = 1;
    // for header and footer
    context->body_height = context->view_size.height - 2;
    context->footer_height = 1;
    vailidate_render_position(context);
}

/*
 * output contents of context
 */
void render(struct context context) {
    struct context_header context_header;
    context_header.message = (unsigned char *)context.filename;
    context_header.view_size = context.view_size;
    struct context_footer context_footer;
    unsigned char pathname[256];
	getcwd((char *)pathname, 256);
    context_footer.message = pathname;    context_footer.view_size = context.view_size;
    clear();
    render_header(context_header);
    render_body(context);
    render_footer(context_footer);
    //debug_print_text(context);
}

/*
 * render_body
 * output with color cursor
 */
void render_body(struct context context) {
    int height = context.body_height;
    int render_max_height = context.render_start_height + height;
    struct text *current_text = context.text;
    struct line *current_line = context.text->line;
    
    unum pos_x = 1;
    unum pos_y = 1;
    unum wrote_byte;
    int cursor_color_flag = 0;
    while (current_text) {
        current_line = current_text->line;
        while (current_line) {
            wrote_byte = 0;
            // brank line
            if (current_text->position_count <= 1 && context.cursor.position_y == pos_y) {
                color_cursor(1);
                printf(" ");
                color_cursor(0);
            }
            while (wrote_byte < current_line->byte_count) {
                if (render_max_height > pos_y && pos_y > context.render_start_height) {
                    if (cursor_color_flag) {
                        color_cursor(0);
                        cursor_color_flag = 0;
                    }
                    if (context.cursor.position_x == pos_x && context.cursor.position_y == pos_y) {
                        color_cursor(1);
                        cursor_color_flag = 1;
                    }
                    wrote_byte += print_one_mbchar(&(current_line->string[wrote_byte]));
                    pos_x++;
                } else {
                    wrote_byte++;
                }
            }
            current_line = current_line->next;
        }
        current_text = current_text->next;
        pos_y++;
        height--;
        pos_x = 1;
    }
    while(height > 0) {
        printf("\n");
        height--;
    }
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
    printf("\n---debug---\n");
    struct text *current_text = context.text;
	struct line *current_line = context.text->line;
    
    unum i;
    while (current_text) {
        current_line = current_text->line;
        printf("#%lluw %llup", current_text->width_count, current_text->position_count);
        while (current_line) {
            i = 0;
            printf("[%dp, %dp]",current_line->byte_count, current_line->position_count);
            while (i < current_line->byte_count) {
                if (is_line_break(&current_line->string[i]))
                    printf("<BR>");
                else
                    printf("%c", current_line->string[i]);
                i++;
            }
            current_line = current_line->next;
            if (current_line)
                printf(" -> ");
        }
        current_text = current_text->next;
        if (current_text)
            printf("\n-------\n");
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
