// Included after the production handlers by scripts/test_firmware_update.py.
void start_session() {
    g_server.args["size"] = "1000";
    handle_firmware_begin();
    assert(g_server.code == 200 && firmware_update_status().active);
    g_server.args["session"] = g_ota_session;
    g_server.args["offset"] = "0";
    g_server.args["size"] = "1000";
}
void event(int status, size_t size = 1000) {
    static uint8_t bytes[16385] = {0xE9};
    g_server.data.status = status;
    g_server.data.buf = bytes;
    g_server.data.currentSize = size;
    handle_firmware_chunk_upload();
}
int main(int argc,char **argv) {
    assert(argc == 2);
    const std::string test = argv[1];
    if (test == "legacy") {
        g_server.data.status = UPLOAD_FILE_START; handle_firmware_upload();
        uint8_t bytes[1000] = {0xE9};
        g_server.data.buf = bytes; g_server.data.currentSize = sizeof(bytes);
        g_server.data.status = UPLOAD_FILE_WRITE; handle_firmware_upload();
        g_server.data.status = UPLOAD_FILE_END; handle_firmware_upload();
        assert(!Update.activated);
        handle_firmware_complete();
        assert(Update.activated && g_reboot_at_ms && g_server.code == 200);
    } else {
        start_session();
        if (test == "unauthorized") g_server.authorized = false;
        if (test == "session") g_server.args["session"] = "wrong";
        if (test == "offset") g_server.args["offset"] = "50";
        if (test == "size") g_server.args["size"] = "16385";
        if (test == "content_length") g_server.contentLength = 8000;
        event(UPLOAD_FILE_START);
        if (test == "unauthorized" || test == "session" || test == "offset" || test == "size" || test == "content_length") {
            event(UPLOAD_FILE_WRITE); event(UPLOAD_FILE_END); handle_firmware_chunk_complete();
            assert(g_server.code >= 400 && Update.written == 0 && Update.aborts == 0);
            assert(firmware_update_status().active); // Foreign requests cannot cancel another session.
        } else if (test == "truncated") {
            event(UPLOAD_FILE_WRITE, 999); event(UPLOAD_FILE_END); handle_firmware_chunk_complete();
            assert(g_server.code == 400 && Update.aborts == 1 && !Update.activated);
        } else if (test == "multiple_parts") {
            event(UPLOAD_FILE_WRITE); event(UPLOAD_FILE_END); event(UPLOAD_FILE_START);
            handle_firmware_chunk_complete();
            assert(g_server.code == 400 && Update.aborts == 1 && !Update.activated);
        } else if (test == "disconnect_after_end") {
            event(UPLOAD_FILE_WRITE); event(UPLOAD_FILE_END); event(UPLOAD_FILE_ABORTED);
            assert(!Update.activated && Update.aborts == 1);
            handle_firmware_finish(); assert(!Update.activated && !g_reboot_at_ms);
        } else if (test == "success") {
            event(UPLOAD_FILE_WRITE); event(UPLOAD_FILE_END);
            assert(!Update.activated);
            handle_firmware_chunk_complete();
            assert(g_server.code == 200 && !Update.activated);
            handle_firmware_status(); assert(g_server.code == 200);
            handle_firmware_finish(); assert(g_server.code == 200 && Update.activated && g_reboot_at_ms);
            handle_firmware_abort(); assert(Update.aborts == 0);
            handle_firmware_finish(); assert(Update.ends == 1); // Lost final response is safe to retry.
        } else assert(false);
    }
    printf("OTA production handlers passed: %s\n",test.c_str());
}
