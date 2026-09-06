/*****
Copyright @ 2017-2024, Hang Zhou Xiao Kong Cheng Xiang Ke Ji Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef DFACE_license_MANAGER_H
#define DFACE_license_MANAGER_H
#include "common.h"
#include <stdint.h>
#include <stdio.h>

/**
* @brief license API
*/
class API_EXPORTS LicenseManager {
public:
  /**
  * Constructor
  */
  LicenseManager();

  /**
  * Destructor
  */
  virtual ~LicenseManager();

  /**
  * license tool login
  * @return 0:success other:Error Code
  */
  virtual int login();

  /**
  * Online license authorize
  * @param[in] authCode license authorize code
  * @return 0:success, other:Error Code
  */
  virtual int updateOnline(char *authCode);

  /**
  * OffLine license authorize
  * @param[in] acFilePath license authorize file path
  * @return 0:success, other:Error Code
  */
  virtual int updateOfline(char *acFilePath);

  /**
  * license tool logout
  * @return 0:success, other:Error Code
  */
  virtual int logout();

  /**
  * Get device fingerPrint information
  * @param[in] authCode Activation code
  * @param[out] fingerPrintInfo Output fingerPrint information
  * @param[out] fingerPrintSize Output fingerPrint information size
  * @return 0:success, other:Error Code
  */
  virtual int getFingerPrint(const char *authCode, char *fingerPrintInfo,
                             unsigned int *fingerPrintSize);

  /**
  * Set environment root path
  * @param[in] path Root path
  * @return 0:success, other:Error Code
  */
  virtual int setRootPath(char *path);

  /**
  * license tool version
  * @return: version number
  */
  virtual int getVersion();

  /**
  * Remove license
  * @param[in] license authCode code
  * @return 0:success, other:Error Code
  */
  virtual int remove(const char *authCode);

  /**
   * Set group authorize server IP:port
   * @param hostName ip
   * @param port port
   * @param timeoutSeconds timeout sseconds
   * @return 0:success, other:Error Code
   */
  virtual int setLocalServer(const char *hostName, int port,
                             int timeoutSeconds);

  /**
   * Set Proxy server IP:port
   * @param hostName ip
   * @param port port
   * @param userId Proxy user id
   * @param password Proxy user password
   * @return 0:success, other:Error Code
   */
  virtual int setProxy(const char *hostName, int port, const char *userId,
                       const char *password);

  /**
  * Revoke license online
  * @param[in] authCode Activation code
  * @return 0:success, other:Error Code
  */
  virtual int revokeOnline(const char *authCode);

  /**
  * Revoke license offline
  * @param[in] authCode Activation code
  * @param[out] revocationInfo out revoke request information
  * @param[out] revocationInfoSize out revoke request information size
  * @return 0:success, other:Error Code
  */
  virtual int revokeOffline(const char *authCode, char *revocationInfo,
                            unsigned int *revocationInfoSize);

  /**
   * get license information
   * @param[in] authCode
   * @param[in] type @see license_INFO_TYPE enum(INFO_SN, INFO_SN_FEATURE,
   * INFO_SN_license)
   * @param[out] pInfo, information, xml format
   * @param[out] pInfoSize, information byte size
   * @return 0:success, other:Error Code
   */
  virtual int getInfo(const char *authCode, int type, char *pInfo,
                      unsigned int *pInfoSize);

private:
  void *mc;
};

#endif // DFACE_ACCREDIT_H
