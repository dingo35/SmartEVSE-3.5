#ifndef HTTP_HANDLERS_H
#define HTTP_HANDLERS_H

#include "network_common.h"

bool handle_URI(struct mg_connection *c, struct mg_http_message *hm, webServerRequest *request);

#endif
