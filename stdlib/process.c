#include "stdlib.h"

void spawnProcess(TKStreamPointer *out, char *path, char *filename) {
    int path_length;
    int filename_length;
    TKMsgSpawnProcess *req;

    path_length = strnlen(path, 1024);
    filename_length = strnlen(filename, 1024);

    req = (TKMsgSpawnProcess*)streamCreateMsg(out, ID_SPAWN_PROCESS, sizeof(TKMsgSpawnProcess) + path_length + filename_length + 2);
    req->filename_offset = path_length + 1;
    memcpy(req->path_and_filename, path, path_length + 1);
    memcpy(&req->path_and_filename[path_length + 1], filename, filename_length + 1);
}

void openStream(TKStreamPointer *out, const char *uri, int request_num) {
    int uri_length;
    TKMsgOpenStream *req;

    uri_length = strnlen(uri, 1024);

    req = (TKMsgOpenStream*)streamCreateMsg(out, ID_OPEN_STREAM, sizeof(TKMsgOpenStream) + uri_length + 1);
    memcpy(req->uri, uri, uri_length + 1);
    req->request_num = request_num;
}
