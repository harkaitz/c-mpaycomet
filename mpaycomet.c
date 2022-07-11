#include "mpaycomet.h"
#include <str/mtext.h>
#include <string.h>
#include <str/sizes.h>
#include <str/str2num.h>
#include <str/str2ptr.h>
#include <str/strdupa.h>
#include <types/long_ss.h>
#include <types/time_ss.h>
#include <curl/crest.h>
#include <jansson/extra.h>
#include <syslog.h>
#ifdef NO_GETTEXT
#  define _(T) T
#else
#  include <libintl.h>
#  define _(T) dgettext("c-mpaycomet", T)
#endif

struct mpay {
    crest  *crest;
    str256  auth_api_token;
    long    auth_terminal;
    bool    auth_ok;
};

const char *MPAY_URL = "https://rest.paycomet.com";

/* ------------------------------------
 * ---- CONSTRUCTOR AND DESTRUCTOR ----
 * ------------------------------------ */

bool mpay_create(mpay **_mpay) {
    mpay          *mpay;
    int            e;
    mpay = calloc(1, sizeof(struct mpay));
    if (!mpay/*err*/) goto cleanup_errno;
    e = crest_create(&mpay->crest);
    if (!e/*err*/) goto cleanup;
    *_mpay = mpay;
    return true;
 cleanup_errno:
    syslog(LOG_ERR, "%s", strerror(errno));
    goto cleanup;
 cleanup:
    mpay_destroy(mpay);
    return false;
}

void mpay_destroy(mpay *_mpay) {
    if (_mpay) {
        if (_mpay->crest) {
            crest_destroy(_mpay->crest);
        }
        free(_mpay);
    }
}

/* ------------------------------------
 * ---- AUTHORIZATION -----------------
 * ------------------------------------ */

void mpay_set_auth(mpay *_mpay, const char *_api_token, const char *_terminal) {
    _mpay->auth_ok = false;
    if (_api_token) {
        strncpy(_mpay->auth_api_token, _api_token, sizeof(_mpay->auth_api_token)-1);
    }
    if (_terminal) {
        int e = long_parse(&_mpay->auth_terminal, _terminal, NULL);
        if (!e/*err*/) { _mpay->auth_terminal = -1; }
    }
}

bool mpay_chk_auth(mpay *_mpay, const char **_reason) {
    if (_mpay->auth_ok == false) {
        if (_mpay->auth_terminal == 0) {
            if(_reason) *_reason = _("Missing terminal number");
            return false;
        } else if (_mpay->auth_terminal < 0) {
            if(_reason) *_reason = _("Invalid terminal");
            return false;
        } else if (_mpay->auth_api_token[0] == '\0') {
            if(_reason) *_reason = _("Missing API token");
            return false;
        }
        int e = crest_set_auth_header(_mpay->crest,
                                      "PAYCOMET-API-TOKEN: %s",
                                      _mpay->auth_api_token);
        if (!e) {
            if(_reason) *_reason = _("Internal error");
            return false;
        }
    }
    _mpay->auth_ok = true;
    return true;
}

/* ------------------------------------
 * ---- CHECK IT WORKS ----------------
 * ------------------------------------ */

bool mpay_heartbeat(mpay *_mpay, FILE *_fp1) {
    bool           retval          = false;
    crest_result   hr              = {0};
    FILE          *fp              = NULL;
    json_t        *j1              = NULL;
    int            e;
    e = mpay_chk_auth(_mpay, NULL);
    if (!e/*err*/) return false;
    e = crest_start_url(_mpay->crest, "%s/v1/heartbeat", MPAY_URL);
    if (!e/*err*/) goto cleanup;
    e = crest_post_data(_mpay->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    e = fprintf(fp, "{\"terminal\": %li}", _mpay->auth_terminal);
    if (e<0/*err*/) goto cleanup_errno;
    e = crest_perform(_mpay->crest, &hr.ctype, &hr.rcode, &hr.d, &hr.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&j1, hr.ctype, hr.rcode, hr.d, hr.dsz);
    if (!e/*err*/) goto cleanup;
    const char *ping_paycomet      = json_object_get_string (j1, "time");
    const char *ping_processor     = json_object_get_string (j1, "processorTime");
    e = ping_paycomet && ping_processor;
    if (!e/*err*/) goto cleanup_invalid_response;
    if (_fp1) {
        fprintf(_fp1, "Paycomet ping     : %s\n", ping_paycomet);
        fprintf(_fp1, "Processor ping    : %s\n", ping_processor);
    }
    retval = true;
 cleanup:
    if (j1) json_decref(j1);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "%s", strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    syslog(LOG_ERR, "Received invalid response:\n%.*s", (int)hr.dsz, hr.d);
    goto cleanup;
}

/* ------------------------------------
 * ---- AUXILIARY METHODS -------------
 * ------------------------------------ */

bool mpay_methods_get(mpay *_mpay, json_t **_r) {
    crest_result   hr              = {0};
    bool           retval          = false;
    FILE          *fp              = NULL;
    json_t        *j1              = NULL;
    int            e;
    e = mpay_chk_auth(_mpay, NULL);
    if (!e/*err*/) return false;
    e = crest_start_url(_mpay->crest, "%s/v1/methods", MPAY_URL);
    if (!e/*err*/) goto cleanup;
    e = crest_post_data(_mpay->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    e = fprintf(fp, "{\"terminal\": %li}", _mpay->auth_terminal);
    if (e<0/*err*/) goto cleanup_errno;
    e = crest_perform(_mpay->crest, &hr.ctype, &hr.rcode, &hr.d, &hr.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&j1, hr.ctype, hr.rcode, hr.d, hr.dsz);
    if (!e/*err*/) goto cleanup;
    if (_r) {
        *_r = json_incref(j1);
    }
    retval = true;
 cleanup:
    if (j1) json_decref(j1);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "%s", strerror(errno));
    goto cleanup;
}

bool mpay_exchange(mpay *_mpay, coin_t _fr, coin_t *_to, const char *_currency) {
    
    bool           r               = false;
    crest_result   hr              = {0};
    FILE          *fp              = NULL;
    json_t        *resp_j          = NULL,*j2;
    int            e;
    
    e = mpay_chk_auth(_mpay, NULL);
    if (!e/*err*/) return false;
    e = crest_start_url(_mpay->crest, "%s/v1/exchange", MPAY_URL);
    if (!e/*err*/) goto cleanup;
    e = crest_post_data(_mpay->crest, CREST_CONTENT_TYPE_JSON, &fp);
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
                _mpay->auth_terminal,
                _fr.cents,
                _fr.currency,
                _to->currency);
    if (e<0/*err*/) goto cleanup_errno;
    e = crest_perform(_mpay->crest, &hr.ctype, &hr.rcode, &hr.d, &hr.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&resp_j, hr.ctype, hr.rcode, hr.d, hr.dsz);
    if (!e/*err*/) goto cleanup;
    j2 = json_object_get(resp_j, "amount");
    e = j2 && json_is_number(j2);
    if (!e/*err*/) goto cleanup_invalid_response;
    _to->cents = json_number_value(j2);
    r = true;
 cleanup:
    if (resp_j) json_decref(resp_j);
    return r;
 cleanup_errno:
    syslog(LOG_ERR, "%s", strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    syslog(LOG_ERR, "Invalid response from paycomet: %.*s", (int)hr.dsz, hr.d);
    goto cleanup;
}

/* ------------------------------------
 * ---- FORMS -------------------------
 * ------------------------------------ */

bool
mpay_form_prepare(struct mpay_form *_f, enum mpay_operationType type, char *_opts[]) {
    
    char         **opt;
    char         **dst_s;
    int           *dst_i;
    time_t        *dst_t;
    long           l;
    int            e;

    _f->operationType = type;
    _f->payment.methods[0] = MPAY_METHOD_CARD;
    for (opt = _opts; _opts && *opt; opt+=2) {
        if (!strcasecmp(*opt, "amount")) {
            e = coin_parse(&_f->payment.amount, *(opt+1), NULL);
            if (!e/*err*/) return false;
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
                     "periodicity"    , &_f->subscription.periodicity,
                     NULL))) {
            e = long_parse(&l, *(opt+1), NULL);
            if (!e/*err*/) return false;
            *dst_i = l;
        } else if ((dst_t = str2ptr
                    (*opt, strcasecmp,
                     "date_start", &_f->subscription.start_date,
                     "date_end"  , &_f->subscription.end_date,
                     NULL))) {
            e = time_day_parse(dst_t, *(opt+1), NULL);
            if (!e/*err*/) return false;
        }
        
    }
    return true;
}

json_t *
mpay_form_to_json(mpay *_mpay, struct mpay_form *_f) {
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
    json_object_set_integer(body, "terminal", _mpay->auth_terminal);
    if (_f->operationType == MPAY_FORM_TOKENIZATION) {
        if (_f->productDescription) {
            json_object_set_string(body, "productDescription", _f->productDescription);
        }
    } else {
        json_t *payment = json_object(); long_ss l_ss;
        json_object_set_integer(payment, "terminal", _mpay->auth_terminal);
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
    if (_f->operationType == MPAY_FORM_SUBSCRIPTION) {
        json_t        *subscription_j  = json_object();
        struct tm      tm              = {0};
        char           txt[20]         = {0};
        time_t         date_start      = 0;
        time_t         date_end        = 0;
        int            periodicity     = 0;
        
        if (_f->subscription.start_date) {
            date_start = _f->subscription.start_date;
        } else {
            date_start = time(NULL);
        }

        if (_f->subscription.end_date) {
            date_end = _f->subscription.end_date;
        } else {
            date_end = time(NULL)+(3600*24*364*5);
        }

        if (_f->subscription.periodicity) {
            periodicity = _f->subscription.periodicity;
        } else {
            periodicity = 30;
        }
    
        localtime_r(&date_start, &tm);
        snprintf(txt, sizeof(txt)-1, "%04i%02i%02i", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);
        json_object_set_string(subscription_j, "startDate", txt);
        
        localtime_r(&date_end, &tm);
        snprintf(txt, sizeof(txt)-1, "%04i%02i%02i", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);
        json_object_set_string(subscription_j, "endDate", txt);

        snprintf(txt, sizeof(txt)-1, "%i", periodicity);
        json_object_set_string(subscription_j, "periodicity", txt);

        json_object_set(body, "subscription", subscription_j);
    }
        

        
        
    
    return body;
}

bool
mpay_form(mpay *_mpay, struct mpay_form *_form, char **_url_m) {
    
    json_t        *req             = NULL;
    bool           retval          = false;
    FILE          *fp              = NULL;
    crest_result   rh              = {0};
    json_t        *response        = NULL;
    const char    *url             = NULL;
    int            e;

    /* Check _mpay has the credentials. */
    e = mpay_chk_auth(_mpay, NULL);
    if (!e/*err*/) return false;

    /* Convert C struct to Json. */
    req = mpay_form_to_json(_mpay, _form);
    if (!req/*err*/) goto cleanup;

    /* Set the requested url. */
    e = crest_start_url(_mpay->crest, "%s/v1/form", MPAY_URL);
    if (!e/*err*/) goto cleanup;

    /* Set the request body. */
    e = crest_post_data(_mpay->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    e = json_dumpf(req, fp, JSON_INDENT(4));
    if (e<0/*err*/) goto c_errno;

    /* Perform the request and get response. */
    e = crest_perform(_mpay->crest, &rh.ctype, &rh.rcode, &rh.d, &rh.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&response, rh.ctype, rh.rcode, rh.d, rh.dsz);
    if (!e/*err*/) goto cleanup;

    /* Print the JSON (When debugging) */
    json_dumpf(response, stderr, JSON_INDENT(4));
    
    /* Get URL. */
    url = json_object_get_string(response, "challengeUrl");
    if (!url/*err*/) goto c_invalid_response;
    if (_url_m) {
        *_url_m = strdup(url);
        if (!*_url_m/*err*/) goto c_errno;
    }

    /* Returns. */
    retval = true;
 cleanup:
    json_decref(response);
    return retval;
 c_errno:
    syslog(LOG_ERR, "%s", strerror(errno));
    goto cleanup;
 c_invalid_response:
    syslog(LOG_ERR, "Invalid response:\n%.*s", (int)rh.dsz, rh.d);
    goto cleanup;
}


/* ------------------------------------
 * ---- USERS -------------------------
 * ------------------------------------ */

/* ------------------------------------
 * ---- Check payments ----------------
 * ------------------------------------ */

bool mpay_payment_info(mpay *_mpay,
                       const char *_order,
                       enum mpay_payment_state *_opt_state,
                       json_t    **_opt_info,
                       json_t    **_opt_history)
{
    int          e;
    bool         ret = false;
    FILE        *fp  = NULL;
    json_t      *j   = NULL, *j_payment, *j_state, *j_history, *j_err;
    int          n;
    crest_result rh;

    /* Check _mpay has the credentials. */
    e = mpay_chk_auth(_mpay, NULL);
    if (!e/*err*/) return false;

    /* Set the requested url. */
    e = crest_start_url(_mpay->crest, "%s/v1/payments/%s/info", MPAY_URL, _order);
    if (!e/*err*/) goto cleanup;

    /* Set the request body. */
    e = crest_post_data(_mpay->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    e = fprintf(fp, "{\"terminal\": %li}", _mpay->auth_terminal);
    if (e<0/*err*/) goto cleanup_errno;

    /* Perform the request and get response. */
    e = crest_perform(_mpay->crest, &rh.ctype, &rh.rcode, &rh.d, &rh.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&j, rh.ctype, rh.rcode, rh.d, rh.dsz);
    if (!e/*err*/) goto cleanup;

    /* Handle normal error cases. */
    j_err = json_object_get(j, "errorCode");
    if (j_err &&
        json_is_integer(j_err) &&
        ((n=json_integer_value(j_err)) >= 300 || n==130)) {
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
 cleanup:
    if (j) json_decref(j);
    return ret;
 cleanup_errno:
    syslog(LOG_ERR, "%s", strerror(errno));
    goto cleanup;
 cleanup_invalid_response:
    syslog(LOG_ERR, "Invalid response: %.*s", (int)rh.dsz, rh.d);
    goto cleanup;
}



bool mpay_subscription_info(mpay *_mpay,
                            const char                   *_order,
                            enum mpay_subscription_state *_opt_state) {

    bool           retval          = false;
    json_t        *response        = NULL;
    crest_result   rh;
    FILE          *fp;
    int            e;

    /* Check _mpay has the credentials. */
    e = mpay_chk_auth(_mpay, NULL);
    if (!e/*err*/) return false;

    /* Set the requested url. */
    //e = crest_start_url(_mpay->crest, "%s/v1/payments/%s/info", MPAY_URL, _order);
    //e = crest_start_url(_mpay->crest, "%s/v1/subscription/%s/info", MPAY_URL, _order);
    e = crest_start_url(_mpay->crest, "%s/v1/payments/search", MPAY_URL);
    //e = crest_start_url(_mpay->crest, "%s/v1/cards", MPAY_URL);
    if (!e/*err*/) goto cleanup;

    /* Set the request body. */
    e = crest_post_data(_mpay->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    #if 0
    e = fprintf(fp,
                "{"                             "\n"
                //"    \"payment\": {"            "\n"
                "        \"terminal\": %li"     ",\n"
                "        \"pan\": \"123\""            ",\n"
                "        \"expiryMonth\": \"01\"" ",\n"
                "        \"expiryYear\": \"30\"" "\n"
                //"    }"                         "\n"
                "}"                             "\n"
                , _mpay->auth_terminal);
    #endif
    #if 1
    e = fprintf(fp,
                "{"                          "\n"
                "        \"currency\": \"EUR\"," "\n"
                "        \"sortOrder\": \"DESC\"," "\n"
                "        \"sortType\": 0,"          "\n"
                "        \"terminal\": %li,"     "\n"
                "        \"operations\": [%i]," "\n"
                "        \"minAmount\": 0," "\n"
                "        \"maxAmount\": 0," "\n"
                "        \"state\": 2," "\n"
                "        \"fromDate\": \"20220507020000\","
                "        \"toDate\" : \"20221007020000\""
                //"        \"order\": \"%s\""  "\n"
                "}"                          "\n",
                _mpay->auth_terminal,
                MPAY_FORM_SUBSCRIPTION
                //,_order
                );
    #endif
    if (e<0/*err*/) goto cleanup_errno;

    /* Perform the request and get response. */
    e = crest_perform(_mpay->crest, &rh.ctype, &rh.rcode, &rh.d, &rh.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&response, rh.ctype, rh.rcode, rh.d, rh.dsz);
    if (!e/*err*/) goto cleanup;

    /* Print the JSON (When debugging) */
    json_dumpf(response, stderr, JSON_INDENT(4));

    /* Returns. */
    retval = true;
 cleanup:
    json_decref(response);
    return retval;
 cleanup_errno:
    syslog(LOG_ERR, "%s", strerror(errno));
    goto cleanup;
}



json_t *payment_info_to_refund(json_t *_i, coin_t _opt_different_amount) {
    json_t *o = json_object();
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
    json_t *p = json_object();
    json_object_set(p, "terminal"  , i_terminal);
    json_object_set(p, "amount"    , i_amount);
    json_object_set(p, "currency"  , i_currency);
    json_object_set(p, "authCode"  , i_authCode);
    json_object_set(p, "originalIp", i_originalIp);
    json_object_set(o, "payment", p);
    return o;
}


bool mpay_payment_refund (mpay       *_mpay,
                          const char *_order,
                          json_t     *_info,
                          coin_t      _opt_different_amount,
                          json_t    **_opt_result)
{
    int          e;
    bool         ret = false;
    json_t      *req = NULL;
    FILE        *fp;
    crest_result hr;
    json_t      *j   = NULL;
    req = payment_info_to_refund(_info, _opt_different_amount);
    if (!req/*err*/) goto cleanup;
    e = mpay_chk_auth(_mpay, NULL);
    if (!e/*err*/) goto cleanup_unauthorized;
    e = crest_start_url(_mpay->crest, "%s/v1/payments/%s/refund", MPAY_URL, _order);
    if (!e/*err*/) goto cleanup;
    e = crest_post_data(_mpay->crest, CREST_CONTENT_TYPE_JSON, &fp);
    if (!e/*err*/) goto cleanup;
    e = json_dumpf(req, fp, JSON_INDENT(4));
    if (e<0/*err*/) goto cleanup_errno;
    e = crest_perform(_mpay->crest, &hr.ctype, &hr.rcode, &hr.d, &hr.dsz);
    if (!e/*err*/) goto cleanup;
    e = crest_get_json(&j, hr.ctype, hr.rcode, hr.d, hr.dsz);
    if (!e/*err*/) goto cleanup;
    if (_opt_result) {
        *_opt_result = json_incref(j);
    }
    ret = true;
    goto cleanup;
 cleanup_errno:
    syslog(LOG_ERR, "%s", strerror(errno));
    goto cleanup;
 cleanup_unauthorized:
    syslog(LOG_ERR, "Not configured.");
    goto cleanup;
 cleanup:
    if (j) json_decref(j);
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
