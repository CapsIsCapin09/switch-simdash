#pragma once

// Opens a TCP connection with a timeout. On failure returns -1 and puts a
// short, human readable reason in err.
int tcp_connect(const char *host, int port, int timeout_ms, char *err, int err_size);

// The Switch's own IP address as text, or "not connected" if it has none.
void local_ip(char *out, int size);
