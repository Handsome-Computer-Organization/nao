/**
 * @file Tools/ProcessFramework/TeamHandler.h
 * The file declares a class for the team communication between robots.
 * @author <A href="mailto:Thomas.Roefer@dfki.de">Thomas Röfer</A>
 * @author <A href="mailto:Ingmar.Schwarz@udo.edu">Ingmar Schwarz</A>
 * @author <A href="mailto:Aaron.Larisch@udo.edu">Aaron Larisch</A>
 */

#pragma once

#include "Tools/MessageQueue/MessageQueue.h"
#include "Tools/Network/UdpComm.h"
#include <string>
#include "Tools/Network/NTP.h"

#define MAX_NUM_OF_TC_MESSAGES 10 // 5 broadcasting players and substitute + space for delays
#define NDEVILS_TC_VERSION 1

struct NDevilsHeader
{
  unsigned char teamName[4];
  unsigned int version = NDEVILS_TC_VERSION;
  std::array<NTPData, MAX_NUM_PLAYERS> ntpResponses;
};

const int teamCommHeaderSize = sizeof(RoboCup::SPLStandardMessage) - SPL_STANDARD_MESSAGE_DATA_SIZE;
const int ndevilsHeaderSize = sizeof(NDevilsHeader);

// Wrapper class for SPL standard messages.
class TeamDataIn
{
public:
  TeamDataIn() { clear(); messages.reserve(MAX_NUM_OF_TC_MESSAGES); }
  std::vector<RoboCup::SPLStandardMessage> messages;
  void clear() { messages.clear(); }
  bool isEmpty() { return messages.empty(); }
  void handleAllMessages(MessageHandler& handler) { }
};

// Wrapper class for SPLStandardMessage and MessageQueue.
// Always send TeamCommHeader and set numOfDataBytes!
class TeamDataOut
{
public:
  TeamDataOut() { clear(); }
  RoboCup::SPLStandardMessage message;
  bool isEmpty() { return message.numOfDataBytes == 0; }
  void clear() { message.numOfDataBytes = 0; }
};

#define TEAM_COMM \
  TeamDataIn theTeamReceiver; \
  TeamDataOut theTeamSender; \
  TeamHandler theTeamHandler;

#define INIT_TEAM_COMM \
  theTeamHandler(theTeamReceiver, theTeamSender)

#ifdef TARGET_SIM
#define START_TEAM_COMM \
  theTeamHandler.startLocal(Global::getSettings().teamPort, (unsigned) Global::getSettings().playerNumber);
#else
#define START_TEAM_COMM \
  std::string bcastAddr = UdpComm::getWifiBroadcastAddress(); \
  theTeamHandler.start(Global::getSettings().teamPort, bcastAddr.c_str());
#endif

#define RECEIVE_TEAM_COMM \
  (void) theTeamHandler.receive()

#define SEND_TEAM_COMM \
  theTeamHandler.send()

/**
 * @class TeamHandler
 * A class for team communication by broadcasting.
 */
class TeamHandler
{
private:
  TeamDataIn& in; /**< Incoming debug data is stored here. */
  TeamDataOut& out; /**< Outgoing debug data is stored here. */
  int port = 0; /**< The UDP port this handler is listening to. */
  UdpComm socket; /**< The socket used to communicate. */
  unsigned localId = 0; /**< The id of a local team communication participant or 0 for normal udp communication. */

public:
  /**
   * Constructor.
   * @param in Incoming debug data is stored here.
   * @param out Outgoing debug data is stored here.
   */
  TeamHandler(TeamDataIn& in, TeamDataOut& out);

  /**
   * The method starts the actual communication for local communication.
   * @param port The UDP port this handler is listening to.
   * @param localId An identifier for a local robot
   */
  void startLocal(int port, unsigned localId);

  /**
   * The method starts the actual communication on the given port.
   * @param port The UDP port this handler is listening to.
   * @param subnet The subnet the handler is broadcasting to.
   */
  void start(int port, const char* subnet);

  /**
   * The method sends the outgoing message queue if possible.
   */
  void send();

  /**
   * The method receives packages if available.
   * @return The number of bytes received.
   */
  unsigned receive();

  /**
   * Checks if a received message is a valid SPLStandardMessage.
   * @return True, if message is valid.
  */
  bool checkMessage(RoboCup::SPLStandardMessage& msg, const unsigned remoteIp, const unsigned realNumOfDataBytes);
};
