/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From extensions/dev/ppb_ext_socket_dev.idl,
 *   modified Tue May 21 16:00:11 2013.
 */

#ifndef PPAPI_C_EXTENSIONS_DEV_PPB_EXT_SOCKET_DEV_H_
#define PPAPI_C_EXTENSIONS_DEV_PPB_EXT_SOCKET_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_EXT_SOCKET_DEV_INTERFACE_0_1 "PPB_Ext_Socket(Dev);0.1"
#define PPB_EXT_SOCKET_DEV_INTERFACE_0_2 "PPB_Ext_Socket(Dev);0.2"
#define PPB_EXT_SOCKET_DEV_INTERFACE PPB_EXT_SOCKET_DEV_INTERFACE_0_2

/**
 * @file
 * This file defines the Pepper equivalent of the <code>chrome.socket</code>
 * extension API.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * A string <code>PP_Var</code> which has one of the following values:
 * - "tcp"
 * - "udp"
 */
typedef struct PP_Var PP_Ext_Socket_SocketType_Dev;

/**
 * A dictionary <code>PP_Var</code>.
 */
typedef struct PP_Var PP_Ext_Socket_CreateOptions_Dev;

/**
 * A dictionary <code>PP_Var</code> which contains
 * - "socketId" : integer <code>PP_Var</code>
 * The id of the newly created socket.
 */
typedef struct PP_Var PP_Ext_Socket_CreateInfo_Dev;

/**
 * A dictionary <code>PP_Var</code> which contains
 * - "resultCode" : integer <code>PP_Var</code>
 * - "socketId" : integer or undefined <code>PP_Var</code>
 * The id of the accepted socket.
 */
typedef struct PP_Var PP_Ext_Socket_AcceptInfo_Dev;

/**
 * A dictionary <code>PP_Var</code> which contains
 * - "resultCode" : integer <code>PP_Var</code>
 * The resultCode returned from the underlying read() call.
 * - "data" : array buffer <code>PP_Var</code>
 */
typedef struct PP_Var PP_Ext_Socket_ReadInfo_Dev;

/**
 * A dictionary <code>PP_Var</code> which contains
 * - "bytesWritten" : integer <code>PP_Var</code>
 * The number of bytes sent, or a negative error code.
 */
typedef struct PP_Var PP_Ext_Socket_WriteInfo_Dev;

/**
 * A dictionary <code>PP_Var</code> which contains
 * - "resultCode" : integer <code>PP_Var</code>
 * The resultCode returned from the underlying recvfrom() call.
 * - "data": array buffer <code>PP_Var</code>
 * - "address": string <code>PP_Var</code>
 * The address of the remote machine.
 * - "port": integer <code>PP_Var</code>
 */
typedef struct PP_Var PP_Ext_Socket_RecvFromInfo_Dev;

/**
 * A dictionary <code>PP_Var</code> which contains
 * - "socketType" : string <code>PP_Var</code> which matches the description of
 * <code>PP_Ext_Socket_SocketType_Dev</code>
 * The type of the passed socket. This will be <code>tcp</code> or
 * <code>udp</code>.
 * - "connected" : boolean <code>PP_Var</code>
 * Whether or not the underlying socket is connected.
 *
 * For <code>tcp</code> sockets, this will remain true even if the remote peer
 * has disconnected. Reading or writing to the socket may then result in an
 * error, hinting that this socket should be disconnected via
 * <code>Disconnect()</code>.
 *
 * For <code>udp</code> sockets, this just represents whether a default remote
 * address has been specified for reading and writing packets.
 * - "peerAddress" : string or undefined <code>PP_Var</code>
 * If the underlying socket is connected, contains the IPv4/6 address of the
 * peer.
 * - "peerPort" : integer or undefined <code>PP_Var</code>
 * If the underlying socket is connected, contains the port of the connected
 * peer.
 * - "localAddress" : string or undefined <code>PP_Var</code>
 * If the underlying socket is bound or connected, contains its local IPv4/6
 * address.
 * - "localPort" : integer or undefined <code>PP_Var</code>
 * If the underlying socket is bound or connected, contains its local port.
 */
typedef struct PP_Var PP_Ext_Socket_SocketInfo_Dev;

/**
 * A dictionary <code>PP_Var</code> which contains
 * - "name" : string <code>PP_Var</code>
 * The underlying name of the adapter. On *nix, this will typically be "eth0",
 * "lo", etc.
 * - "address": string <code>PP_Var</code>
 * The available IPv4/6 address.
 */
typedef struct PP_Var PP_Ext_Socket_NetworkInterface_Dev;

/**
 * An array <code>PP_Var</code> which contains elements of
 * <code>PP_Ext_Socket_NetworkInterface_Dev</code>.
 */
typedef struct PP_Var PP_Ext_Socket_NetworkInterface_Dev_Array;
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Ext_Socket_Dev_0_2 {
  /**
   * Creates a socket of the specified type that will connect to the specified
   * remote machine.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] type A <code>PP_Ext_Socket_SocketType_Dev</code>. The type of
   * socket to create. Must be <code>tcp</code> or <code>udp</code>.
   * @param[in] options An undefined <code>PP_Var</code> or
   * <code>PP_Ext_Socket_CreateOptions_Dev</code>. The socket options.
   * @param[out] create_info A <code>PP_Ext_Socket_CreateInfo_Dev</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*Create)(PP_Instance instance,
                    PP_Ext_Socket_SocketType_Dev type,
                    PP_Ext_Socket_CreateOptions_Dev options,
                    PP_Ext_Socket_CreateInfo_Dev* create_info,
                    struct PP_CompletionCallback callback);
  /**
   * Destroys the socket. Each socket created should be destroyed after use.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   */
  void (*Destroy)(PP_Instance instance, struct PP_Var socket_id);
  /**
   * Connects the socket to the remote machine (for a <code>tcp</code> socket).
   * For a <code>udp</code> socket, this sets the default address which packets
   * are sent to and read from for <code>Read()</code> and <code>Write()</code>
   * calls.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] hostname A string <code>PP_Var</code>. The hostname or IP
   * address of the remote machine.
   * @param[in] port An integer <code>PP_Var</code>. The port of the remote
   * machine.
   * @param[out] result An integer <code>PP_Var</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*Connect)(PP_Instance instance,
                     struct PP_Var socket_id,
                     struct PP_Var hostname,
                     struct PP_Var port,
                     struct PP_Var* result,
                     struct PP_CompletionCallback callback);
  /**
   * Binds the local address for socket. Currently, it does not support TCP
   * socket.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] address A string <code>PP_Var</code>. The address of the local
   * machine.
   * @param[in] port An integer <code>PP_Var</code>. The port of the local
   * machine.
   * @param[out] result An integer <code>PP_Var</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*Bind)(PP_Instance instance,
                  struct PP_Var socket_id,
                  struct PP_Var address,
                  struct PP_Var port,
                  struct PP_Var* result,
                  struct PP_CompletionCallback callback);
  /**
   * Disconnects the socket. For UDP sockets, <code>Disconnect</code> is a
   * non-operation but is safe to call.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   */
  void (*Disconnect)(PP_Instance instance, struct PP_Var socket_id);
  /**
   * Reads data from the given connected socket.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] buffer_size An undefined or integer <code>PP_Var</code>. The
   * read buffer size.
   * @param[out] read_info A <code>PP_Ext_Socket_ReadInfo_Dev</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*Read)(PP_Instance instance,
                  struct PP_Var socket_id,
                  struct PP_Var buffer_size,
                  PP_Ext_Socket_ReadInfo_Dev* read_info,
                  struct PP_CompletionCallback callback);
  /**
   * Writes data on the given connected socket.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] data An array buffer <code>PP_Var</code>. The data to write.
   * @param[out] write_info A <code>PP_Ext_Socket_WriteInfo_Dev</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*Write)(PP_Instance instance,
                   struct PP_Var socket_id,
                   struct PP_Var data,
                   PP_Ext_Socket_WriteInfo_Dev* write_info,
                   struct PP_CompletionCallback callback);
  /**
   * Receives data from the given UDP socket.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] buffer_size An undefined or integer <code>PP_Var</code>. The
   * receive buffer size.
   * @param[out] recv_from_info A <code>PP_Ext_Socket_RecvFromInfo_Dev</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*RecvFrom)(PP_Instance instance,
                      struct PP_Var socket_id,
                      struct PP_Var buffer_size,
                      PP_Ext_Socket_RecvFromInfo_Dev* recv_from_info,
                      struct PP_CompletionCallback callback);
  /**
   * Sends data on the given UDP socket to the given address and port.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] data An array buffer <code>PP_Var</code>.
   * @param[in] address A string <code>PP_Var</code>. The address of the remote
   * machine.
   * @param[in] port An integer <code>PP_Var</code>. The port of the remote
   * machine.
   * @param[out] write_info A <code>PP_Ext_Socket_WriteInfo_Dev</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*SendTo)(PP_Instance instance,
                    struct PP_Var socket_id,
                    struct PP_Var data,
                    struct PP_Var address,
                    struct PP_Var port,
                    PP_Ext_Socket_WriteInfo_Dev* write_info,
                    struct PP_CompletionCallback callback);
  /**
   * This method applies to TCP sockets only.
   * Listens for connections on the specified port and address. This effectively
   * makes this a server socket, and client socket functions (Connect, Read,
   * Write) can no longer be used on this socket.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] address A string <code>PP_Var</code>. The address of the local
   * machine.
   * @param[in] port An integer <code>PP_Var</code>. The port of the local
   * machine.
   * @param[in] backlog An undefined or integer <code>PP_Var</code>. Length of
   * the socket's listen queue.
   * @param[out] result An integer <code>PP_Var</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*Listen)(PP_Instance instance,
                    struct PP_Var socket_id,
                    struct PP_Var address,
                    struct PP_Var port,
                    struct PP_Var backlog,
                    struct PP_Var* result,
                    struct PP_CompletionCallback callback);
  /**
   * This method applies to TCP sockets only.
   * Registers a callback function to be called when a connection is accepted on
   * this listening server socket. Listen must be called first.
   * If there is already an active accept callback, this callback will be
   * invoked immediately with an error as the resultCode.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[out] accept_info A <code>PP_Ext_Socket_AcceptInfo_Dev</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*Accept)(PP_Instance instance,
                    struct PP_Var socket_id,
                    PP_Ext_Socket_AcceptInfo_Dev* accept_info,
                    struct PP_CompletionCallback callback);
  /**
   * Enables or disables the keep-alive functionality for a TCP connection.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] enable A boolean <code>PP_Var</code>. If true, enable keep-alive
   * functionality.
   * @param[in] delay An undefined or integer <code>PP_Var</code>. Set the delay
   * seconds between the last data packet received and the first keepalive
   * probe. Default is 0.
   * @param[out] result A boolean <code>PP_Var</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*SetKeepAlive)(PP_Instance instance,
                          struct PP_Var socket_id,
                          struct PP_Var enable,
                          struct PP_Var delay,
                          struct PP_Var* result,
                          struct PP_CompletionCallback callback);
  /**
   * Sets or clears <code>TCP_NODELAY</code> for a TCP connection. Nagle's
   * algorithm will be disabled when <code>TCP_NODELAY</code> is set.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] no_delay A boolean <code>PP_Var</code>.
   * @param[out] result A boolean <code>PP_Var</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*SetNoDelay)(PP_Instance instance,
                        struct PP_Var socket_id,
                        struct PP_Var no_delay,
                        struct PP_Var* result,
                        struct PP_CompletionCallback callback);
  /**
   * Retrieves the state of the given socket.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[out] result A <code>PP_Ext_Socket_SocketInfo_Dev</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*GetInfo)(PP_Instance instance,
                     struct PP_Var socket_id,
                     PP_Ext_Socket_SocketInfo_Dev* result,
                     struct PP_CompletionCallback callback);
  /**
   * Retrieves information about local adapters on this system.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[out] result A <code>PP_Ext_Socket_NetworkInterface_Dev_Array</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*GetNetworkList)(PP_Instance instance,
                            PP_Ext_Socket_NetworkInterface_Dev_Array* result,
                            struct PP_CompletionCallback callback);
  /**
   * Joins the multicast group and starts to receive packets from that group.
   * The socket must be of UDP type and must be bound to a local port before
   * calling this method.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] address A string <code>PP_Var</code>. The group address to join.
   * Domain names are not supported.
   * @param[out] result An integer <code>PP_Var</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*JoinGroup)(PP_Instance instance,
                       struct PP_Var socket_id,
                       struct PP_Var address,
                       struct PP_Var* result,
                       struct PP_CompletionCallback callback);
  /**
   * Leaves the multicast group previously joined using <code>JoinGroup</code>.
   * It's not necessary to leave the multicast group before destroying the
   * socket or exiting. This is automatically called by the OS.
   *
   * Leaving the group will prevent the router from sending multicast datagrams
   * to the local host, presuming no other process on the host is still joined
   * to the group.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] address A string <code>PP_Var</code>. The group address to
   * leave. Domain names are not supported.
   * @param[out] result An integer <code>PP_Var</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*LeaveGroup)(PP_Instance instance,
                        struct PP_Var socket_id,
                        struct PP_Var address,
                        struct PP_Var* result,
                        struct PP_CompletionCallback callback);
  /**
   * Sets the time-to-live of multicast packets sent to the multicast group.
   *
   * Calling this method does not require multicast permissions.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] ttl An integer <code>PP_Var</code>. The time-to-live value.
   * @param[out] result An integer <code>PP_Var</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*SetMulticastTimeToLive)(PP_Instance instance,
                                    struct PP_Var socket_id,
                                    struct PP_Var ttl,
                                    struct PP_Var* result,
                                    struct PP_CompletionCallback callback);
  /**
   * Sets whether multicast packets sent from the host to the multicast group
   * will be looped back to the host.
   *
   * Note: the behavior of <code>SetMulticastLoopbackMode</code> is slightly
   * different between Windows and Unix-like systems. The inconsistency
   * happens only when there is more than one application on the same host
   * joined to the same multicast group while having different settings on
   * multicast loopback mode. On Windows, the applications with loopback off
   * will not RECEIVE the loopback packets; while on Unix-like systems, the
   * applications with loopback off will not SEND the loopback packets to
   * other applications on the same host. See MSDN: http://goo.gl/6vqbj
   *
   * Calling this method does not require multicast permissions.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[in] enabled A boolean <code>PP_Var</code>. Indicates whether to
   * enable loopback mode.
   * @param[out] result An integer <code>PP_Var</code>.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*SetMulticastLoopbackMode)(PP_Instance instance,
                                      struct PP_Var socket_id,
                                      struct PP_Var enabled,
                                      struct PP_Var* result,
                                      struct PP_CompletionCallback callback);
  /**
   * Gets the multicast group addresses the socket is currently joined to.
   *
   * @param[in] instance A <code>PP_Instance</code>.
   * @param[in] socket_id An integer <code>PP_Var</code>. The socket ID.
   * @param[out] groups An array <code>PP_Var</code> of string
   * <code>PP_Var</code>s.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called
   * upon completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*GetJoinedGroups)(PP_Instance instance,
                             struct PP_Var socket_id,
                             struct PP_Var* groups,
                             struct PP_CompletionCallback callback);
};

typedef struct PPB_Ext_Socket_Dev_0_2 PPB_Ext_Socket_Dev;

struct PPB_Ext_Socket_Dev_0_1 {
  int32_t (*Create)(PP_Instance instance,
                    PP_Ext_Socket_SocketType_Dev type,
                    PP_Ext_Socket_CreateOptions_Dev options,
                    PP_Ext_Socket_CreateInfo_Dev* create_info,
                    struct PP_CompletionCallback callback);
  void (*Destroy)(PP_Instance instance, struct PP_Var socket_id);
  int32_t (*Connect)(PP_Instance instance,
                     struct PP_Var socket_id,
                     struct PP_Var hostname,
                     struct PP_Var port,
                     struct PP_Var* result,
                     struct PP_CompletionCallback callback);
  int32_t (*Bind)(PP_Instance instance,
                  struct PP_Var socket_id,
                  struct PP_Var address,
                  struct PP_Var port,
                  struct PP_Var* result,
                  struct PP_CompletionCallback callback);
  void (*Disconnect)(PP_Instance instance, struct PP_Var socket_id);
  int32_t (*Read)(PP_Instance instance,
                  struct PP_Var socket_id,
                  struct PP_Var buffer_size,
                  PP_Ext_Socket_ReadInfo_Dev* read_info,
                  struct PP_CompletionCallback callback);
  int32_t (*Write)(PP_Instance instance,
                   struct PP_Var socket_id,
                   struct PP_Var data,
                   PP_Ext_Socket_WriteInfo_Dev* write_info,
                   struct PP_CompletionCallback callback);
  int32_t (*RecvFrom)(PP_Instance instance,
                      struct PP_Var socket_id,
                      struct PP_Var buffer_size,
                      PP_Ext_Socket_RecvFromInfo_Dev* recv_from_info,
                      struct PP_CompletionCallback callback);
  int32_t (*SendTo)(PP_Instance instance,
                    struct PP_Var socket_id,
                    struct PP_Var data,
                    struct PP_Var address,
                    struct PP_Var port,
                    PP_Ext_Socket_WriteInfo_Dev* write_info,
                    struct PP_CompletionCallback callback);
  int32_t (*Listen)(PP_Instance instance,
                    struct PP_Var socket_id,
                    struct PP_Var address,
                    struct PP_Var port,
                    struct PP_Var backlog,
                    struct PP_Var* result,
                    struct PP_CompletionCallback callback);
  int32_t (*Accept)(PP_Instance instance,
                    struct PP_Var socket_id,
                    PP_Ext_Socket_AcceptInfo_Dev* accept_info,
                    struct PP_CompletionCallback callback);
  int32_t (*SetKeepAlive)(PP_Instance instance,
                          struct PP_Var socket_id,
                          struct PP_Var enable,
                          struct PP_Var delay,
                          struct PP_Var* result,
                          struct PP_CompletionCallback callback);
  int32_t (*SetNoDelay)(PP_Instance instance,
                        struct PP_Var socket_id,
                        struct PP_Var no_delay,
                        struct PP_Var* result,
                        struct PP_CompletionCallback callback);
  int32_t (*GetInfo)(PP_Instance instance,
                     struct PP_Var socket_id,
                     PP_Ext_Socket_SocketInfo_Dev* result,
                     struct PP_CompletionCallback callback);
  int32_t (*GetNetworkList)(PP_Instance instance,
                            PP_Ext_Socket_NetworkInterface_Dev_Array* result,
                            struct PP_CompletionCallback callback);
};
/**
 * @}
 */

#endif  /* PPAPI_C_EXTENSIONS_DEV_PPB_EXT_SOCKET_DEV_H_ */

