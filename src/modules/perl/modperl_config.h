#ifndef MODPERL_CONFIG_H
#define MODPERL_CONFIG_H

void *modperl_config_dir_create(apr_pool_t *p, char *dir);

void *modperl_config_dir_merge(apr_pool_t *p, void *basev, void *addv);

modperl_config_srv_t *modperl_config_srv_new(apr_pool_t *p);

modperl_config_dir_t *modperl_config_dir_new(apr_pool_t *p);

modperl_config_req_t *modperl_config_req_new(request_rec *r);

void *modperl_config_srv_create(apr_pool_t *p, server_rec *s);

void *modperl_config_srv_merge(apr_pool_t *p, void *basev, void *addv);

char **modperl_config_srv_argv_init(modperl_config_srv_t *scfg, int *argc);

#define modperl_config_srv_argv_push(arg) \
    *(const char **)apr_array_push(scfg->argv) = arg

apr_status_t modperl_config_request_cleanup(pTHX_ request_rec *r);

apr_status_t modperl_config_req_cleanup(void *data);

#define modperl_config_req_cleanup_register(r, rcfg) \
    if (r && !MpReqCLEANUP_REGISTERED(rcfg)) { \
        apr_pool_cleanup_register(r->pool, \
                                  (void*)r, \
                                   modperl_config_req_cleanup, \
                                   apr_pool_cleanup_null); \
        MpReqCLEANUP_REGISTERED_On(rcfg); \
    }

void *modperl_get_perl_module_config(ap_conf_vector_t *cv);
void modperl_set_perl_module_config(ap_conf_vector_t *cv, void *cfg);

#if defined(MP_IN_XS) && defined(WIN32)
#   define modperl_get_module_config(v) \
       modperl_get_perl_module_config(v)

#   define modperl_set_module_config(v, c) \
       modperl_set_perl_module_config(v, c)
#else
#   define modperl_get_module_config(v) \
       ap_get_module_config(v, &perl_module)

#   define modperl_set_module_config(v, c) \
       ap_set_module_config(v, &perl_module, c)
#endif

#define modperl_config_req_init(r, rcfg) \
    if (!rcfg) { \
        rcfg = modperl_config_req_new(r); \
        modperl_set_module_config(r->request_config, rcfg); \
    }

#define modperl_config_req_get(r) \
    (r ? (modperl_config_req_t *) \
          modperl_get_module_config(r->request_config) : NULL)

#define MP_dRCFG \
    modperl_config_req_t *rcfg = modperl_config_req_get(r)

#define modperl_config_dir_get(r) \
    (r ? (modperl_config_dir_t *) \
          modperl_get_module_config(r->per_dir_config) : NULL)

#define modperl_config_dir_get_defaults(s) \
    (modperl_config_dir_t *) \
        modperl_get_module_config(s->lookup_defaults)

#define MP_dDCFG \
    modperl_config_dir_t *dcfg = modperl_config_dir_get(r)

#define modperl_config_srv_get(s) \
    (modperl_config_srv_t *) \
        modperl_get_module_config(s->module_config)

#define MP_dSCFG(s) \
   modperl_config_srv_t *scfg = modperl_config_srv_get(s)

#ifdef USE_ITHREADS
#   define MP_dSCFG_dTHX \
    dTHXa(scfg->mip->parent->perl); \
    PERL_SET_CONTEXT(aTHX)
#else
#   define MP_dSCFG_dTHX dTHXa(scfg->perl)
#endif

/* hopefully this macro will not need to be used often */
#ifdef USE_ITHREADS
#   define MP_dTHX \
    modperl_interp_t *interp = \
       modperl_interp_select(r, r->connection, r->server); \
    dTHXa(interp->perl)
#else
#   define MP_dTHX dNOOP
#endif

int modperl_config_apply_PerlModule(server_rec *s,
                                    modperl_config_srv_t *scfg,
                                    PerlInterpreter *perl, apr_pool_t *p);

int modperl_config_apply_PerlRequire(server_rec *s,
                                     modperl_config_srv_t *scfg,
                                     PerlInterpreter *perl, apr_pool_t *p);

#endif /* MODPERL_CONFIG_H */
