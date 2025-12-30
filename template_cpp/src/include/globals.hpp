#pragma once

typedef uint64_t proc_id_t;
typedef uint32_t pkt_seq_t;
typedef uint32_t proposal_t;
typedef uint32_t prop_nb_t;

constexpr uint32_t SEND_TIMEOUT_MS = 0;      // 5
constexpr uint32_t LOG_TIMEOUT = 2000;
constexpr uint32_t PROPOSAL_TIMEOUT_MS = 10; // TODO: reduce this

constexpr uint32_t MAX_MESSAGES_PER_PACKET = 8; // 8
constexpr uint32_t SEND_WINDOW_SIZE = 32;        // 8
constexpr uint32_t MAX_CONTAINER_SIZE =  MAX_MESSAGES_PER_PACKET * SEND_WINDOW_SIZE;
constexpr uint32_t BROADCAST_COOLDOWN_MS = 0;
constexpr uint32_t MAX_PROPOSAL_SET_SIZE = 1000;

constexpr int INITIAL_SLIDING_SET_PREFIX = 0; 