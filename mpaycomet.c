#include "mpaycomet.h"
#include <string.h>
#include <str/str2num.h>
#include <str/str2ptr.h>
#include <str/strdupa.h>
#include <types/long_ss.h>
#include <curl/crest.h>
#include <jansson/extra.h>


struct mpay {
    crest *crest;
    const char *url;
    struct {
        const char   *api_token;
        unsigned long terminal;
    } auth;
};

bool
mpay_create(mpay **_o, const char *_opts[]) {
    mpay        *o   = NULL;
    int          res;
    const char **opt;
    o = calloc(1, sizeof(struct mpay));
    if (!o/*err*/) goto cleanup_errno;
    res = crest_create(&o->crest);
    if (!res/*err*/) goto cleanup;
    o->url = "https://rest.paycomet.com";
    for (opt = _opts; _opts && *opt; opt++) {
        switch(str2num(*opt, strcasecmp,
                       "paycomet_api_token", 1,
                       "paycomet_terminal" , 2,
                       NULL)) {
        case 1:
            o->auth.api_token = *(opt+1); break;
        case 2:
            res = ulong_parse(&o->auth.terminal, *(opt+1), NULL);
            if (!res/*err*/) goto cleanup_invalid_terminal;
            break;
        }
    }
    res = o->auth.api_token && o->auth.terminal;
    if (!res/*err*/) goto cleanup_missing_params;
    res = crest_set_auth_header(o->crest, "PAYCOMET-API-TOKEN: %s", o->auth.api_token);
    if (!res/*err*/) goto cleanup;
    *_o = o;
    return true;
 cleanup_errno:
    syslog(LOG_ERR, "%s: %s", __func__, strerror(errno));
    goto cleanup;
 cleanup_missing_params:
    if (!o->auth.api_token) syslog(LOG_ERR, "Missing parameter: `paycomet_api_token`.");
    if (!o->auth.terminal)  syslog(LOG_ERR, "Missing parameter: `paycomet_terminal`.");
    goto cleanup;
 cleanup_invalid_terminal:
    syslog(LOG_ERR, "Invalid paycomet_terminal: %s", *(opt+1));
    goto cleanup;
 cleanup:
    mpay_destroy(o);
    return false;
}

void
mpay_destroy(mpay *_o) {
    if (_o) {
        if (_o->crest) crest_destroy(_o->crest);
        free(_o);
    }
}

bool
mpay_methods_get(mpay *_o, json_t **_r) {
    int          res;
    bool         ret = false;
    crest_result r;
    FILE        *fp = NULL;
    json_t      *j1 = NULL;
    res = crest_start_url(_o->crest, "%s/v1/methods", _o->url);
    if (!res/*err*/) goto cleanup;
    res = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!res/*err*/) goto cleanup;
    res = fprintf(fp, "{\"terminal\": %li}", _o->auth.terminal);
    if (res<0/*err*/) goto cleanup_errno;
    res = crest_perform(_o->crest, &r.ctype, &r.rcode, &r.d, &r.dsz);
    if (!res/*err*/) goto cleanup;
    res = crest_get_json(&j1, r.ctype, r.rcode, r.d, r.dsz);
    if (!res/*err*/) goto cleanup;
    if (_r) {
        *_r = json_incref(j1);
    }
    ret = true;
    goto cleanup;
 cleanup_errno:
    syslog(LOG_ERR, "%s: %s", __func__, strerror(errno));
    goto cleanup;
 cleanup:
    if (j1) json_decref(j1);
    return ret;
}

bool
mpay_exchange(mpay *_o, coin_t _fr, coin_t *_to, const char *_currency) {
    int          res;
    bool         ret = false;
    crest_result r;
    FILE        *fp = NULL;
    json_t      *j1 = NULL,*j2;
    res = crest_start_url(_o->crest, "%s/v1/exchange", _o->url);
    if (!res/*err*/) goto cleanup;
    res = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!res/*err*/) goto cleanup;
    strncpy(_to->currency, _currency, sizeof(_to->currency)-1);
    for (char *c=_fr.currency; *c; c++)  *c=toupper(*c);
    for (char *c=_to->currency; *c; c++) *c=toupper(*c);
    res = fprintf(fp,
                  "{"
                  "    \"terminal\"        : %li, "   "\n"
                  "    \"amount\"          : %li,"    "\n"
                  "    \"originalCurrency\": \"%s\"," "\n"
                  "    \"finalCurrency\"   : \"%s\""  "\n"
                  "}"                                 "\n",
                  _o->auth.terminal,
                  _fr.cents,
                  _fr.currency,
                  _to->currency);
    if (res<0/*err*/) goto cleanup_errno;
    res = crest_perform(_o->crest, &r.ctype, &r.rcode, &r.d, &r.dsz);
    if (!res/*err*/) goto cleanup;
    res = crest_get_json(&j1, r.ctype, r.rcode, r.d, r.dsz);
    if (!res/*err*/) goto cleanup;
    j2 = json_object_get(j1, "amount");
    res = j2 && json_is_number(j2);
    if (!res/*err*/) goto cleanup_invalid_response;
    _to->cents = json_number_value(j2);
    ret = true;
    goto cleanup;
 cleanup_errno:
    syslog(LOG_ERR, "%s: %s", __func__, strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    syslog(LOG_ERR, "%s: Invalid response from paycomet: %.*s", __func__, (int)r.dsz, r.d);
    goto cleanup;
 cleanup:
    if (j1) json_decref(j1);
    return ret;
}

bool
mpay_form_prepare(struct mpay_form *_f, enum mpay_operationType type, char *_opts[]) {
    int    res;
    char **opt;
    char **dst_s;
    int   *dst_i;
    long   l;
    _f->operationType = type;
    _f->payment.methods[0] = MPAY_METHOD_CARD;
    for (opt = _opts; _opts && *opt; opt+=2) {
        if (!strcasecmp(*opt, "amount")) {
            res = coin_parse(&_f->payment.amount, *(opt+1), NULL);
            if (!res/*err*/) goto invalid_amount;
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
            res = long_parse(&l, *(opt+1), NULL);
            if (!res/*err*/) goto invalid_number;
            *dst_i = l;
        }
        
    }
    
    
    return true;
 invalid_amount:
    syslog(LOG_ERR, "Invalid amount: %s", *(opt+1));
    return false;
 invalid_number:
    syslog(LOG_ERR, "Invalid number: %s", *(opt+1));
    return false;
}

json_t *
mpay_form_to_json(mpay *_o, struct mpay_form *_f) {
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
    json_object_set_integer(body, "terminal", _o->auth.terminal);
    if (_f->operationType == MPAY_FORM_TOKENIZATION) {
        if (_f->productDescription) {
            json_object_set_string(body, "productDescription", _f->productDescription);
        }
    } else {
        json_t *payment = json_object(); long_ss l_ss;
        json_object_set_integer(payment, "terminal", _o->auth.terminal);
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

bool
mpay_form(mpay *_o, struct mpay_form *_form, char **_url_m) {
    json_t      *req = NULL;
    bool         ret = false;
    int          res;
    FILE        *fp = NULL;
    crest_result r;
    json_t      *j1 = NULL;
    char        *m1 = NULL;
    const char  *url = NULL;
    req = mpay_form_to_json(_o, _form);
    if (!req/*err*/) goto cleanup;
    res = crest_start_url(_o->crest, "%s/v1/form", _o->url);
    if (!res/*err*/) goto cleanup;
    res = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!res/*err*/) goto cleanup;
    res = json_dumpf(req, fp, JSON_INDENT(4));
    if (res<0/*err*/) goto cleanup_errno;
    res = crest_perform(_o->crest, &r.ctype, &r.rcode, &r.d, &r.dsz);
    if (!res/*err*/) goto cleanup;
    res = crest_get_json(&j1, r.ctype, r.rcode, r.d, r.dsz);
    if (!res/*err*/) goto cleanup;
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
    syslog(LOG_ERR, "%s: %s", __func__, strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    syslog(LOG_ERR, "%s: Received invalid response:\n%.*s", __func__, (int)r.dsz, r.d);
    goto cleanup;
 cleanup:
    if (j1) json_decref(j1);
    if (m1) free(m1);
    return ret;
}

bool
mpay_heartbeat(mpay *_o, FILE *_fp1) {
    int          res;
    bool         ret = false;
    crest_result r;
    FILE        *fp = NULL;
    json_t      *j1 = NULL;
    res = crest_start_url(_o->crest, "%s/v1/heartbeat", _o->url);
    if (!res/*err*/) goto cleanup;
    res = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!res/*err*/) goto cleanup;
    res = fprintf(fp, "{\"terminal\": %li}", _o->auth.terminal);
    if (res<0/*err*/) goto cleanup_errno;
    res = crest_perform(_o->crest, &r.ctype, &r.rcode, &r.d, &r.dsz);
    if (!res/*err*/) goto cleanup;
    res = crest_get_json(&j1, r.ctype, r.rcode, r.d, r.dsz);
    if (!res/*err*/) goto cleanup;
    const char *ping_paycomet    = json_object_get_string (j1, "time");
    const char *ping_processor   = json_object_get_string (j1, "processorTime");
    bool        status_processor = json_object_get_boolean(j1, "processorStatus");
    res = ping_paycomet && ping_processor && status_processor;
    if (!res/*err*/) goto cleanup_invalid_response;
    fprintf(_fp1, "Paycomet ping    : %s\n", ping_paycomet);
    fprintf(_fp1, "Processor ping   : %s\n", ping_processor);
    fprintf(_fp1, "Processor status : %s\n", (status_processor)?"on":"off");
    ret = true;
    goto cleanup;
 cleanup_errno:
    syslog(LOG_ERR, "%s: %s", __func__, strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    syslog(LOG_ERR, "%s: Received invalid response:\n%.*s", __func__, (int)r.dsz, r.d);
    goto cleanup;
 cleanup:
    if (j1) json_decref(j1);
    return ret;
}

bool
mpay_payment_info(mpay       *_o,
                  const char *_order,
                  enum mpay_payment_state *_opt_state,
                  json_t    **_opt_info,
                  json_t    **_opt_history)
{
    int          res;
    bool         ret = false;
    FILE        *fp  = NULL;
    json_t      *j   = NULL, *j_payment, *j_state, *j_history, *j_err;
    crest_result r;
    res = crest_start_url(_o->crest, "%s/v1/payments/%s/info", _o->url, _order);
    if (!res/*err*/) goto cleanup;
    res = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!res/*err*/) goto cleanup;
    res = fprintf(fp, "{\"terminal\": %li}", _o->auth.terminal);
    if (res<0/*err*/) goto cleanup_errno;
    res = crest_perform(_o->crest, &r.ctype, &r.rcode, &r.d, &r.dsz);
    if (!res/*err*/) goto cleanup;
    res = crest_get_json(&j, r.ctype, r.rcode, r.d, r.dsz);
    if (!res/*err*/) goto cleanup;
    j_err = json_object_get(j, "errorCode");
    if (j_err && json_is_integer(j_err) && json_integer_value(j_err) >= 300) {
        if (_opt_state)   *_opt_state   = MPAY_PAYMENT_UNFINISHED;
        if (_opt_history) *_opt_history = json_array();
        if (_opt_info)    *_opt_info    = json_object();
        ret = true;
        goto cleanup;
    }
    j_payment = json_object_get(j, "payment");
    res = j_payment && json_is_object(j_payment);
    if (!res/*err*/) goto cleanup_invalid_response;
    j_state = json_object_get(j_payment, "state");
    res = j_state && json_is_integer(j_state);
    if (!res/*err*/) goto cleanup_invalid_response;
    j_history = json_object_get(j_payment, "history");
    res = j_history && json_is_array(j_history);
    if (!res/*err*/) goto cleanup_invalid_response;
    if (_opt_state) {
        res = json_integer_value(j_state);
        if (res != 0 && res != 1 && res != 2/*err*/) goto cleanup_invalid_response;
        *_opt_state = res;
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
    syslog(LOG_ERR, "%s: %s", __func__, strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    syslog(LOG_ERR, "%s: Received invalid response:\n%.*s", __func__, (int)r.dsz, r.d);
    goto cleanup;
 cleanup:
    if (j) json_decref(j);
    return ret;
}

json_t *
payment_info_to_refund(json_t *_i, coin_t _opt_different_amount) {
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


bool
mpay_payment_refund (mpay         *_o,
                     const char   *_order,
                     json_t       *_info,
                     coin_t        _opt_different_amount,
                     json_t      **_opt_result)
{
    int          res;
    bool         ret = false;
    json_t      *req = NULL;
    FILE        *fp;
    crest_result r;
    json_t      *j   = NULL;
    req = payment_info_to_refund(_info, _opt_different_amount);
    if (!req/*err*/) goto cleanup;
    res = crest_start_url(_o->crest, "%s/v1/payments/%s/refund", _o->url, _order);
    if (!res/*err*/) goto cleanup;
    res = crest_post_data(_o->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!res/*err*/) goto cleanup;
    res = json_dumpf(req, fp, JSON_INDENT(4));
    if (res<0/*err*/) goto cleanup_errno;
    res = crest_perform(_o->crest, &r.ctype, &r.rcode, &r.d, &r.dsz);
    if (!res/*err*/) goto cleanup;
    res = crest_get_json(&j, r.ctype, r.rcode, r.d, r.dsz);
    if (!res/*err*/) goto cleanup;
    if (_opt_result) {
        *_opt_result = json_incref(j);
    }
    ret = true;
    goto cleanup;
 cleanup_errno:
    syslog(LOG_ERR, "%s: %s", __func__, strerror(errno));
    goto cleanup;
 cleanup:
    if (j) json_decref(j);
    return ret;
}
