#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

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

/* プロトタイプ宣言 */
void clear(void);
struct string *insert(struct string *current);
struct string *file_read(char *filename);
void context_set_filename(struct context *context, char *filename);

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
        exit(EXIT_SUCCESS);
    }
}

/*
 * new_strをmallocしてcurrentの次に挿入
 * 返り値　new_str
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
 * file_read
 * ファイルをstruct stringに読み込む関数
 */
struct string *file_read(char *filename) {
	FILE* fp;
	char buf[BUFFER_SIZE];
	
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "file open error\n");
		exit(1);
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
 * 引数struct contextのメンバーにfilenameの中身を格納
 * 
 */
void context_set_filename(struct context *context, char *filename) {
    context->filename = (char *)malloc(sizeof(filename));
    strcpy(context->filename, filename);
    context->filestr = file_read(context->filename);
}

/*
 * ターミナルをクリアする関数
 */
void clear(void) {
    // 2J: 画面全体消去
    // H = 1;1H: カーソルを1行目1列目
    printf("\e[2J\e[H");
}
