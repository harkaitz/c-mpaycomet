#ifndef MPAYCOMET_H
#define MPAYCOMET_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <types/coin.h>

typedef struct mpay   mpay;
typedef struct json_t json_t;
struct mpay_form;

enum mpay_method {
    MPAY_METHOD_INVALID = 0,
    MPAY_METHOD_CARD    = 1
};
enum mpay_operationType {
    MPAY_FORM_INVALID              = 0,
    MPAY_FORM_AUTHORIZATION        = 1,
    MPAY_FORM_REFUND               = 2, /* This is undocumented. */
    MPAY_FORM_PREAUTHORIZATION     = 3,
    MPAY_FORM_SUBSCRIPTION         = 9,
    MPAY_FORM_TOKENIZATION         = 107,
    MPAY_FORM_AUTHORIZATION_BY_REF = 114,
    MPAY_FORM_AUTHORIZATION_DCC    = 116
};
enum mpay_payment_state {
    MPAY_PAYMENT_FAILED     = 0,
    MPAY_PAYMENT_CORRECT    = 1,
    MPAY_PAYMENT_UNFINISHED = 2,
    MPAY_PAYMENT_REFUNDED   = -1 /* Not part of REST, it marks it got a refund. */
};



/* Constructor and destructor. */
bool mpay_create  (mpay **_o);
void mpay_destroy (mpay  *_o);

/* Setup authorization. */
void mpay_set_auth (mpay  *_o, const char *_api_token, const char *_terminal);
bool mpay_chk_auth (mpay  *_o, const char **_reason);

/* Check it works. */
bool mpay_heartbeat    (mpay *_o, FILE *_fp1);

/* Form. */
bool mpay_form_prepare (struct mpay_form *_f, enum mpay_operationType type, char *opts[]);
bool mpay_form         (mpay *_o, struct mpay_form *_form, char **_url_m);

/* Auxiliary methods. */
bool mpay_methods_get  (mpay *_o, json_t **_opt_r);
bool mpay_exchange     (mpay *_o, coin_t _fr, coin_t *_to, const char *_currency);

/* Check payments. */
bool mpay_payment_info  (mpay                    *_o,
                         const char              *_order,
                         enum mpay_payment_state *_opt_state,
                         json_t                 **_opt_info,
                         json_t                 **_opt_history);
bool mpay_payment_refund(mpay         *_o,
                         const char   *_order,
                         json_t       *_info,
                         coin_t        _opt_different_amount,
                         json_t      **_opt_result);



struct escrow_target {
    const char *id;
    coin_t      amount;
};

struct mpay_form {
    enum mpay_operationType operationType;
    const char *language;
    const char *productDescription;
    struct {
        const char           *merchantDescription;
        enum mpay_method      methods[10];
        enum mpay_method      excludedMethods[10];
        const char           *order;
        coin_t                amount;
        int                   idUser;    /* User card id by paycomet, mandatory card payments. */
        const char           *tokenUser; /* User card id by paycomet, mandatory card payments. */
        int                   secure;
        const char           *scoring;   /* Risk skoring from 0 to 100. */
        int                   userInteraction; /* Business can interact with user? */
        struct escrow_target *escrow_targets;
        const char           *urlOk;
        const char           *urlKo;
    } payment;
    struct {
        time_t start_date;
        time_t end_date;
        int    periodicity;
    } subscription;
};

#endif
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
