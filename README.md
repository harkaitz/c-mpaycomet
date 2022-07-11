# MPAYCOMET

Small C library to interface with paycomet.

You can read the manpage [here](mpaycomet.3.md).

## Implemented features and state.

|------------------------------|------------------------------------------------------------------|
| BALANCE                      |                                                                  |
|------------------------------|------------------------------------------------------------------|
| productBalance               | 1031: Insufficient Rights.                                       |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| CARDS                        |                                                                  |
|------------------------------|------------------------------------------------------------------|
| addUser                      | Certification required. Uninteresting.                           |
| editUser                     |                                                                  |
| infoUser                     |                                                                  |
| physicalAddCard              |                                                                  |
| removeUser                   |                                                                  |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| DCC                          | Dynamic currency conversion (DCC)                                |
|------------------------------|------------------------------------------------------------------|
| dccPurchaseConfirm           | Interesting, Requires user token.                                |
| dccPurchaseCreate            |                                                                  |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| ERRORS & INFO                |                                                                  |
|------------------------------|------------------------------------------------------------------|
| infoError                    | Not needed really.                                               |
| heartbeat                    | [mpay_heartbeat()]                                               |
| getUserPaymentMethods        | mpay_methods_get(): Get supported payment methods.               |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| CURRENCY CONVERSION          |                                                                  |
|------------------------------|------------------------------------------------------------------|
| exchange                     | [mpay_exchange()]                                                |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| FORM                         |                                                                  |
|------------------------------|------------------------------------------------------------------|
| form                         | [mpay_form_prepare(), mpay_form()]                               |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| IVR & MIRAKL                 |                                                                  |
|------------------------------|------------------------------------------------------------------|
| checkSession                 | (IVR) Unimplemented, uninsteresting, Phone call centers.         |
| getSession                   | (IVR)                                                            |
| sessionCancel                | (IVR)                                                            |
| miraklInvoicesSearch         | (Mirakl) Unimplemented, (mirakl invoices need a "shop id").      |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| IP                           |                                                                  |
|------------------------------|------------------------------------------------------------------|
| getCountrybyIP               | Unimplemented, interesting.                                      |
| getRemoteAddress             |                                                                  |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| LAUNCHPAD                    |                                                                  |
|------------------------------|------------------------------------------------------------------|
| launchAuthorization          | Unimplemented, uninsteresting. Requires client's IP              |
| launchPreauthorization       | and somehow contacts with it.                                    |
| launchSubscription           |                                                                  |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| MARKETPLACE                  |                                                                  |
|------------------------------|------------------------------------------------------------------|
| splitTransfer                | Unimplemented, interesting. Transfers to other paycomet accounts |
| splitTransferReversal        |                                                                  |
| transfer                     |                                                                  |
| transferReversal             |                                                                  |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| PAYMENTS                     |                                                                  |
|------------------------------|------------------------------------------------------------------|
| executePurchase              | Unimplemented.                                                   |
| executePurchaseRtoken        | Unimplemented.                                                   |
| operationInfo                | mpay_payment_info() : Get form info.                             |
| operationSearch              | TODO.                                                            |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| PREAUTHORIZATIONS            |                                                                  |
|------------------------------|------------------------------------------------------------------|
| cancelPreauthorization       | Not known, unimplemented, maybe interesting.                     |
| confirmPreauthorization      |                                                                  |
| createPreauthorization       |                                                                  |
| createPreauthorizationRtoken |                                                                  |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| REFUND                       |                                                                  |
|------------------------------|------------------------------------------------------------------|
| executeRefund                | [mpay_payment_refund()]                                          |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| SEPA                         |                                                                  |
|------------------------------|------------------------------------------------------------------|
| addDocument                  | Not known, unimplemented, maybe interesting.                     |
| checkCustomer                |                                                                  |
| checkDocument                |                                                                  |
| sepaOperations               |                                                                  |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
| SUBSCRIPTIONS                |                                                                  |
|------------------------------|------------------------------------------------------------------|
| createSubscription           | Discarded, the REST API is buggy.                                |
| editSubscription             |                                                                  |
| removeSubscription           |                                                                  |
|                              |                                                                  |
|------------------------------|------------------------------------------------------------------|
