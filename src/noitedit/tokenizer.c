/*	$NetBSD: tokenizer.c,v 1.7 2001/01/04 15:56:32 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "noitedit/compat.h"
#if !defined(lint) && !defined(SCCSID)
#if 0
static char sccsid[] = "@(#)tokenizer.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: tokenizer.c,v 1.7 2001/01/04 15:56:32 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * tokenize.c: Bourne shell like tokenizer
 */
#include "noitedit/sys.h"
#include <string.h>
#include <stdlib.h>
#include "noitedit/tokenizer.h"

typedef enum {
	Q_none, Q_single, Q_double, Q_one, Q_doubleone
} quote_t;

#define	IFS		"\t \n"

#define	TOK_KEEP	1
#define	TOK_EAT		2

#define	WINCR		20
#define	AINCR		10

#define	tok_malloc(a)		malloc(a)
#define	tok_free(a)		free(a)
#define	tok_realloc(a, b)	realloc(a, b)


struct tokenizer {
	char	*ifs;		/* In field separator			 */
	int	 argc, amax;	/* Current and maximum number of args	 */
	char   **argv;		/* Argument list			 */
	char	*wptr, *wmax;	/* Space and limit on the word buffer	 */
	char	*wstart;	/* Beginning of next word		 */
	char	*wspace;	/* Space of word buffer			 */
	quote_t	 quote;		/* Quoting state			 */
	int	 flags;		/* flags;				 */
};


private void tok_finish(Tokenizer *);


/* tok_finish():
 *	Finish a word in the tokenizer.
 */
private void
tok_finish(Tokenizer *tok)
{

	*tok->wptr = '\0';
	if ((tok->flags & TOK_KEEP) || tok->wptr != tok->wstart) {
		tok->argv[tok->argc++] = tok->wstart;
		tok->argv[tok->argc] = NULL;
		tok->wstart = ++tok->wptr;
	}
	tok->flags &= ~TOK_KEEP;
}


/* tok_init():
 *	Initialize the tokenizer
 */
public Tokenizer *
tok_init(const char *ifs)
{
	Tokenizer *tok = (Tokenizer *) tok_malloc(sizeof(Tokenizer));

	tok->ifs = strdup(ifs ? ifs : IFS);
	tok->argc = 0;
	tok->amax = AINCR;
	tok->argv = (char **) tok_malloc(sizeof(char *) * tok->amax);
	if (tok->argv == NULL)
		return (NULL);
	tok->argv[0] = NULL;
	tok->wspace = (char *) tok_malloc(WINCR);
	if (tok->wspace == NULL)
		return (NULL);
	tok->wmax = tok->wspace + WINCR;
	tok->wstart = tok->wspace;
	tok->wptr = tok->wspace;
	tok->flags = 0;
	tok->quote = Q_none;

	return (tok);
}


/* tok_reset():
 *	Reset the tokenizer
 */
public void
tok_reset(Tokenizer *tok)
{

	tok->argc = 0;
	tok->wstart = tok->wspace;
	tok->wptr = tok->wspace;
	tok->flags = 0;
	tok->quote = Q_none;
}


/* tok_end():
 *	Clean up
 */
public void
tok_end(Tokenizer *tok)
{

	tok_free((ptr_t) tok->ifs);
	tok_free((ptr_t) tok->wspace);
	tok_free((ptr_t) tok->argv);
	tok_free((ptr_t) tok);
}



/* tok_line():
 *	Bourne shell like tokenizing
 *	Return:
 *		-1: Internal error
 *		 3: Quoted return
 *		 2: Unmatched double quote
 *		 1: Unmatched single quote
 *		 0: Ok
 */
public int
tok_line(Tokenizer *tok, const char *line, int *argc, char ***argv)
{
	const char *ptr;

	for (;;) {
		switch (*(ptr = line++)) {
		case '\'':
			tok->flags |= TOK_KEEP;
			tok->flags &= ~TOK_EAT;
			switch (tok->quote) {
			case Q_none:
				tok->quote = Q_single;	/* Enter single quote
							 * mode */
				break;

			case Q_single:	/* Exit single quote mode */
				tok->quote = Q_none;
				break;

			case Q_one:	/* Quote this ' */
				tok->quote = Q_none;
				*tok->wptr++ = *ptr;
				break;

			case Q_double:	/* Stay in double quote mode */
				*tok->wptr++ = *ptr;
				break;

			case Q_doubleone:	/* Quote this ' */
				tok->quote = Q_double;
				*tok->wptr++ = *ptr;
				break;

			default:
				return (-1);
			}
			break;

		case '"':
			tok->flags &= ~TOK_EAT;
			tok->flags |= TOK_KEEP;
			switch (tok->quote) {
			case Q_none:	/* Enter double quote mode */
				tok->quote = Q_double;
				break;

			case Q_double:	/* Exit double quote mode */
				tok->quote = Q_none;
				break;

			case Q_one:	/* Quote this " */
				tok->quote = Q_none;
				*tok->wptr++ = *ptr;
				break;

			case Q_single:	/* Stay in single quote mode */
				*tok->wptr++ = *ptr;
				break;

			case Q_doubleone:	/* Quote this " */
				tok->quote = Q_double;
				*tok->wptr++ = *ptr;
				break;

			default:
				return (-1);
			}
			break;

		case '\\':
			tok->flags |= TOK_KEEP;
			tok->flags &= ~TOK_EAT;
			switch (tok->quote) {
			case Q_none:	/* Quote next character */
				tok->quote = Q_one;
				break;

			case Q_double:	/* Quote next character */
				tok->quote = Q_doubleone;
				break;

			case Q_one:	/* Quote this, restore state */
				*tok->wptr++ = *ptr;
				tok->quote = Q_none;
				break;

			case Q_single:	/* Stay in single quote mode */
				*tok->wptr++ = *ptr;
				break;

			case Q_doubleone:	/* Quote this \ */
				tok->quote = Q_double;
				*tok->wptr++ = *ptr;
				break;

			default:
				return (-1);
			}
			break;

		case '\n':
			tok->flags &= ~TOK_EAT;
			switch (tok->quote) {
			case Q_none:
				tok_finish(tok);
				*argv = tok->argv;
				*argc = tok->argc;
				return (0);

			case Q_single:
			case Q_double:
				*tok->wptr++ = *ptr;	/* Add the return */
				break;

			case Q_doubleone:   /* Back to double, eat the '\n' */
				tok->flags |= TOK_EAT;
				tok->quote = Q_double;
				break;

			case Q_one:	/* No quote, more eat the '\n' */
				tok->flags |= TOK_EAT;
				tok->quote = Q_none;
				break;

			default:
				return (0);
			}
			break;

		case '\0':
			switch (tok->quote) {
			case Q_none:
				/* Finish word and return */
				if (tok->flags & TOK_EAT) {
					tok->flags &= ~TOK_EAT;
					return (3);
				}
				tok_finish(tok);
				*argv = tok->argv;
				*argc = tok->argc;
				return (0);

			case Q_single:
				return (1);

			case Q_double:
				return (2);

			case Q_doubleone:
				tok->quote = Q_double;
				*tok->wptr++ = *ptr;
				break;

			case Q_one:
				tok->quote = Q_none;
				*tok->wptr++ = *ptr;
				break;

			default:
				return (-1);
			}
			break;

		default:
			tok->flags &= ~TOK_EAT;
			switch (tok->quote) {
			case Q_none:
				if (strchr(tok->ifs, *ptr) != NULL)
					tok_finish(tok);
				else
					*tok->wptr++ = *ptr;
				break;

			case Q_single:
			case Q_double:
				*tok->wptr++ = *ptr;
				break;


			case Q_doubleone:
				*tok->wptr++ = '\\';
				tok->quote = Q_double;
				*tok->wptr++ = *ptr;
				break;

			case Q_one:
				tok->quote = Q_none;
				*tok->wptr++ = *ptr;
				break;

			default:
				return (-1);

			}
			break;
		}

		if (tok->wptr >= tok->wmax - 4) {
			size_t size = tok->wmax - tok->wspace + WINCR;
			char *s = (char *) tok_realloc(tok->wspace, size);
			/* SUPPRESS 22 */
			int offs = s - tok->wspace;
			if (s == NULL)
				return (-1);

			if (offs != 0) {
				int i;
				for (i = 0; i < tok->argc; i++)
					tok->argv[i] = tok->argv[i] + offs;
				tok->wptr = tok->wptr + offs;
				tok->wstart = tok->wstart + offs;
				tok->wmax = s + size;
				tok->wspace = s;
			}
		}
		if (tok->argc >= tok->amax - 4) {
			char **p;
			tok->amax += AINCR;
			p = (char **) tok_realloc(tok->argv,
			    tok->amax * sizeof(char *));
			if (p == NULL)
				return (-1);
			tok->argv = p;
		}
	}
}
