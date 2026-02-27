#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>


/************** Enum class Generales *****************/
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

struct StackMessage {
    uint8_t ruleId;
    std::string deviceId;
    int len;
    std::vector<uint8_t> payload;
};

enum class EventType
{
    TimerExpired,
    StackMsgReceived,
    SCHCCoreMsgReceived,
    ExecuteAgain,
};

struct EventMessage {
    EventType evType;
    std::vector<uint8_t> payload;
};



/************** SCHC enum class *****************/
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

enum class SCHCAckMechanism{
    ACK_END_WIN,
    ACK_END_SES,
    ACK_COMPOUND,
    ARQ_FEC
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

enum class SCHCArqFecSenderStates{
    STATE_INIT,
    STATE_WAIT_X_S_ACK,
    STATE_SEND,
    STATE_WAIT_x_SESSION_ACK,
    STATE_RESEND_MISSING_TILES,
    STATE_ERROR,
    STATE_END,
};

enum class SCHCArqFecReceiverStates{
    STATE_INIT,
    STATE_WAIT_X_S,
    STATE_RCV_WINDOW,
    STATE_WAIT_X_MISSING_FRAG,
    STATE_WAIT_X_ALL1,
    STATE_ERROR,
    STATE_END,
};
enum class SCHCMsgType {
    SCHC_REGULAR_FRAGMENT_MSG,
    SCHC_ALL1_FRAGMENT_MSG,
    SCHC_ACK_MSG,
    SCHC_ACK_REQ_MSG,
    SCHC_SENDER_ABORT_MSG,
    SCHC_RECEIVER_ABORT_MSG,
    SCHC_COMPOUND_ACK,
};

enum class SCHCLoRaWANFragRule : uint8_t{
    SCHC_FRAG_UPDIR_RULE_ID = 20,
    SCHC_FRAG_DOWNDIR_RULE_ID = 21
};
