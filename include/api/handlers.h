#ifndef E2_HANDLERS_H
#define E2_HANDLERS_H
#include <event2/http.h>
#ifdef __cplusplus
extern "C" {
#endif
void handler_device(struct evhttp_request *req, void *ctx);
void handler_shadow(struct evhttp_request *req, void *ctx);
void handler_command(struct evhttp_request *req, void *ctx);
void handler_alarm(struct evhttp_request *req, void *ctx);
void handler_user(struct evhttp_request *req, void *ctx);
void handler_onenet(struct evhttp_request *req, void *ctx);
void handler_group(struct evhttp_request *req, void *ctx);
void handler_product(struct evhttp_request *req, void *ctx);
#ifdef __cplusplus
}
#endif
#endif
