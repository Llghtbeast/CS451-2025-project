#pragma once

typedef uint64_t proc_id_t;
typedef uint32_t msg_seq_t;

constexpr uint32_t MAX_MESSAGES_PER_PACKET = 8;
constexpr uint32_t SEND_WINDOW_SIZE = 8;
constexpr uint32_t SEND_TIMEOUT_MS = 5;
constexpr uint32_t MAX_QUEUE_SIZE = 1024;

constexpr int INITIAL_SLIDING_SET_PREFIX = 0; 