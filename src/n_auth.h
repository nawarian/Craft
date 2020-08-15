#ifndef _n_auth_h_
#define _n_auth_h_

int n_auth_access_token_get(
    char *result, int length, char *username, char *identity_token);

#endif
