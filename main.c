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
#include <stdio.h>
#include <jansson.h>

#define COPYRIGHT_LINE \
    "Bug reports, feature requests to gemini|https://harkadev.com/oss" "\n" \
    "Copyright (c) 2022 Harkaitz Agirre, harkaitz.aguirre@gmail.com" "\n" \
    ""

static const char help[] =
    "Usage: %s ..."                                                          "\n"
    ""                                                                       "\n"
    "Environment variables:"                                                 "\n"
    ""                                                                       "\n"
    "    PAYCOMET_API_TOKEN"                                                 "\n"
    "    PAYCOMET_TERMINAL"                                                  "\n"
    ""                                                                       "\n"
    "Create payment forms using PAYCOMET."                                   "\n"
    ""                                                                       "\n"
    "    methods-get                : Get allowed methods."                  "\n"
    "    exchange MONETARY CURRENCY : Exchange currency."                    "\n"
    "    form-auth OPTS...          : Create a payment form and get URL."    "\n"
    "    heartbeat                  : Check the connection is right."        "\n"
    "    payment-info ORDER-ID      : Get payment info of form."             "\n"
    "    payment-status ORDER-ID    : Get status: correct,failed,unfinished" "\n"
    ""                                                                       "\n"
    "Form options:"                                                          "\n"
    ""                                                                       "\n"
    "    order=ORDER-ID             : An identifier to check it later."      "\n"
    "    amount=MONETARY            : The amount to charge."                 "\n"
    "    language=LANG              : Language to use."                      "\n"
    "    description=DESC           : Description to write on."              "\n"
    "    merchantDescription=DESC   : Merchant's description."               "\n"
    "    url_success=URL_OK         : URL when success."                     "\n"
    "    url_cancel=URL_KO          : URL when cancelling."                  "\n"
    ""                                                                       "\n"
    COPYRIGHT_LINE
    ;

int main (int _argc, char *_argv[]) {
    int     e     = 0;
    int     ret   = 1;
    mpay   *mpay  = NULL;
    json_t *j1    = NULL;
    char   *pname = basename(strdupa(_argv[0]));
    /* Print help. */
    if (_argc == 1 ||
        !strcmp(_argv[1], "-h") ||
        !strcmp(_argv[1], "--help")) {
        printf(help, pname);
        return 0;
    }
    /* Initialize logging. */
    openlog(pname, LOG_PERROR, LOG_USER);
    /* Initiaze paycomet. */
    e = mpay_create(&mpay);
    if (!e/*err*/) goto cleanup;
    mpay_set_auth(mpay,
                  getenv("PAYCOMET_API_TOKEN"),
                  getenv("PAYCOMET_TERMINAL"));
    /* Get command and arguments. */
    char  *cmd  = (_argc>1)?_argv[1]:NULL;
    char **args = _argv+1;
    char  *arg1 = (_argc>2)?_argv[2]:NULL;
    char  *arg2 = (_argc>3)?_argv[3]:NULL;
    int    cmdi = str2num(cmd, strcasecmp,
                          "methods-get"   , 1, "exchange"      , 2,
                          "form-auth"     , 3, "heartbeat"     , 4,
                          "payment-info"  , 5, "payment-status", 6,
                          NULL);
    /* Operations. */
    switch (cmdi) {
    case 1: /* methods-get */
        e = mpay_methods_get(mpay, &j1);
        if (!e/*err*/) goto cleanup;
        json_dumpf(j1, stdout, JSON_INDENT(4));
        break;
    case 2: /* exchange */ {
        coin_t  c1,c2;
        if (!arg1 || !arg2/*err*/) goto cleanup_invalid_args;
        e = coin_parse(&c1, arg1, NULL);
        if (!e/*err*/) goto cleanup_invalid_args;
        e = mpay_exchange(mpay, c1, &c2, arg2);
        if (!e/*err*/) goto cleanup;
        printf("%s\n", coin_str(c2, COIN_SS_STORE));
        break; }
    case 3: /* form-auth */ {
        char            *opts[100];
        struct mpay_form form = {0};
        char            *m1   = NULL;
        streq2map(args, 100, opts);
        e = mpay_form_prepare(&form, MPAY_FORM_AUTHORIZATION, opts);
        if (!e/*err*/) goto cleanup;
        e = mpay_form(mpay, &form, &m1);
        if (!e/*err*/) goto cleanup;
        printf("%s\n", m1);
        free(m1);
        break; }
    case 4: /* heartbeat */
        e = mpay_heartbeat(mpay, stdout);
        if (!e/*err*/) goto cleanup;
        break;
    case 5: /* payment-info */
        if (!arg1/*err*/) goto cleanup_invalid_args;
        e = mpay_payment_info(mpay, arg1, NULL, &j1, NULL);
        if (!e/*err*/) goto cleanup;
        json_dumpf(j1, stdout, JSON_INDENT(4));
        break;
    case 6: /* payment-status */ {
        enum mpay_payment_state state;
        if (!arg1/*err*/) goto cleanup_invalid_args;
        e = mpay_payment_info(mpay, arg1, &state, NULL, NULL);
        if (!e/*err*/) goto cleanup;
        switch(state) {
        case MPAY_PAYMENT_FAILED:     fputs("failed\n", stdout);     break;
        case MPAY_PAYMENT_CORRECT:    fputs("correct\n", stdout);    break;
        case MPAY_PAYMENT_UNFINISHED: fputs("unfinished\n", stdout); break;
        }
        break; }
    default:
        syslog(LOG_ERR, "Invalid subcommand: %s", cmd);
        goto cleanup;
    }
    
    /* Return value. */
    ret = 0;
    goto cleanup;

    /* Cleanup. */
 cleanup_invalid_args:
    syslog(LOG_ERR, "Invalid arguments.");
    goto cleanup;
 cleanup:
    if (mpay) mpay_destroy(mpay);
    if (j1)   json_decref(j1);
    return ret;
}
/**l*
 * 
 * MIT License
 * 
 * Bug reports, feature requests to gemini|https://harkadev.com/oss
 * Copyright (c) 2022 Harkaitz Agirre, harkaitz.aguirre@gmail.com
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **l*/
