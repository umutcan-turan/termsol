#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"

#define MAX_CARDS 52

/* structs */
struct card {
	char suit;
	char val;
	char hide;
};

struct c_stack {
	struct card cards[MAX_CARDS];
	int len;
};

/* function prototypes */
void print_card(struct card *c, int force_hide, int empty);
void print_board(void);
int revealed_cards(struct c_stack *cs);
void card_add(struct c_stack *cs, struct card *c);
struct card *card_get(struct c_stack *cs, int i);
void card_move(struct c_stack *dst, struct c_stack *src, int amnt);

/* globals */
struct c_stack deck;
struct c_stack remain;
struct c_stack waste;
struct c_stack foundation[4];
struct c_stack pile[7];
struct c_stack selection;

struct c_stack *last;

int running;
int newgame;
int display_help;

/* suitstr[x] % 2 ? black : red
 * order matters
 */
static const char *suitstr[] = {
	"♦",
	"♣",
	"♥",
	"♠",
};

static const char *valstr[] = {
	" A",
	" 2",
	" 3",
	" 4",
	" 5",
	" 6",
	" 7",
	" 8",
	" 9",
	"10",
	" J",
	" Q",
	" K"
};

/* function implementations */
struct c_stack *key_to_cs(char key)
{
	switch (key) {
	case KEY_WASTE:
		return &waste;
	case KEY_FOUNDATION1:
		return foundation;
	case KEY_FOUNDATION2:
		return foundation + 1;
	case KEY_FOUNDATION3:
		return foundation + 2;
	case KEY_FOUNDATION4:
		return foundation + 3;
	case KEY_PILE1:
		return pile;
	case KEY_PILE2:
		return pile + 1;
	case KEY_PILE3:
		return pile + 2;
	case KEY_PILE4:
		return pile + 3;
	case KEY_PILE5:
		return pile + 4;
	case KEY_PILE6:
		return pile + 5;
	case KEY_PILE7:
		return pile + 6;
	default:
		return NULL;
	}
	/* no reach */
	return NULL;
}

int cs_is_pile(struct c_stack *cs)
{
	int i;

	for (i = 0; i < 7; i++)
		if (cs == (pile + i))
			return 1;
	return 0;
}

int cs_is_foundation(struct c_stack *cs)
{
	int i;

	for (i = 0; i < 4; i++)
		if (cs == (foundation + i))
			return 1;
	return 0;
}

void print_card(struct card *c, int force_hide, int empty)
{
	if (empty) {
		printf("%4s", "");
	} else if (force_hide) {
		printf("%4s", "XX");
	} else if (!c) {
		printf("%4s", "--");
	} else if (c->hide) {
		printf("%4s", "XX");
	} else {
		printf(" %s%s%s\033[0m",
			valstr[c->val],
			c->suit % 2 ? "" : "\033[0;31m",
			suitstr[c->suit]);
	}
}

void print_board(void)
{
	int i, j, maxl;
	
	printf("\033[2J\033[H");
	if (display_help)
		printf("%8c%8c%4c%4c%4c\n",
			KEY_WASTE,
			KEY_FOUNDATION1,
			KEY_FOUNDATION2,
			KEY_FOUNDATION3,
			KEY_FOUNDATION4);
	print_card(NULL, 1, remain.len ? 0 : 1);
	print_card(card_get(&waste, -1), 0, 0);
	print_card(NULL, 0, 1);
	for (i = 0; i < 4; i++)
		print_card(card_get(&foundation[i], -1), 0, 0);
	putchar('\n');
	putchar('\n');
	for (maxl = i = 0; i < 7; i++)
		if (pile[i].len > maxl)
			maxl = pile[i].len;
	if (selection.len > maxl)
		maxl = selection.len;
	for (i = 0; i < maxl; i++) {
		for (j = 0; j < 7; j++)
			print_card(card_get(&pile[j], i), 0, i && (pile[j].len <= i) ? 1 : 0);
		print_card(card_get(&selection, i), 0, i && (selection.len <= i) ? 1 : 0);
		putchar('\n');
	}
	if (display_help)
		printf("%3c%4c%4c%4c%4c%4c%4c\n",
			KEY_PILE1,
			KEY_PILE2,
			KEY_PILE3,
			KEY_PILE4,
			KEY_PILE5,
			KEY_PILE6,
			KEY_PILE7);
}

/* Test cards from starting at the last one,
 * count up the revealed ones and stop at first hidden card */
int revealed_cards(struct c_stack *cs)
{
	int rev;

	for (rev = 0; (rev < cs->len) && !(card_get(cs, -1 - rev))->hide; rev++)
		;
	return rev;
}

/* Add a card to a c_stack */
void card_add(struct c_stack *cs, struct card *c)
{
	if (cs->len >= MAX_CARDS || !c)
		return;
	cs->cards[cs->len++] = *c;
}

void card_set(struct c_stack *cs, int i, struct card *c)
{
	struct card *dst;

	if (!c)
		return;
	dst = card_get(cs, i);
	if (dst)
		*dst = *c;
}

/* Get the address of nth card in a c_stack */
struct card *card_get(struct c_stack *cs, int i)
{
	/* Ensure we are within bounds */
	if (!cs->len || (i >= cs->len) || (i < -cs->len))
		return NULL;
	/* Negative means count from the last */
	else if (i < 0) 
		i += cs->len;
	return cs->cards + i;
}

void card_move(struct c_stack *dst, struct c_stack *src, int amnt)
{
	struct card *c;

	if (amnt < 1 || amnt > src->len || (amnt + dst->len) >= MAX_CARDS)
		return;
	memmove(dst->cards + (dst->len),
		src->cards + (src->len - amnt),
		sizeof(struct card) * amnt);
	src->len -= amnt;
	dst->len += amnt;
}

void take_card(struct c_stack *cs, int amnt)
{
	/* Only allowed to take 1 card from waste
	 * or foundations */
	if (cs == &waste || cs_is_foundation(cs))
		amnt = 1;
	/* Don't take more than available revealed
	 * cards */
	if (amnt > revealed_cards(cs))
		amnt = revealed_cards(cs);
	card_move(&selection, cs, amnt);
	last = cs;
}

int put_card(struct c_stack *cs)
{
	int valid_move;
	struct card *c1, *c2;

	if (!selection.len)
		return 0;
	c1 = card_get(&selection, 0);
	valid_move = 0;
	if (cs->len) {
		c2 = card_get(cs, -1);
		if (cs_is_pile(cs) && (c1->val + 1) == c2->val && ((c1->suit % 2) != (c2->suit % 2))) {
			/* not empty PILE, val 1 less than n
			 * and alternating color */
			valid_move = 1;
		} else if (cs_is_foundation(cs) && (selection.len == 1) && c1->val == (c2->val + 1) && c1->suit == c2->suit) {
			/* not empty FOUNDATION, val 1 more than n
			 * and same suit and single card */
			valid_move = 1;
		}
	} else {
		if (cs_is_pile(cs) && c1->val == 12) {
			/* empty PILE, K of any suit */
			valid_move = 1;
		} else if (cs_is_foundation(cs) && selection.len == 1 && c1->val == 0) {
			/* empty FOUNDATION, A of any suit
			 * single card */
			valid_move = 1;
		}
	}
	if (valid_move) {
		/* move the cards, turn face */
		card_move(cs, &selection, selection.len);
		if (last->len && card_get(last, -1)->hide)
			card_get(last, -1)->hide = 0;
		return 1;
	} else {
		/* invalid move, return cards */
		card_move(last, &selection, selection.len);
		return 0;
	}
	/* no reach */
	return -1;
}

void put_auto(void)
{
	int done, i, j;

	done = 0;
	/* easier to check WASTE on its own */
	for (i = 0; i < 4 && !done; i++) {
		take_card(&waste, 1);
		if (put_card(foundation + i))
			done = 1;
	}
	/* for each PILE... */
	for (i = 0; i < 7 && !done; i++) {
		/* check if a card can be put on any FOUNDATION... */
		for (j = 0; j < 4 && !done; j++) {
			take_card(pile + i, 1);
			/* done after 1 successful move */
			if (put_card(foundation + j))
				done = 1;
		}
	}
}

void deal_waste(void)
{
	int i, amnt;
	struct card tmp;

	if (!remain.len) {
		card_move(&remain, &waste, waste.len);
		/* reverse */
		for (i = 0; i < remain.len / 2; i++) {
			/* deref possible null! */
			tmp = *card_get(&remain, i);
			card_set(&remain, i, card_get(&remain, -1 - i));
			card_set(&remain, -1 - i, &tmp);
		}
	} else {
		amnt = remain.len < 3 ? remain.len : 3;
		while (amnt--)
			card_move(&waste, &remain, 1);
	}
}

void get_input(void)
{
	char buffer[20];
	char *s, *tv[3];
	int tc, slen;
	struct c_stack *cs;

	if (!fgets(buffer, 20, stdin))
		exit(1);
	/*
	else if ((slen = strlen(buffer)) && buffer[slen - 1] == '\n')
		buffer[slen - 1] = '\0';
	*/
	for (tc = 0, s = strtok(buffer, " "); s && tc < 3; s = strtok(NULL, " "))
		tv[tc++] = s;
	printf("tc: %d\n", tc);
	if (tc == 1 && *tv[0] == KEY_DEAL) {
		if (selection.len)
			put_card(last);
		else
			deal_waste();
	} else {
		switch (*tv[0]) {
		case KEY_QUIT:
			running = 0;
			break;
		case KEY_NEW:
			newgame = 1;
			break;
		case KEY_AUTO:
			put_auto();
			break;
		case KEY_HELP:
			display_help = !display_help;
		default:
			cs = key_to_cs(*tv[0]);
			if (!cs)
				break;
			else if (selection.len)
				put_card(cs);
			else
				take_card(cs, tc == 2 ? atoi(tv[1]) : 1);
			break;
		}
	}
}

void check_win(void)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (!foundation[i].len || (card_get(&foundation[i], -1)->val != 12))
			return;
	}
	newgame = 1;
}

void init_board(void)
{
	int i, j, rnd;
	struct card c, tmp;

	/* zero stacks */
	deck.len = remain.len = waste.len = selection.len = 0;
	for (i = 0; i < 4; i++)
		foundation[i].len = 0;
	for (i = 0; i < 7; i++)
		pile[i].len = 0;
	last = NULL;
	/* create a randomized deck */
	for (i = 0; i < MAX_CARDS; i++) {
		rnd = rand() % (i + 1);
		c.suit = i % 4;
		c.val  = i % 13;
		c.hide = 1;
		if (rnd != i)
			deck.cards[i] = deck.cards[rnd];
		deck.cards[rnd] = c;
		deck.len++;
	}
	/* deal cards into piles */
	for (i = 0; i < 7; i++) {
		card_move(&pile[i], &deck, i + 1);
		card_get(&pile[i], -1)->hide = 0;
	}
	/* put rest of deck to remain */
	card_move(&remain, &deck, deck.len);
	for (i = 0; i < remain.len; i++)
		remain.cards[i].hide = 0;
	newgame = 0;
}

int main(void)
{
	srand(time(NULL));
	display_help = 1;
	init_board();
	running = 1;
	while (running) {
		print_board();
		get_input();
		check_win();
		if (newgame)
			init_board();
	}
	return 0;
}
