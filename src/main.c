#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define BUFFER_SIZE 100 

void clear(void);

struct termios cookedMode, rawMode; 

struct string {
	char str[BUFFER_SIZE];
	struct string *prev;
	struct string *next;
};

/*
 * toをmallocしてfromの次に挿入
 * 返り値　to
 */
struct string *insert(struct string *from) {
	struct string* to = (struct string*)malloc(sizeof(struct string));
	if (from->next) {
		from->next->prev = to;
		to->next = from->next;
	} else {
		to->next = NULL;
	}
	if (from) {
		from->next = to;
	}
	to->prev = from;
	return to;
}

void file_read(char* filename, struct string* head){
	FILE* fp;
	char buf[BUFFER_SIZE];
	
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "file open error\n");
		exit(1);
	}
	struct string* current = head;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strcpy(current->str, buf);
		insert(current);
		current = current->next;
	}
	fclose(fp);
}
		
int main(int argc, char *argv[]) {
	struct termios non_canon, term_org;

	struct string* head = (struct string*)malloc(sizeof(struct string));
	head->prev = NULL;
	head->next = NULL;
	if (argc != 2) {
		printf("usage: ./main filename\n");
	} else {
		int line_num = 1;
		file_read(argv[1], head);
		struct string* current = head;

        int input_key;
		// 元の設定を保存
		if (tcgetattr(STDIN_FILENO, &term_org) != 0)
		    perror("tcgetatt() error");
		
	    cfmakeraw(&non_canon);
		tcsetattr(STDIN_FILENO, TCSANOW, &non_canon);
		while(current) {
			clear();
			printf("%s", current->str);
			printf("\n\rline%d\n", line_num);
			if (!current->next)
				printf("\n\r(END)");

			input_key = getchar();
			if (input_key == 'n') {
				if (current->next) {
					current = current->next;
					line_num++;
				}
			}
			if (input_key == 'p') {
				if (current->prev) {
					current = current->prev;
					line_num--;
				}
			}
			if (input_key == 'e') {
				clear();
				break;
			}
        }
	    tcsetattr(STDIN_FILENO, TCSANOW, &term_org);
    }
	return 0;
}

/*
 * ターミナルをクリアする関数
 */
void clear(void) {
    // 2J: 画面全体消去
    // H = 1;1H: カーソルを1行目1列目
    printf("\e[2J\e[H");
}