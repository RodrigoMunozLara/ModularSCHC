#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

enum class CoreId {
    BACKHAUL,
    SCHC
};

enum class ProtocolType {
    LORAWAN,
    NB_IOT,
    SIGFOX,
    MYRIOTA,
    LORAWAN_NS
};

struct RoutedMetadata {
    CoreId ingress;
    std::size_t payloadSize;
};

struct RoutedMessage {
    RoutedMetadata meta;
    std::vector<uint8_t> payload;
};

enum class SCHCFragDir {
    UPLINK_DIR,
    DOWNLINK_DIR
};

enum class SCHCFragMode {
    ACK_ON_ERROR,
    ACK_ALWAYS,
    NO_ACK,
    ARQ_FEC
};

enum class SCHCStates {
    STATE_TX_INIT

};

enum class SCHCAckMechanism{
    ACK_END_WIN,
    ACK_END_SES,
    ACK_COMPOUND
};

enum class SCHCAckOnErrorSenderStates{
    STATE_INIT,
    STATE_SEND,
    STATE_WAIT_x_ACK,
    STATE_RESEND_MISSING_FRAG,
    STATE_ERROR,
    STATE_END,
};

enum class SCHCAckOnErrorReceiverStates{
    STATE_INIT,
    STATE_RCV_WINDOW,
    STATE_WAIT_X_MISSING_FRAG,
    STATE_WAIT_END,
    STATE_ERROR,
    STATE_END,
};
