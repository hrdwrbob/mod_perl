/* ====================================================================
 * Copyright (c) 1995-1997 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 5. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/* $Id: mod_perl.c,v 1.63 1997/09/16 00:47:48 dougm Exp $ */

/* 
 * And so it was decided the camel should be given magical multi-colored
 * feathers so it could fly and journey to once unknown worlds.
 * And so it was done...
 */

#define CORE_PRIVATE 
#include "mod_perl.h"

#ifdef MULTITHREAD
void *mod_perl_mutex = &mod_perl_mutex;
#else
void *mod_perl_dummy_mutex = &mod_perl_dummy_mutex;
#endif

static IV mp_request_rec;
static int seqno = 0;
static int perl_is_running = 0;
static PerlInterpreter *perl = NULL;
static AV *orig_inc = Nullav;
static AV *cleanup_av = Nullav;
#ifdef PERL_STACKED_HANDLERS
static HV *stacked_handlers = Nullhv;
#endif

static command_rec perl_cmds[] = {
#ifdef PERL_SECTIONS
    { "<Perl>", perl_section, NULL, OR_ALL, RAW_ARGS, "Perl code" },
    { "</Perl>", perl_end_section, NULL, OR_ALL, NO_ARGS, NULL },
#endif
    { "PerlTaintCheck", perl_cmd_tainting,
      NULL,
      RSRC_CONF, FLAG, "Turn on -T switch" },
    { "PerlWarn", perl_cmd_warn,
      NULL,
      RSRC_CONF, FLAG, "Turn on -w switch" },
    { "PerlScript", perl_cmd_script,
      NULL,
      RSRC_CONF, TAKE1, "A Perl script name" },
    { "PerlModule", perl_cmd_module,
      NULL,
      OR_ALL, ITERATE, "List of Perl modules" },
    { "PerlSetVar", perl_cmd_var,
      NULL,
      OR_ALL, TAKE2, "Perl config var and value" },
    { "PerlSetEnv", perl_cmd_setenv,
      NULL,
      OR_ALL, TAKE2, "Perl %ENV key and value" },
    { "PerlSendHeader", perl_cmd_sendheader,
      NULL,
      OR_ALL, FLAG, "Tell mod_perl to parse and send HTTP headers" },
    { "PerlSetupEnv", perl_cmd_env,
      NULL,
      OR_ALL, FLAG, "Tell mod_perl to setup %ENV by default" },
    { "PerlHandler", perl_cmd_handler_handlers,
      NULL,
      OR_ALL, ITERATE, "the Perl handler routine name" },
#ifdef PERL_TRANS
    { PERL_TRANS_CMD_ENTRY },
#endif
#ifdef PERL_AUTHEN
    { PERL_AUTHEN_CMD_ENTRY },
#endif
#ifdef PERL_AUTHZ
    { PERL_AUTHZ_CMD_ENTRY },
#endif
#ifdef PERL_ACCESS
    { PERL_ACCESS_CMD_ENTRY },
#endif
#ifdef PERL_TYPE
    { PERL_TYPE_CMD_ENTRY },
#endif
#ifdef PERL_FIXUP
    { PERL_FIXUP_CMD_ENTRY },
#endif
#ifdef PERL_LOG
    { PERL_LOG_CMD_ENTRY },
#endif
#ifdef PERL_CLEANUP
    { PERL_CLEANUP_CMD_ENTRY },
#endif
#ifdef PERL_INIT
    { PERL_INIT_CMD_ENTRY },
#endif
#ifdef PERL_HEADER_PARSER
    { PERL_HEADER_PARSER_CMD_ENTRY },
#endif
#ifdef PERL_CHILD_INIT
    { PERL_CHILD_INIT_CMD_ENTRY },
#endif
#ifdef PERL_CHILD_EXIT
    { PERL_CHILD_EXIT_CMD_ENTRY },
#endif
#ifdef PERL_POST_READ_REQUEST
    { PERL_POST_READ_REQUEST_CMD_ENTRY },
#endif
    { NULL }
};

static handler_rec perl_handlers [] = {
    { PERL_APACHE_SSI_TYPE, perl_handler },
    { "perl-script", perl_handler },
    { NULL }
};

module MODULE_VAR_EXPORT perl_module = {
    STANDARD_MODULE_STUFF,
    perl_startup,                 /* initializer */
    perl_create_dir_config,    /* create per-directory config structure */
    perl_merge_dir_config,     /* merge per-directory config structures */
    perl_create_server_config, /* create per-server config structure */
    NULL,                      /* merge per-server config structures */
    perl_cmds,                 /* command table */
    perl_handlers,             /* handlers */
    PERL_TRANS_HOOK,           /* translate_handler */
    PERL_AUTHEN_HOOK,          /* check_user_id */
    PERL_AUTHZ_HOOK,           /* check auth */
    PERL_ACCESS_HOOK,          /* check access */
    PERL_TYPE_HOOK,            /* type_checker */
    PERL_FIXUP_HOOK,           /* pre-run fixups */
    PERL_LOG_HOOK,          /* logger */
#if MODULE_MAGIC_NUMBER >= 19970103
    PERL_HEADER_PARSER_HOOK,   /* header parser */
#endif
#if MODULE_MAGIC_NUMBER >= 19970719
    PERL_CHILD_INIT_HOOK,   /* child_init */
#endif
#if MODULE_MAGIC_NUMBER >= 19970728
    PERL_CHILD_EXIT_HOOK,   /* child_exit */
#endif
#if MODULE_MAGIC_NUMBER >= 19970825
    PERL_POST_READ_REQUEST_HOOK,   /* post_read_request */
#endif
};

#if defined(STRONGHOLD) && !defined(APACHE_SSL)
#define APACHE_SSL
#endif

int PERL_RUNNING (void) 
{
    return (perl_is_running);
}

void perl_shutdown (server_rec *s, pool *p)
{
    /* execute END blocks we suspended during perl_startup() */
    perl_run_endav("perl_shutdown"); 

#ifdef HAVE_PERL_5__4
    perl_destruct_level = 2;

    MP_TRACE(fprintf(stderr, 
		     "destructing and freeing perl interpreter..."));

    perl_util_cleanup();

    mp_request_rec = 0;

    av_undef(orig_inc);
    SvREFCNT_dec((SV*)orig_inc);
    orig_inc = Nullav;

    av_undef(cleanup_av);
    SvREFCNT_dec((SV*)cleanup_av);
    cleanup_av = Nullav;

#ifdef PERL_STACKED_HANDLERS
    hv_undef(stacked_handlers);
    SvREFCNT_dec((SV*)stacked_handlers);
    stacked_handlers = Nullhv;
#endif
    
    perl_destruct(perl);
    perl_free(perl);
    perl_is_running = 0;
    MP_TRACE(fprintf(stderr, "ok\n"));

#else

#endif
}

void perl_startup (server_rec *s, pool *p)
{
    char *argv[] = { NULL, NULL, NULL, NULL, NULL };
    char *constants[] = { "Apache::Constants", "OK", "DECLINED", NULL };
    int status, i, argc=2, t=0, w=0;
    dPSRV(s);

#ifndef WIN32
    argv[0] = server_argv0;
#endif

    if(perl_is_running++) {
#if 0
      perl_shutdown(s, p);
#else
      MP_TRACE(fprintf(stderr, "perl_startup: perl aleady running...ok\n"));
      return;
#endif
    }

    MP_TRACE(fprintf(stderr, "allocating perl interpreter..."));
    if((perl = perl_alloc()) == NULL) {
	MP_TRACE(fprintf(stderr, "not ok\n"));
	perror("alloc");
	exit(1);
    }
    MP_TRACE(fprintf(stderr, "ok\n"));
  
    MP_TRACE(fprintf(stderr, "constructing perl interpreter...ok\n"));
    perl_construct(perl);

    /* fake-up what the shell usually gives perl */
    if((t = cls->PerlTaintCheck)) {
	argv[1] = "-T";
	argc++;
    }
    if((w = cls->PerlWarn)) {
	argv[1+t] = "-w";
	argc++;
    }

    argv[1+t+w] = cls->PerlScript ? 
	server_root_relative(p, cls->PerlScript) : NULL;

    if (argv[1+t+w] == NULL) {
	argv[1+t+w] = "-e";
	argv[2+t+w] = "0";
	argc++;
    } 
    MP_TRACE(fprintf(stderr, "parsing perl script: "));
    for(i=1; i<argc; i++)
	MP_TRACE(fprintf(stderr, "'%s' ", argv[i]));
    MP_TRACE(fprintf(stderr, "..."));

    status = perl_parse(perl, xs_init, argc, argv, NULL);
    if (status != OK) {
	MP_TRACE(fprintf(stderr,"not ok, status=%d\n", status));
	perror("parse");
	exit(1);
    }
    MP_TRACE(fprintf(stderr, "ok\n"));

    perl_clear_env();
    hv_store(PerlEnvHV, "GATEWAY_INTERFACE", 17, 
	     newSVpv(PERL_GATEWAY_INTERFACE,0), 0);

    if(hv_exists(GvHV(incgv), "CGI.pm", 6)) {
	/* be sure CGI.pm knows GATEWAY_INTERFACE by recompiling  
	 * we'll only get here if there's a `use CGI' in the PerlScript file 
	 */
	bool old_warn = dowarn;
	(void)hv_delete(GvHV(incgv), "CGI.pm", 6, G_DISCARD);
	MP_TRACE(fprintf(stderr, 
		 "Reloading CGI.pm so it sees GATEWAY_INTERFACE...\n"));
	dowarn = FALSE; perl_require_module("CGI", s); dowarn = old_warn;
    }

    MP_TRACE(fprintf(stderr, "running perl interpreter..."));

    ENTER;
    /* suspend END blocks */
    save_aptr(&endav);
    endav = Nullav;

    status = perl_run(perl);

    MP_TRACE(fprintf(stderr, 
	     "mod_perl: %d END blocks encountered during server startup\n",
	     AvFILL(endav)+1));
#if MODULE_MAGIC_NUMBER < 19970728
    if(endav)
	fprintf(stderr, "mod_perl: cannot run END blocks encoutered at server startup without apache_1.3b1+\n");
#endif

    LEAVE;

    if (status != OK) {
	MP_TRACE(fprintf(stderr,"not ok, status=%d\n", status));
	perror("run");
	exit(1);
    }
    MP_TRACE(fprintf(stderr, "ok\n"));

    for(i = 0; i < cls->NumPerlModules; i++) {
	if(perl_require_module(cls->PerlModules[i], s) != OK) {
	    fprintf(stderr, "Can't load Perl module `%s', exiting...\n", 
		    cls->PerlModules[i]);
	    exit(1);
	}
    }

    /* import Apache::Constants qw(OK DECLINED) */
    perl_call_argv("Exporter::import", G_DISCARD | G_EVAL, constants);

    if(perl_eval_ok(s) != OK) 
	perror("Apache::Constants->import failed");

    orig_inc = av_copy_array(GvAV(incgv));

    {
	GV *gv = gv_fetchpv("Apache::__T", GV_ADDMULTI, SVt_PV);
	if(cls->PerlTaintCheck) 
	    sv_setiv(GvSV(gv), 1);
	SvREADONLY_on(GvSV(gv));
    }
    {
	GV *sig = gv_fetchpv("SIG", FALSE, SVt_PVHV);
	hv_store(GvHV(sig), "PIPE", 4, newSVpv("IGNORE",6), FALSE);
    }
#ifdef PERL_STACKED_HANDLERS
    if(!stacked_handlers)
	stacked_handlers = newHV();
#endif 
#ifdef MULTITHREAD
    mod_perl_mutex = create_mutex(NULL);
#endif
}

int mod_perl_sent_header(request_rec *r, int val)
{
    dPPDIR;

    if(val) MP_SENTHDR_on(cld);
    val = MP_SENTHDR(cld) ? 1 : 0;
    return MP_SENDHDR(cld) ? val : 1;
}

#ifndef perl_init_ids
#define perl_init_ids mod_perl_init_ids()
#endif

int perl_handler(request_rec *r)
{
    dSTATUS;
    dPPDIR;

    (void)perl_request_rec(r); 
    register_cleanup(r->pool, NULL, mod_perl_end_cleanup, NULL);

#ifdef USE_SFIO
    IoFLAGS(GvIOp(defoutgv)) |= IOf_FLUSH; /* $|=1 */
#else
    IoFLAGS(GvIOp(defoutgv)) &= ~IOf_FLUSH; /* $|=0 */
#endif

    MP_TRACE(fprintf(stderr, "perl_handler ENTER: SVs = %5d, OBJs = %5d\n",
		     (int)sv_count, (int)sv_objcount));
    ENTER;
    SAVETMPS;

    save_aptr(&endav); 
    endav = Nullav;

    /* hookup STDIN & STDOUT to the client */
    perl_stdout2client(r);
    perl_stdin2client(r);

    seqno++;

    if(MP_ENV(cld)) 
	perl_setup_env(r);

    PERL_CALLBACK("PerlHandler", cld->PerlHandler);

    perl_run_rgy_endav(r->uri);

    FREETMPS;
    LEAVE;
    MP_TRACE(fprintf(stderr, "perl_handler LEAVE: SVs = %5d, OBJs = %5d\n", 
		     (int)sv_count, (int)sv_objcount));
    return status;
}


#ifdef PERL_CHILD_INIT
void PERL_CHILD_INIT_HOOK(server_rec *s, pool *p)
{
    request_rec *r = (request_rec *)palloc(p, sizeof(request_rec));
    dSTATUS;
    dPSRV(s);

    r->pool = p; 
    r->server = s;

    mod_perl_init_ids();

    PERL_CALLBACK("PerlChildInitHandler", cls->PerlChildInitHandler);
}
#endif

#ifdef PERL_CHILD_EXIT
void PERL_CHILD_EXIT_HOOK(server_rec *s, pool *p)
{
    request_rec *r = (request_rec *)palloc(p, sizeof(request_rec));
    dSTATUS;
    dPSRV(s);

    r->pool = p; 
    r->server = s;

    PERL_CALLBACK("PerlChildExitHandler", cls->PerlChildExitHandler);

    perl_shutdown(s,p);
}
#endif

#ifdef PERL_POST_READ_REQUEST
int PERL_POST_READ_REQUEST_HOOK(request_rec *r)
{
    dSTATUS;
    dPSRV(r->server);
    PERL_CALLBACK("PerlPostReadRequestHandler", cls->PerlPostReadRequestHandler);
    return status;
}
#endif

#ifdef PERL_TRANS
int PERL_TRANS_HOOK(request_rec *r)
{
    dSTATUS;
    dPSRV(r->server);
    PERL_CALLBACK("PerlTransHandler", cls->PerlTransHandler);
    return status;
}
#endif

#ifdef PERL_HEADER_PARSER
int PERL_HEADER_PARSER_HOOK(request_rec *r)
{
    dSTATUS;
    dPPDIR;
#ifdef PERL_INIT
    PERL_CALLBACK("PerlInitHandler", 
			 cld->PerlInitHandler);
#endif
    PERL_CALLBACK("PerlHeaderParserHandler", 
			 cld->PerlHeaderParserHandler);
    return status;
}
#endif

#ifdef PERL_AUTHEN
int PERL_AUTHEN_HOOK(request_rec *r)
{
    dSTATUS;
    dPPDIR;
    PERL_CALLBACK("PerlAuthenHandler", cld->PerlAuthenHandler);
    return status;
}
#endif

#ifdef PERL_AUTHZ
int PERL_AUTHZ_HOOK(request_rec *r)
{
    dSTATUS;
    dPPDIR;
    PERL_CALLBACK("PerlAuthzHandler", cld->PerlAuthzHandler);
    return status;
}
#endif

#ifdef PERL_ACCESS
int PERL_ACCESS_HOOK(request_rec *r)
{
    dSTATUS;
    dPPDIR;
    PERL_CALLBACK("PerlAccessHandler", cld->PerlAccessHandler);
    return status;
}
#endif

#ifdef PERL_TYPE
int PERL_TYPE_HOOK(request_rec *r)
{
    dSTATUS;
    dPPDIR;
    PERL_CALLBACK("PerlTypeHandler", cld->PerlTypeHandler);
    return status;
}
#endif

#ifdef PERL_FIXUP
int PERL_FIXUP_HOOK(request_rec *r)
{
    dSTATUS;
    dPPDIR;
    PERL_CALLBACK("PerlFixupHandler", cld->PerlFixupHandler);
    return status;
}
#endif

#ifdef PERL_LOG
int PERL_LOG_HOOK(request_rec *r)
{
    dSTATUS;
    dPPDIR;
    int rstatus;
    PERL_CALLBACK("PerlLogHandler", cld->PerlLogHandler);
    rstatus = status;
#ifdef PERL_CLEANUP
    PERL_CALLBACK("PerlCleanupHandler", cld->PerlCleanupHandler);
#endif
    return rstatus;
}
#endif

void mod_perl_end_cleanup(void *data)
{
    (void)acquire_mutex(mod_perl_mutex); 
    perl_clear_env();
    av_undef(GvAV(incgv));
    SvREFCNT_dec(GvAV(incgv));
    GvAV(incgv) = Nullav;
    GvAV(incgv) = av_copy_array(orig_inc);
    /* reset $/ */
    sv_setpvn(GvSV(gv_fetchpv("/", FALSE, SVt_PV)), "\n", 1);
    MP_TRACE(fprintf(stderr, "perl_end_cleanup...ok\n"));
    (void)release_mutex(mod_perl_mutex); 
}

void mod_perl_cleanup_handler(void *data)
{
    request_rec *r = perl_request_rec(NULL);
    SV *cv;
    I32 i;

    (void)acquire_mutex(mod_perl_mutex); 
    MP_TRACE(fprintf(stderr, "running registered cleanup handlers...\n")); 
    for(i=0; i<=AvFILL(cleanup_av); i++) { 
	cv = *av_fetch(cleanup_av, i, 0);
	perl_call_handler(cv, (request_rec *)r, Nullav);
    }
    av_clear(cleanup_av);
    (void)release_mutex(mod_perl_mutex); 
}

#ifdef PERL_METHOD_HANDLERS
int perl_handler_ismethod(HV *class, char *sub)
{
    CV *cv;
    HV *stash;
    GV *gv;
    SV *sv;
    int is_method=0;

    if(!sub) return 0;
    sv = newSVpv(sub,0);
    if(!(cv = sv_2cv(sv, &stash, &gv, FALSE)))
	cv = GvCV(gv_fetchmethod(class, sub));

    if (cv && SvPOK(cv)) 
	is_method = strnEQ(SvPVX(cv), "$$", 2);
    MP_TRACE(fprintf(stderr, "checking if `%s' is a method...%s\n", 
	   sub, (is_method ? "yes" : "no")));
    SvREFCNT_dec(sv);
    return is_method;
}
#endif

void mod_perl_register_cleanup(request_rec *r, SV *sv)
{
    dPPDIR;

    if(!MP_RCLEANUP(cld)) {
	(void)perl_request_rec(r); 
	register_cleanup(r->pool, (void*)r,
			 mod_perl_cleanup_handler, NULL);
	MP_RCLEANUP_on(cld);
	if(cleanup_av == Nullav) cleanup_av = newAV();
    }
    MP_TRACE(fprintf(stderr, "registering PerlCleanupHandler\n"));
    
    ++SvREFCNT(sv); av_push(cleanup_av, sv);
}

#ifdef PERL_STACKED_HANDLERS

int mod_perl_push_handlers(SV *self, char *hook, SV *sub, AV *handlers)
{
    int do_store=0, len=strlen(hook);
    SV **svp;

    if(self && SvTRUE(sub)) {
	if(handlers == Nullav) {
	    svp = hv_fetch(stacked_handlers, hook, len, 0);
	    MP_TRACE(fprintf(stderr, "fetching %s stack\n", hook));
	    if(svp && SvTRUE(*svp) && SvROK(*svp)) {
		handlers = (AV*)SvRV(*svp);
	    }
	    else {
		MP_TRACE(fprintf(stderr, "%s handlers stack undef, creating\n", hook));
		handlers = newAV();
	    }
	    do_store = 1;
	}
	    
	if(SvROK(sub) && (SvTYPE(SvRV(sub)) == SVt_PVCV)) {
	    MP_TRACE(fprintf(stderr, "pushing CODE ref into `%s' handlers\n", hook));
	}
	else if(SvPOK(sub)) {
	    MP_TRACE(fprintf(stderr, "pushing `%s' into `%s' handlers\n", 
		   SvPV(sub,na), hook));
	}
	else {
	    warn("mod_perl_push_handlers: Not a subroutine name or CODE reference!");
	}

	++SvREFCNT(sub); av_push(handlers, sub);

	if(do_store) 
	    hv_store(stacked_handlers, hook, len, 
		     (SV*)newRV((SV*)handlers), 0);
	return 1;
    }
    return 0;
}

int perl_run_stacked_handlers(char *hook, request_rec *r, AV *handlers)
{
    dSTATUS;
    I32 i, do_clear=FALSE;
    SV *sub, **svp; 
    int hook_len = strlen(hook);

    if(handlers == Nullav) {
	if(hv_exists(stacked_handlers, hook, hook_len)) {
	   svp = hv_fetch(stacked_handlers, hook, hook_len, 0);
	   if(svp && SvROK(*svp)) 
	       handlers = (AV*)SvRV(*svp);
	}
	else {
	    MP_TRACE(fprintf(stderr, "`%s' push_handlers() stack is empty\n", hook));
	    return DECLINED;
	}
	do_clear = TRUE;
	MP_TRACE(fprintf(stderr, 
		 "running %d pushed (stacked) handlers for %s...\n", 
			 AvFILL(handlers)+1, r->uri)); 
    }
    else {
#ifdef PERL_STACKED_HANDLERS
      /* XXX: bizarre, 
	 I only see this with httpd.conf.pl and PerlAccessHandler */
	if(SvTYPE((SV*)handlers) != SVt_PVAV) {
	    fprintf(stderr, "%s stack is not an ARRAY!\n", hook);
	    sv_dump((SV*)handlers);
	    return DECLINED;
	}
#endif
	MP_TRACE(fprintf(stderr, 
		 "running %d server configured stacked handlers for %s...\n", 
			 AvFILL(handlers)+1, r->uri)); 
    }
    for(i=0; i<=AvFILL(handlers); i++) {
	MP_TRACE(fprintf(stderr, "calling &{%s->[%d]}\n", hook, (int)i));

	if(!(sub = *av_fetch(handlers, i, FALSE))) {
	    MP_TRACE(fprintf(stderr, "sub not defined!\n"));
	}
	else {
	    if(!SvTRUE(sub)) {
		MP_TRACE(fprintf(stderr, "sub undef!  skipping callback...\n"));
		continue;
	    }
	    status = perl_call_handler(sub, r, Nullav);

	    if((status != OK) && (status != DECLINED)) {
		if(do_clear)
		    av_clear(handlers);	
		return status;
	    }
	}
    }
    if(do_clear)
	av_clear(handlers);	
    return status;
}

#endif /* PERL_STACKED_HANDLERS */

/* things to do once per-request */
void perl_per_request_init(request_rec *r)
{
    dPPDIR;

    /* hookup stderr to error_log */
#ifndef PERL_TRACE
    if(!MP_DSTDERR(cld)) {
	if(r->server->error_log) {
	    error_log2stderr(r->server);
	    MP_DSTDERR_on(cld);
	}
    }
#endif

    /* set $$, $>, etc., if 1.3a1+, this really happens during child_init */
    perl_init_ids; 

    /* PerlSetEnv */
    mod_perl_dir_env(cld);

    /* PerlSendHeader */
    if(MP_SENDHDR(cld))
	MP_SENTHDR_off(cld);
    else
	MP_SENTHDR_on(cld);

    /* SetEnv PERL5LIB */
    if(!MP_INCPUSH(cld)) {
	char *path = table_get(r->subprocess_env, "PERL5LIB");
	if(path) {
	    perl_incpush(path);
	    MP_INCPUSH_on(cld);
	}
    }
}

/* XXX this still needs work, getting there... */
API_EXPORT(int) perl_call_handler(SV *sv, request_rec *r, AV *args)
{
    int count, status, is_method=0;
    dSP;
    HV *stash = Nullhv;
    SV *class = newSVsv(sv);
    CV *cv = Nullcv;
    char *method = "handler";
    int defined_sub = 0, anon = 0;
      
    if(r->per_dir_config)
	perl_per_request_init(r);

    if(SvTYPE(sv) == SVt_PV) {
	char *imp = pstrdup(r->pool, (char *)SvPV(class,na));

	if((anon = strnEQ(imp,"sub ",4))) {
#ifdef HAVE_PERL_5__4
	    sv = perl_eval_pv(imp, FALSE);
	    MP_TRACE(fprintf(stderr, "perl_call: caching CV pointer to `__ANON__'\n"));
	    defined_sub++;
	    goto callback; /* XXX, I swear I've never used goto before! */
#else
	    warn("Need Perl version 5.004+ to use anonymous subs!\n");
	    return SERVER_ERROR;
#endif
	}


#ifdef PERL_METHOD_HANDLERS
	{
	    char *end_class = NULL;

	    if ((end_class = strstr(imp, "->"))) {
		end_class[0] = '\0';
		if(class)
		    SvREFCNT_dec(class);
		class = newSVpv(imp, 0);
		end_class[0] = ':';
		end_class[1] = ':';
		method = &end_class[2];
		imp = method;
		++is_method;
	    }
	}

	if(class) stash = gv_stashpv(SvPV(class,na),FALSE);
	   
	MP_TRACE(fprintf(stderr, "perl_call: class=`%s'\n", SvPV(class,na)));
	MP_TRACE(fprintf(stderr, "perl_call: imp=`%s'\n", imp));
	MP_TRACE(fprintf(stderr, "perl_call: method=`%s'\n", method));
	MP_TRACE(fprintf(stderr, "perl_call: stash=`%s'\n", 
			 stash ? HvNAME(stash) : "unknown"));
#else
	method = NULL; /* avoid warning */
#endif


    /* if a Perl*Handler is not a defined function name,
     * default to the class implementor's handler() function
     * attempt to load the class module if it is not already
     */
	if(!imp) imp = SvPV(sv,na);
	if(!stash) stash = gv_stashpv(imp,FALSE);
	if(!is_method)
	    defined_sub = (cv = perl_get_cv(imp, FALSE)) ? TRUE : FALSE;
#ifdef PERL_METHOD_HANDLERS
	if(!defined_sub && stash) {
	    MP_TRACE(fprintf(stderr, 
		   "perl_call: trying method lookup on `%s' in class `%s'...", 
		   method, HvNAME(stash)));
	    /* XXX Perl caches method lookups internally, 
	     * should we cache this lookup?
	     */
	    if((cv = GvCV(gv_fetchmethod(stash, method)))) {
		MP_TRACE(fprintf(stderr, "found\n"));
		is_method = perl_handler_ismethod(stash, method);
	    }
	    else {
		MP_TRACE(fprintf(stderr, "not found\n"));
	    }
	}
#endif

	if(!stash && !defined_sub) {
	    MP_TRACE(fprintf(stderr, "%s symbol table not found, loading...\n", imp));
	    if(perl_require_module(imp, r->server) == OK)
		stash = gv_stashpv(imp,FALSE);
#ifdef PERL_METHOD_HANDLERS
	    if(stash) /* check again */
		is_method = perl_handler_ismethod(stash, method);
#endif
	}
	
	if(!is_method && !defined_sub) {
	    if(!strnEQ(imp,"OK",2) && !strnEQ(imp,"DECLINED",8)) { /*XXX*/
		MP_TRACE(fprintf(stderr, 
		       "perl_call: defaulting to %s::handler\n", imp));
		sv_catpv(sv, "::handler");
	    }
	}
#ifdef PERL_STACKED_HANDLERS
 	if(!is_method && defined_sub) { /* cache it */
	    MP_TRACE(fprintf(stderr, 
			     "perl_call: caching CV pointer to `%s'\n", 
			     (anon ? "__ANON__" : SvPV(sv,na))));
	    SvREFCNT_dec(sv);
 	    sv = (SV*)newRV((SV*)cv); /* let newRV inc the refcnt */
	}
#endif
    }
    else {
	MP_TRACE(fprintf(stderr, "perl_call: handler is a cached CV\n"));
    }

callback:
    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
#ifdef PERL_METHOD_HANDLERS
    if(is_method)
	XPUSHs(sv_2mortal(class));
    else
	SvREFCNT_dec(class);
#else
    SvREFCNT_dec(class);
#endif

    XPUSHs((SV*)perl_bless_request_rec(r)); 
    {
	I32 i, len = (args ? AvFILL(args) : 0);
	
	if(args) {
	    EXTEND(sp, len);
	    for(i=0; i<=len; i++)
		PUSHs(sv_2mortal(*av_fetch(args, i, FALSE)));
	}
    }
    PUTBACK;
    
    /* use G_EVAL so we can trap errors */
#ifdef PERL_METHOD_HANDLERS
    if(is_method)
	count = perl_call_method(method, G_EVAL | G_SCALAR);
    else
#endif
	count = perl_call_sv(sv, G_EVAL | G_SCALAR);
    
    SPAGAIN;

    if (perl_eval_ok(r->server) != OK) {
        status = SERVER_ERROR;
    }
    else if (count != 1) {
	log_error("perl_call did not return a status arg, assuming OK",
		  r->server);
	status = OK;
    }
    else {
	status = POPi;
	if((status == 1) || (status == 200) || (status > 600)) 
	    status = OK; 
    }

    PUTBACK;
    FREETMPS;
    LEAVE;
    MP_TRACE(fprintf(stderr, "perl_call_handler: SVs = %5d, OBJs = %5d\n", 
	    (int)sv_count, (int)sv_objcount));

    if(SvMAGICAL(GvSV(errgv)))
       sv_unmagic(GvSV(errgv), 'U'); /* Apache::exit was called */

    return status;
}

request_rec *perl_request_rec(request_rec *r)
{
    if(r != NULL) {
	mp_request_rec = (IV)r;
	return NULL;
    }
    else
	return (request_rec *)mp_request_rec;
}

SV *perl_bless_request_rec(request_rec *r)
{
    SV *sv = sv_newmortal();
    sv_setref_pv(sv, "Apache", (void*)r);
    MP_TRACE(fprintf(stderr, "blessing request_rec=(0x%lx)\n",
		     (unsigned long)r));
    return sv;
}

void perl_setup_env(request_rec *r)
{ 
    int klen;
    array_header *env_arr = table_elts (r->subprocess_env); 
    HV *cgienv = PerlEnvHV;
    CGIENVinit; 

    if (tz != NULL) 
	hv_store(cgienv, "TZ", 2, newSVpv(tz,0), 0);
    
    for (i = 0; i < env_arr->nelts; ++i) {
	if (strnEQ("HTTP_AUTHORIZATION", elts[i].key, 18)) continue;
	if (!elts[i].key) continue;
	klen = strlen(elts[i].key);  
	hv_store(cgienv, elts[i].key, klen,
		 newSVpv(elts[i].val,0), 0);
	HV_SvTAINTED_on(cgienv, elts[i].key, klen);
    }
    MP_TRACE(fprintf(stderr, "perl_setup_env...%d keys\n", i));
}

int mod_perl_seqno(SV *self)
{
    if(SvTRUE(self)) return seqno;
    else return -1;
}

