#include "api/handlers.h"
#include "api/http_server.h"
#include "api/auth_middleware.h"
#include "common/log.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <event2/buffer.h>

void handler_user(struct evhttp_request *req, void *ctx) {
    (void)ctx;
    struct evbuffer *in = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in);
    char *body = malloc(len + 1);
    evbuffer_copyout(in, body, len); body[len] = '\0';
    cJSON *root = cJSON_Parse(body);
    const cJSON *action = cJSON_GetObjectItem(root, "action");
    if (action && strcmp(action->valuestring, "login") == 0) {
        const cJSON *user = cJSON_GetObjectItem(root, "username");
        const cJSON *pass = cJSON_GetObjectItem(root, "password");
        if (user && pass && strcmp(user->valuestring, "admin") == 0 &&
            strcmp(pass->valuestring, "admin123") == 0) {
            char token[256];
            auth_middleware_generate_token(user->valuestring, token, sizeof(token));
            cJSON *res = cJSON_CreateObject();
            cJSON_AddStringToObject(res, "token", token);
            cJSON_AddStringToObject(res, "role", "admin");
            char *txt = cJSON_PrintUnformatted(res);
            http_reply_json(req, 200, "OK", txt);
            free(txt); cJSON_Delete(res);
        } else {
            http_reply_json(req, 401, "Unauthorized", "{\"error\":\"invalid credentials\"}");
        }
    } else {
        http_reply_json(req, 400, "Bad Request", "{\"error\":\"unknown action\"}");
    }
    free(body); cJSON_Delete(root);
}
