/*
 * Minimal ble_l2cap.h stub for S145.
 * S145 does not expose an L2CAP API (neither v9.0.0 nor v10.0.1 ships this
 * header), but the vendored DFU transport code references BLE_L2CAP_MTU_DEF
 * for buffer sizing. Lives in src/ because it belongs to this port rather
 * than to any SoftDevice release; src/ precedes the SoftDevice API directory
 * on the include path.
 */

#ifndef BLE_L2CAP_H__
#define BLE_L2CAP_H__

/* Default ATT MTU minus 3-byte ATT header = 20 bytes payload.
 * This matches the legacy SoftDevice default. */
#ifndef BLE_L2CAP_MTU_DEF
#define BLE_L2CAP_MTU_DEF  23
#endif

#endif /* BLE_L2CAP_H__ */
