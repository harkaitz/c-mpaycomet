# NAME

mpay_create(), mpay_destroy(), mpay_set_auth(), mpay_chk_auth(),
mpay_heartbeat(), mpay_methods_get(), mpay_exchange(),
mpay_form_prepare(), mpay_form(), mpay_payment_info(),
mpay_payment_refund()

# SYNOPSIS

    #include <mpaycomet.h>
    
    typedef struct mpay mpay;
    
    /* Constructor/destructor. */
    bool mpay_create  (mpay **_o);
    void mpay_destroy (mpay  *_o);
    
    
    /* Authorization. */
    void mpay_set_auth(mpay *_o, const char *_api_token, const char *_terminal);
    bool mpay_chk_auth(mpay *_o, const char **_reason);
    
    
    /* Check it works. */
    bool mpay_heartbeat(mpay *_o, FILE *_fp1);
    
    
    /* Auxiliary methods. */
    bool mpay_methods_get(mpay *_o, json_t **_r);
    bool mpay_exchange(mpay *_o, coin_t _fr, coin_t *_to, const char *_currency);
    
    
    /* Forms. */
    bool mpay_form_prepare(struct mpay_form *_f,
                           enum mpay_operationType type,
                           char *_opts[]);
    bool mpay_form(mpay *_o, struct mpay_form *_form, char **_url_m);
    
    
    /* Check payments. */
    bool mpay_payment_info(mpay       *_o,
                           const char *_order,
                           enum mpay_payment_state *_opt_state,
                           json_t    **_opt_info,
                           json_t    **_opt_history);
    bool mpay_payment_refund(mpay         *_o,
                             const char   *_order,
                             json_t       *_info,
                             coin_t        _opt_different_amount,
                             json_t      **_opt_result);

# DESCRIPTION

Minimal PAYCOMET library.

# RETURN VALUE

True on success False on error.

# COLLABORATING

For making bug reports, feature requests and donations visit one of the
following links:

1. [gemini://harkadev.com/oss/](gemini://harkadev.com/oss/)
2. [https://harkadev.com/oss/](https://harkadev.com/oss/)

