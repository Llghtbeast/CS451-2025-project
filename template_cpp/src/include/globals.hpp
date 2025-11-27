#pragma once

typedef uint64_t proc_id_t;
typedef uint32_t msg_seq_t;

constexpr int MAX_MESSAGES_PER_PACKET = 8;
constexpr int SEND_WINDOW_SIZE = 8;
constexpr int SEND_TIMEOUT_MS = 5;