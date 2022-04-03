#include "mpaycomet.h"
#include <string.h>
#include <str/str2num.h>
#include <str/str2ptr.h>
#include <str/strdupa.h>
#include <str/mtext.h>
#include <types/long_ss.h>
#include <curl/crest.h>
#include <jansson/extra.h>


struct mpay {
    crest      *crest;
    const char *url;
    char        auth_api_token[256];
    long        auth_terminal;
    bool        auth_ok;
};

#ifndef error
#  define error(FMT,...) syslog(LOG_ERR, "%s: " FMT, __func__, ##__VA_ARGS__)
#endif
#define set_nn(P,V) ({ if (P) *(P) = (V); })

/* ------------------------------------
 * ---- CONSTRUCTOR AND DESTRUCTOR ----
 * ------------------------------------ */

bool mpay_create(mpay **_o) {
    mpay *o = NULL; int e;
    o = calloc(1, sizeof(struct mpay));
    if (!o/*err*/) goto cleanup_errno;
    e = crest_create(&o->crest);
    if (!e/*err*/) goto cleanup;
    o->url = "https://rest.paycomet.com";
    *_o = o;
    return true;
 cleanup_errno:
    error("%s", strerror(errno));
    goto cleanup;
 cleanup:
    mpay_destroy(o);
    return false;
}

void mpay_destroy(mpay *_o) {
    if (_o) {
        if (_o->crest) crest_destroy(_o->crest);
        free(_o);
    }
}

/* ------------------------------------
 * ---- AUTHORIZATION -----------------
 * ------------------------------------ */

void mpay_set_auth(mpay *_o, const char *_api_token, const char *_terminal) {
    int e;
    _o->auth_ok = false;
    if (_api_token) {
        strncpy(_o->auth_api_token, _api_token, sizeof(_o->auth_api_token)-1);
    }
    if (_terminal) {
        e = long_parse(&_o->auth_terminal, _terminal, NULL);
        if (!e/*err*/) { _o->auth_terminal = -1; }
    }
}

bool mpay_chk_auth(mpay *_o, const char **_reason) {
    int e;
    if (_o->auth_ok == false) {
        if (_o->auth_terminal == 0) {
            set_nn(_reason, _("Missing terminal"));
            return false;
        } else if (_o->auth_terminal < 0) {
            set_nn(_reason, _("Invalid terminal"));
            return false;
        } else if (_o->auth_api_token[0] == '\0') {
            set_nn(_reason, _("Missing API token"));
            return false;
        }
        e = crest_set_auth_header
            (_o->crest, "PAYCOMET-API-TOKEN: %s", _o->auth_api_token);
        if (!e) {
            set_nn(_reason, _("Internal error"));
            return false;
        }
    }
    _o->auth_ok = true;
    return true;
}

/* ------------------------------------
 * ---- CHECK IT WORKS ----------------
 * ------------------------------------ */

bool mpay_heartbeat(mpay *_o, FILE *_fp1) {
    int          e   = 0;
    bool         r   = false;
    crest_result hr  = {0};
    FILE        *fp  = NULL;
    json_t      *j1  = NULL;
    e = mpay_chk_auth(_o, NULL);
    if (!e/*err*/) goto cleanup_unauthorized;
    e = crest_start_url(_o->crest, "%s/v1/heartbeat", _o->url);
    if (!e/*err*/) goto cleanup;
    e = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    e = fprintf(fp, "{\"terminal\": %li}", _o->auth_terminal);
    if (e<0/*err*/) goto cleanup_errno;
    e = crest_perform(_o->crest, &hr.ctype, &hr.rcode, &hr.d, &hr.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&j1, hr.ctype, hr.rcode, hr.d, hr.dsz);
    if (!e/*err*/) goto cleanup;
    const char *ping_paycomet    = json_object_get_string (j1, "time");
    const char *ping_processor   = json_object_get_string (j1, "processorTime");
    bool        status_processor = json_object_get_boolean(j1, "processorStatus");
    e = ping_paycomet && ping_processor && status_processor;
    if (!e/*err*/) goto cleanup_invalid_response;
    fprintf(_fp1, "Paycomet ping    : %s\n", ping_paycomet);
    fprintf(_fp1, "Processor ping   : %s\n", ping_processor);
    fprintf(_fp1, "Processor status : %s\n", (status_processor)?"on":"off");
    r = true;
    goto cleanup;
 cleanup_errno:
    error("%s", strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    error("Received invalid response:\n%.*s", (int)hr.dsz, hr.d);
    goto cleanup;
 cleanup_unauthorized:
    error("Not configured.");
    goto cleanup;
 cleanup:
    if (j1) json_decref(j1);
    return r;
}

/* ------------------------------------
 * ---- AUXILIARY METHODS -------------
 * ------------------------------------ */

bool mpay_methods_get(mpay *_o, json_t **_r) {
    int          e  = 0;
    crest_result hr = {0};
    bool         r  = false;
    FILE        *fp = NULL;
    json_t      *j1 = NULL;
    e = mpay_chk_auth(_o, NULL);
    if (!e/*err*/) goto cleanup_unauthorized;
    e = crest_start_url(_o->crest, "%s/v1/methods", _o->url);
    if (!e/*err*/) goto cleanup;
    e = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    e = fprintf(fp, "{\"terminal\": %li}", _o->auth_terminal);
    if (e<0/*err*/) goto cleanup_errno;
    e = crest_perform(_o->crest, &hr.ctype, &hr.rcode, &hr.d, &hr.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&j1, hr.ctype, hr.rcode, hr.d, hr.dsz);
    if (!e/*err*/) goto cleanup;
    if (_r) {
        *_r = json_incref(j1);
    }
    r = true;
    goto cleanup;
 cleanup_errno:
    error("%s", strerror(errno));
    goto cleanup;
 cleanup_unauthorized:
    error("Not configured.");
    goto cleanup;
 cleanup:
    if (j1) json_decref(j1);
    return r;
}

bool mpay_exchange(mpay *_o, coin_t _fr, coin_t *_to, const char *_currency) {
    int          e  = 0;
    bool         r  = false;
    crest_result hr = {0};
    FILE        *fp = NULL;
    json_t      *j1 = NULL,*j2;
    e = mpay_chk_auth(_o, NULL);
    if (!e/*err*/) goto cleanup_unauthorized;
    e = crest_start_url(_o->crest, "%s/v1/exchange", _o->url);
    if (!e/*err*/) goto cleanup;
    e = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    strncpy(_to->currency, _currency, sizeof(_to->currency)-1);
    for (char *c=_fr.currency; *c; c++)  *c=toupper(*c);
    for (char *c=_to->currency; *c; c++) *c=toupper(*c);
    e = fprintf(fp,
                "{"
                "    \"terminal\"        : %li, "   "\n"
                "    \"amount\"          : %li,"    "\n"
                "    \"originalCurrency\": \"%s\"," "\n"
                "    \"finalCurrency\"   : \"%s\""  "\n"
                "}"                                 "\n",
                _o->auth_terminal,
                _fr.cents,
                _fr.currency,
                _to->currency);
    if (e<0/*err*/) goto cleanup_errno;
    e = crest_perform(_o->crest, &hr.ctype, &hr.rcode, &hr.d, &hr.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&j1, hr.ctype, hr.rcode, hr.d, hr.dsz);
    if (!e/*err*/) goto cleanup;
    j2 = json_object_get(j1, "amount");
    e = j2 && json_is_number(j2);
    if (!e/*err*/) goto cleanup_invalid_response;
    _to->cents = json_number_value(j2);
    r = true;
    goto cleanup;
 cleanup_errno:
    error("%s", strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    error("Invalid response from paycomet: %.*s", (int)hr.dsz, hr.d);
    goto cleanup;
 cleanup_unauthorized:
    error("Not configured.");
    goto cleanup;
 cleanup:
    if (j1) json_decref(j1);
    return r;
}

/* ------------------------------------
 * ---- FORMS -------------------------
 * ------------------------------------ */

bool mpay_form_prepare(struct mpay_form *_f, enum mpay_operationType type, char *_opts[]) {
    int    e;
    char **opt;
    char **dst_s;
    int   *dst_i;
    long   l;
    _f->operationType = type;
    _f->payment.methods[0] = MPAY_METHOD_CARD;
    for (opt = _opts; _opts && *opt; opt+=2) {
        if (!strcasecmp(*opt, "amount")) {
            e = coin_parse(&_f->payment.amount, *(opt+1), NULL);
            if (!e/*err*/) goto invalid_amount;
        } else if ((dst_s = str2ptr
                    (*opt, strcasecmp,
                     "language"           , &_f->language,
                     "description"        , &_f->productDescription,
                     "productDescription" , &_f->productDescription,
                     "merchantDescription", &_f->payment.merchantDescription,
                     "order"              , &_f->payment.order,
                     "tokenUser"          , &_f->payment.tokenUser,
                     "scoring"            , &_f->payment.scoring,
                     "url_success"        , &_f->payment.urlOk,
                     "urlOk"              , &_f->payment.urlOk,
                     "url_cancel"         , &_f->payment.urlKo,
                     "urlKo"              , &_f->payment.urlKo,
                     NULL))) {
            *dst_s = *(opt+1);
        } else if ((dst_i = str2ptr
                    (*opt, strcasecmp,
                     "idUser"         , &_f->payment.idUser,
                     "secure"         , &_f->payment.secure,
                     "userInteraction", &_f->payment.userInteraction,
                     NULL))) {
            e = long_parse(&l, *(opt+1), NULL);
            if (!e/*err*/) goto invalid_number;
            *dst_i = l;
        }
        
    }
    
    
    return true;
 invalid_amount:
    error("Invalid amount: %s", *(opt+1));
    return false;
 invalid_number:
    error("Invalid number: %s", *(opt+1));
    return false;
}

json_t *mpay_form_to_json(mpay *_o, struct mpay_form *_f) {
    json_t *body = NULL;
    if (_f->operationType==MPAY_FORM_INVALID) {
        syslog(LOG_ERR, "mpay_form: Missing operationType.");
        return NULL;
    } else if (_f->operationType == MPAY_FORM_TOKENIZATION) {

    } else {
        if (!_f->payment.amount.cents || !_f->payment.amount.currency[0]) {
            syslog(LOG_ERR, "Missing parameter: `amount=100eur`.");
            return NULL;
        }
        if (!_f->payment.order) {
            syslog(LOG_ERR, "Missing parameter: `order=REF`.");
            return NULL;
        }
    }
        
    body = json_object();
    json_object_set_integer(body, "operationType", _f->operationType);
    if (_f->language) {
        json_object_set_string(body, "language", _f->language);
    }
    json_object_set_integer(body, "terminal", _o->auth_terminal);
    if (_f->operationType == MPAY_FORM_TOKENIZATION) {
        if (_f->productDescription) {
            json_object_set_string(body, "productDescription", _f->productDescription);
        }
    } else {
        json_t *payment = json_object(); long_ss l_ss;
        json_object_set_integer(payment, "terminal", _o->auth_terminal);
        if (_f->payment.methods[0]) {
            json_object_set(payment, "methods", ({
                        json_t *m  = json_array();
                        for (int i=0; i<10 && _f->payment.methods[i]; i++) {
                            json_array_append(m, json_integer(_f->payment.methods[i]));
                        }
                        m;
                    }));
        }
        if (_f->payment.excludedMethods[0]) {
            json_object_set(payment, "excludedMethods", ({
                        json_t *m = json_array();
                        for (int i=0; i<10 && _f->payment.excludedMethods[i]; i++) {
                            json_array_append(m, json_integer(_f->payment.excludedMethods[i]));
                        }
                        m;
                    }));
        }
        if (_f->payment.order) {
            json_object_set_string(payment, "order", _f->payment.order);
        }
        json_object_set_string(payment, "amount", ulong_str(_f->payment.amount.cents, &l_ss));
        json_object_set_string(payment, "currency", ({
                    for (char *c=_f->payment.amount.currency; *c; c++) {
                        *c = toupper(*c);
                    }
                    _f->payment.amount.currency;
                }));
        if (_f->payment.idUser>0) {
            json_object_set_integer(payment, "idUser", _f->payment.idUser);
        }
        if (_f->payment.tokenUser) {
            json_object_set_string(payment, "tokenUser", _f->payment.tokenUser);
        }
        json_object_set_integer(payment, "secure", _f->payment.secure);
        if (_f->payment.scoring) {
            json_object_set_string(payment, "scoring", _f->payment.scoring);
        }
        if (_f->productDescription) {
            json_object_set_string(payment, "productDescription", _f->productDescription);
        }
        if (_f->payment.merchantDescription) {
            json_object_set_string(payment, "merchantDescription", _f->payment.merchantDescription);
        }
        json_object_set_integer(payment, "userInteraction", _f->payment.userInteraction);
        if (_f->payment.urlOk) {
            json_object_set_string(payment, "urlOk", _f->payment.urlOk);
        }
        if (_f->payment.urlKo) {
            json_object_set_string(payment, "urlKo", _f->payment.urlKo);
        }
        json_object_set(body, "payment", payment);
    }
    return body;
}

bool mpay_form(mpay *_o, struct mpay_form *_form, char **_url_m) {
    json_t      *req = NULL;
    bool         ret = false;
    int          e   = 0;
    FILE        *fp  = NULL;
    crest_result rh  = {0};
    json_t      *j1  = NULL;
    char        *m1  = NULL;
    const char  *url = NULL;
    req = mpay_form_to_json(_o, _form);
    if (!req/*err*/) goto cleanup;
    e = mpay_chk_auth(_o, NULL);
    if (!e/*err*/) goto cleanup_unauthorized;
    e = crest_start_url(_o->crest, "%s/v1/form", _o->url);
    if (!e/*err*/) goto cleanup;
    e = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    e = json_dumpf(req, fp, JSON_INDENT(4));
    if (e<0/*err*/) goto cleanup_errno;
    e = crest_perform(_o->crest, &rh.ctype, &rh.rcode, &rh.d, &rh.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&j1, rh.ctype, rh.rcode, rh.d, rh.dsz);
    if (!e/*err*/) goto cleanup;
    url = json_object_get_string(j1, "challengeUrl");
    if (!url/*err*/) goto cleanup_invalid_response;
    if (_url_m) {
        *_url_m = (m1 = strdup(url));
        if (!m1/*err*/) goto cleanup_errno;
        m1 = NULL;
    }
    ret = true;
    goto cleanup;
 cleanup_errno:
    error("%s", strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    error("Received invalid response:\n%.*s", (int)rh.dsz, rh.d);
    goto cleanup;
 cleanup_unauthorized:
    error("Not configured.");
    goto cleanup;
 cleanup:
    if (j1) json_decref(j1);
    if (m1) free(m1);
    return ret;
}

/* ------------------------------------
 * ---- Check payments ----------------
 * ------------------------------------ */

bool mpay_payment_info(mpay       *_o,
                       const char *_order,
                       enum mpay_payment_state *_opt_state,
                       json_t    **_opt_info,
                       json_t    **_opt_history)
{
    int          e;
    bool         ret = false;
    FILE        *fp  = NULL;
    json_t      *j   = NULL, *j_payment, *j_state, *j_history, *j_err;
    crest_result rh;
    e = mpay_chk_auth(_o, NULL);
    if (!e/*err*/) goto cleanup_unauthorized;
    e = crest_start_url(_o->crest, "%s/v1/payments/%s/info", _o->url, _order);
    if (!e/*err*/) goto cleanup;
    e = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    e = fprintf(fp, "{\"terminal\": %li}", _o->auth_terminal);
    if (e<0/*err*/) goto cleanup_errno;
    e = crest_perform(_o->crest, &rh.ctype, &rh.rcode, &rh.d, &rh.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&j, rh.ctype, rh.rcode, rh.d, rh.dsz);
    if (!e/*err*/) goto cleanup;
    j_err = json_object_get(j, "errorCode");
    if (j_err && json_is_integer(j_err) && json_integer_value(j_err) >= 300) {
        if (_opt_state)   *_opt_state   = MPAY_PAYMENT_UNFINISHED;
        if (_opt_history) *_opt_history = json_array();
        if (_opt_info)    *_opt_info    = json_object();
        ret = true;
        goto cleanup;
    }
    j_payment = json_object_get(j, "payment");
    e = j_payment && json_is_object(j_payment);
    if (!e/*err*/) goto cleanup_invalid_response;
    j_state = json_object_get(j_payment, "state");
    e = j_state && json_is_integer(j_state);
    if (!e/*err*/) goto cleanup_invalid_response;
    j_history = json_object_get(j_payment, "history");
    e = j_history && json_is_array(j_history);
    if (!e/*err*/) goto cleanup_invalid_response;
    if (_opt_state) {
        e = json_integer_value(j_state);
        if (e != 0 && e != 1 && e != 2/*err*/) goto cleanup_invalid_response;
        *_opt_state = e;
    }
    if (_opt_history) {
        *_opt_history = json_incref(j_history);
    }
    if (_opt_info) {
        json_object_del(j_payment, "history");
        *_opt_info = json_incref(j_payment);
    }    
    ret = true;
    goto cleanup;
 cleanup_errno:
    error("%s", strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    error("Received invalid response:\n%.*s", (int)rh.dsz, rh.d);
    goto cleanup;
 cleanup_unauthorized:
    error("Not configured.");
    goto cleanup;
 cleanup:
    if (j) json_decref(j);
    return ret;
}

json_t *payment_info_to_refund(json_t *_i, coin_t _opt_different_amount) {
    json_t *o = NULL;
    json_t *i_terminal = json_incref(json_object_get(_i, "terminal"));
    json_t *i_amount   = ({
            json_t *j; long_ss ls;
            if (_opt_different_amount.cents) {
                j = json_string(long_str(_opt_different_amount.cents, &ls));
            } else {
                j = json_incref(json_object_get(_i, "amount"));
            }
            j;
        });
    json_t *i_currency = ({
            json_t *j; char *s,*p;
            if (_opt_different_amount.cents) {
                s = strdupa(_opt_different_amount.currency);
                for (p = s; *p; p++) *p = toupper(*p);
                j = json_string(s);
            } else {
                j = json_incref(json_object_get(_i, "currency"));
            }
            j;
        });
    json_t *i_authCode   = json_incref(json_object_get(_i, "authCode"));
    json_t *i_originalIp = json_incref(json_object_get(_i, "originalIp"));
    int res = o && i_terminal && i_amount && i_currency && i_authCode && i_originalIp;
    if (!res/*err*/) {
        syslog(LOG_ERR, "%s: Received payment info.", __func__);
        json_decref(o);
        json_decref(i_terminal);
        json_decref(i_amount);
        json_decref(i_currency);
        json_decref(i_authCode);
        json_decref(i_originalIp);
        return NULL;
    }
    json_object_set(o, "terminal"  , i_terminal);
    json_object_set(o, "amount"    , i_amount);
    json_object_set(o, "currency"  , i_currency);
    json_object_set(o, "authCode"  , i_authCode);
    json_object_set(o, "originalIp", i_originalIp);
    return o;
}


bool mpay_payment_refund (mpay         *_o,
                          const char   *_order,
                          json_t       *_info,
                          coin_t        _opt_different_amount,
                          json_t      **_opt_result)
{
    int          e;
    bool         ret = false;
    json_t      *req = NULL;
    FILE        *fp;
    crest_result hr;
    json_t      *j   = NULL;
    req = payment_info_to_refund(_info, _opt_different_amount);
    if (!req/*err*/) goto cleanup;
    e = mpay_chk_auth(_o, NULL);
    if (!e/*err*/) goto cleanup_unauthorized;
    e = crest_start_url(_o->crest, "%s/v1/payments/%s/refund", _o->url, _order);
    if (!e/*err*/) goto cleanup;
    e = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    e = json_dumpf(req, fp, JSON_INDENT(4));
    if (e<0/*err*/) goto cleanup_errno;
    e = crest_perform(_o->crest, &hr.ctype, &hr.rcode, &hr.d, &hr.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&j, hr.ctype, hr.rcode, hr.d, hr.dsz);
    if (!e/*err*/) goto cleanup;
    if (_opt_result) {
        *_opt_result = json_incref(j);
    }
    ret = true;
    goto cleanup;
 cleanup_errno:
    error("%s", strerror(errno));
    goto cleanup;
 cleanup_unauthorized:
    error("Not configured.");
    goto cleanup;
 cleanup:
    if (j) json_decref(j);
    return ret;
}
