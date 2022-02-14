#include "mpaycomet.h"
#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>
#include <kcgi.h>
#include <libgen.h>
#include <string.h>
#include <syslog.h>
#include <str/strdupa.h>
#include <str/str2num.h>
#include <str/strarray.h>
#include <io/mconfig.h>
#include <stdio.h>
#include <jansson.h>

#define PROGNAME_CGI "mpaycomet.cgi"
#define PROGNAME_BIN "mpaycomet"



/* --------------------------------------------------------------------------
 * ---- COMMON INITIALIZATION -----------------------------------------------
 * -------------------------------------------------------------------------- */
int main (int _argc, char *_argv[]) {
    char     *pname   = basename(strdupa(_argv[0]));
    mconfig_t mconfig = MCONFIG_INITIALIZER();
    if (!mconfig_load(&mconfig, "/etc/mpaycomet.cfg", true)) return 1;
    if (!mconfig_load(&mconfig, "/etc/mconfig.cfg", true)) return 1;
    if (!strcmp(pname, PROGNAME_CGI)) {
        openlog(pname, 0, LOG_USER);
        int main_cgi ();
        return main_cgi(_argc, _argv, pname, &mconfig);
    } else {
        int main_bin ();
        openlog(pname, LOG_PERROR, LOG_USER);
        return main_bin(_argc, _argv, pname, &mconfig);
    }
}


/* --------------------------------------------------------------------------
 * ---- CLI -----------------------------------------------------------------
 * -------------------------------------------------------------------------- */
static const char help[] =
    "Usage: %s ...\n";
    
int main_bin (int _argc, char *_argv[], char *pname, mconfig_t *_mconfig) {

    mpay   *mpay = NULL;
    int     res  = 0;
    int     ret  = 1;
    int     opt;
    json_t *j1   = NULL;
    char   *m1   = NULL;
    coin_t  c1,c2;
    struct mpay_form form = {0};
    
    if (_argc == 1 || !strcmp(_argv[1], "-h") || !strcmp(_argv[1], "--help")) {
        printf(help, pname);
        return 0;
    }

    res = mpay_create(&mpay, _mconfig->v);
    if (!res/*err*/) goto cleanup;

    opt = 1;
    char  *cmd  = (opt<_argc)?_argv[opt++]:NULL;
    char **args = _argv+opt;
    char  *arg1 = (opt<_argc)?_argv[opt++]:NULL;
    char  *arg2 = (opt<_argc)?_argv[opt++]:NULL;
    char  *opts[100] = {0};
    
    
    
    switch (str2num(cmd, strcasecmp,
                    "methods-get"   , 1,
                    "exchange"      , 2,
                    "form-auth"     , 3,
                    "heartbeat"     , 4,
                    "payment-info"  , 5,
                    "payment-status", 6,
                    NULL)) {
    case 1:
        res = mpay_methods_get(mpay, &j1);
        if (!res/*err*/) goto cleanup;
        json_dumpf(j1, stdout, JSON_INDENT(4));
        break;
    case 2:
        res = arg1 && arg2;
        if (!res/*err*/) goto cleanup_invalid_args;
        res = coin_parse(&c1, arg1, NULL);
        if (!res/*err*/) goto cleanup_invalid_args;
        res = mpay_exchange(mpay, c1, &c2, arg2);
        if (!res/*err*/) goto cleanup;
        printf("%s\n", coin_str(c2, COIN_SS_STORE));
        break;
    case 3: {
        streq2map(args, 100, opts);
        res = mpay_form_prepare(&form, MPAY_FORM_AUTHORIZATION, opts);
        if (!res/*err*/) goto cleanup;
        res = mpay_form(mpay, &form, &m1);
        if (!res/*err*/) goto cleanup;
        printf("%s\n", m1);
        break; }
    case 4:
        res = mpay_heartbeat(mpay, stdout);
        if (!res/*err*/) goto cleanup;
        break;
    case 5:
        if (!arg1/*err*/) goto cleanup_invalid_args;
        res = mpay_payment_info(mpay, arg1, NULL, &j1, NULL);
        if (!res/*err*/) goto cleanup;
        json_dumpf(j1, stdout, JSON_INDENT(4));
        break;
    case 6: {
        enum mpay_payment_state state;
        if (!arg1/*err*/) goto cleanup_invalid_args;
        res = mpay_payment_info(mpay, arg1, &state, NULL, NULL);
        if (!res/*err*/) goto cleanup;
        switch(state) {
        case MPAY_PAYMENT_FAILED:     fputs("failed\n", stdout);     break;
        case MPAY_PAYMENT_CORRECT:    fputs("correct\n", stdout);    break;
        case MPAY_PAYMENT_UNFINISHED: fputs("unfinished\n", stdout); break;
        }
        break; }
    default:
        goto cleanup_invalid_args;
    }

    ret = 0;
    goto cleanup;
 cleanup_invalid_args:
    syslog(LOG_ERR, "Invalid arguments.");
    goto cleanup;
 cleanup:
    if (mpay) mpay_destroy(mpay);
    if (j1)   json_decref(j1);
    return ret;
}


/* --------------------------------------------------------------------------
 * ---- CGI -----------------------------------------------------------------
 * -------------------------------------------------------------------------- */
int main_cgi (int _argc, char *_argv[], char *pname, mconfig_t *_mconfig) {
    return 1;
}




