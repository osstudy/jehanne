/* Copyright (C) Charles Forsyth
 * See /doc/license/NOTICE.Plan9-9k.txt for details about the licensing.
 */
/* Portions of this file are Copyright (C) 2015-2018 Giacomo Tesio <giacomo@tesio.it>
 * See /doc/license/gpl-2.0.txt for details about the licensing.
 */
#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

/*
 * Generous estimate of number of fields, including terminal nil pointer
 */
static int
ncmdfield(char *p, int n)
{
	int white, nwhite;
	char *ep;
	int nf;

	if(p == nil)
		return 1;

	nf = 0;
	ep = p+n;
	white = 1;	/* first text will start field */
	while(p < ep){
		nwhite = (jehanne_strchr(" \t\r\n", *p++ & 0xFF) != 0);	/* UTF is irrelevant */
		if(white && !nwhite)	/* beginning of field */
			nf++;
		white = nwhite;
	}
	return nf+1;	/* +1 for nil */
}

/*
 *  parse a command written to a device
 */
Cmdbuf*
parsecmd(char *p, int n)
{
	Cmdbuf *volatile cb;
	int nf;
	char *sp;

	nf = ncmdfield(p, n);

	/* allocate Cmdbuf plus string pointers plus copy of string including \0 */
	sp = smalloc(sizeof(*cb) + nf * sizeof(char*) + n + 1);
	cb = (Cmdbuf*)sp;
	cb->f = (char**)(&cb[1]);
	cb->buf = (char*)(&cb->f[nf]);

	if(up!=nil && waserror()){
		jehanne_free(cb);
		nexterror();
	}
	jehanne_memmove(cb->buf, p, n);
	if(up != nil)
		poperror();

	/* dump new line and null terminate */
	if(n > 0 && cb->buf[n-1] == '\n')
		n--;
	cb->buf[n] = '\0';

	cb->nf = jehanne_tokenize(cb->buf, cb->f, nf-1);
	cb->f[cb->nf] = nil;

	return cb;
}

/*
 * Reconstruct original message, for error diagnostic
 */
void
cmderror(Cmdbuf *cb, char *s)
{
	int i;
	char *p, *e;

	p = up->genbuf;
	e = p+ERRMAX-10;
	p = jehanne_seprint(p, e, "%s \"", s);
	for(i=0; i<cb->nf; i++){
		if(i > 0)
			p = jehanne_seprint(p, e, " ");
		p = jehanne_seprint(p, e, "%q", cb->f[i]);
	}
	jehanne_strcpy(p, "\"");
	error(up->genbuf);
}

/*
 * Look up entry in table
 */
Cmdtab*
lookupcmd(Cmdbuf *cb, Cmdtab *ctab, int nctab)
{
	int i;
	Cmdtab *ct;

	if(cb->nf == 0)
		error("empty control message");

	for(ct = ctab, i=0; i<nctab; i++, ct++){
		if(jehanne_strcmp(ct->cmd, "*") !=0)	/* wildcard always matches */
		if(jehanne_strcmp(ct->cmd, cb->f[0]) != 0)
			continue;
		if(ct->narg != 0 && ct->narg != cb->nf)
			cmderror(cb, Ecmdargs);
		return ct;
	}

	cmderror(cb, "unknown control message");
	return nil;
}
