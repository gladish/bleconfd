#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <getopt.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <lib/bluetooth/bluetooth.h>
#include <lib/bluetooth/hci.h>
#include <lib/bluetooth/hci_lib.h>
#include <lib/hci.h>

#include "beacon.h"
#include "xLog.h"
#include "appSettings.h"
#include "util.h"

using std::string;
using std::vector;

struct hci_dev_info di;


/**
 * Stop HCI device
 * @param ctl the device controller(fd)
 * @param hdev the device id
 */
void
cmdDown(int ctl, int hdev)
{
  if (ioctl(ctl, HCIDEVDOWN, hdev) < 0)
  {
    XLOG_ERROR("Can't down device hci%d: %s (%d)", hdev, strerror(errno), errno);
    exit(1);
  }
}

/**
 * Start HCI device
 * @param ctl the device controller(fd)
 * @param hdev the device id
 */
void
cmdUp(int ctl, int hdev)
{
  if (ioctl(ctl, HCIDEVUP, hdev) < 0)
  {
    if (errno == EALREADY)
      return;
    XLOG_ERROR("Can't init device hci%d: %s (%d)", hdev, strerror(errno), errno);
    exit(1);
  }
}

/**
 * send no lead v cmd to device
 * @param hdev the device id
 */
void
cmdNoleadv(int hdev)
{
  struct hci_request rq;
  le_set_advertise_enable_cp advertise_cp;
  uint8_t status;
  int dd, ret;

  if (hdev < 0)
    hdev = hci_get_route(NULL);

  dd = hci_open_dev(hdev);
  if (dd < 0)
  {
    XLOG_ERROR("Could not open device");
    exit(1);
  }

  memset(&advertise_cp, 0, sizeof(advertise_cp));

  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
  rq.cparam = &advertise_cp;
  rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  ret = hci_send_req(dd, &rq, 1000);

  hci_close_dev(dd);

  if (ret < 0)
  {
    XLOG_ERROR("Can't set advertise mode on hci%d: %s (%d)",
               hdev, strerror(errno), errno);
    exit(1);
  }

  if (status)
  {
    XLOG_INFO("LE set advertise enable on hci%d returned status %d", hdev, status);
  }
}

/**
 * send lead v to device
 * @param hdev  the device id
 */
void
cmdLeadv(int hdev)
{
  struct hci_request rq;
  le_set_advertise_enable_cp advertise_cp;
  le_set_advertising_parameters_cp adv_params_cp;
  uint8_t status;
  int dd, ret;

  if (hdev < 0)
    hdev = hci_get_route(NULL);

  dd = hci_open_dev(hdev);
  if (dd < 0)
  {
    XLOG_ERROR("Could not open device");
    exit(1);
  }

  memset(&adv_params_cp, 0, sizeof(adv_params_cp));
  adv_params_cp.min_interval = htobs(0x0800);
  adv_params_cp.max_interval = htobs(0x0800);
  adv_params_cp.chan_map = 7;

  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
  rq.cparam = &adv_params_cp;
  rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  ret = hci_send_req(dd, &rq, 1000);
  if (ret >= 0)
  {
    memset(&advertise_cp, 0, sizeof(advertise_cp));
    advertise_cp.enable = 0x01;

    memset(&rq, 0, sizeof(rq));
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    rq.cparam = &advertise_cp;
    rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
    rq.rparam = &status;
    rq.rlen = 1;
    ret = hci_send_req(dd, &rq, 1000);
  }

  hci_close_dev(dd);

  if (ret < 0)
  {
    XLOG_ERROR("Can't set advertise mode on hci%d: %s (%d)\n",
               hdev, strerror(errno), errno);
    exit(1);
  }

  if (status)
  {
    XLOG_ERROR(
        "LE set advertise enable on hci%d returned status %d\n",
        hdev, status);
    exit(1);
  }
}


/**
 * dump hex to console
 * @param width the hex width
 * @param buf the hex buffer
 * @param len the buffer length
 */
void
hex_dump(int width, unsigned char* buf, int len)
{
  char const* pref = "  ";
  register int i, n;
  for (i = 0, n = 1; i < len; i++, n++)
  {
    if (n == 1)
      printf("%s", pref);
    printf("%2.2X ", buf[i]);
    if (n == width)
    {
      printf("\n");
      n = 0;
    }
  }
  if (i && n != 1)
    printf("\n");
}

string
getDeviceNameFromFile()
{
  string str_buffer = runCommand("cat /etc/machine-info");
  if (str_buffer.find("PRETTY_HOSTNAME") != string::npos)
  {
    str_buffer = str_buffer.substr(0, str_buffer.length() - 1);
    return split(str_buffer, "=")[1];
  }
  return std::string();
}

/**
 * update device name, it need restart ble service
 * NOTE: this will run system command, and this need root permission
 * sudo sh -c "echo 'PRETTY_HOSTNAME=tc-device' > /etc/machine-info"
 * sudo service bluetooth restart
 * @param name the new device name
 */
void
updateDeviceName(string const& name)
{
  // device name is same as new name, skip it
  if (!name.compare(getDeviceNameFromFile()))
  {
    XLOG_INFO("current BLE name is %s", name.c_str());
    return;
  }
  char cmd_buffer[256];
  sprintf(cmd_buffer, "sh -c \"echo 'PRETTY_HOSTNAME=%s' > /etc/machine-info\"", name.c_str());
  runCommand(cmd_buffer);
  runCommand("service bluetooth restart");
  XLOG_INFO("BLE set new name = %s", getDeviceNameFromFile().c_str());
}

/**
 * send cmd to device by hci tool
 * @param dev_id  the device id
 * @param args the cmd args
 */
void
hcitoolCmd(int dev_id, vector <string> const& args)
{
  unsigned char buf[HCI_MAX_EVENT_SIZE], * ptr = buf;
  struct hci_filter flt;
  hci_event_hdr* hdr;
  int i, dd;
  int len;
  uint16_t ocf;
  uint8_t ogf;

  if (dev_id < 0)
  {
    dev_id = hci_get_route(NULL);
  }

  errno = 0;
  ogf = strtol(args[0].c_str(), NULL, 16);
  ocf = strtol(args[1].c_str(), NULL, 16);
  if (errno == ERANGE || (ogf > 0x3f) || (ocf > 0x3ff))
  {
    XLOG_ERROR("ogf must be in range (0x3f,0x3ff)");
    exit(1);
  }

  for (i = 2, len = 0; i < (int)args.size() && len < (int) sizeof(buf); i++, len++)
    *ptr++ = (uint8_t) strtol(args[i].c_str(), NULL, 16);

  dd = hci_open_dev(dev_id);
  if (dd < 0)
  {
    XLOG_ERROR("Device open failed");
    exit(1);
  }

  /* Setup filter */
  hci_filter_clear(&flt);
  hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
  hci_filter_all_events(&flt);
  if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0)
  {
    XLOG_ERROR("HCI filter setup failed");
    exit(1);
  }

  XLOG_INFO("< HCI Command: ogf 0x%02x, ocf 0x%04x, plen %d", ogf, ocf, len);
  hex_dump(20, buf, len);
  fflush(stdout);

  if (hci_send_cmd(dd, ogf, ocf, len, buf) < 0)
  {
    XLOG_ERROR("Send failed");
    exit(1);
  }

  len = read(dd, buf, sizeof(buf));
  if (len < 0)
  {
    XLOG_ERROR("Read failed");
    exit(1);
  }

  hdr = static_cast<hci_event_hdr*>((void*) (buf + 1));
  ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
  len -= (1 + HCI_EVENT_HDR_SIZE);

  XLOG_INFO("> HCI Event: 0x%02x plen %d", hdr->evt, hdr->plen);
  hex_dump(20, ptr, len);
  fflush(stdout);

  hci_close_dev(dd);
}

/**
 * parse hci tool string args to vector args
 * @param str the string args
 * @return  the vector args
 */
std::vector <string>
parseArgs(string str)
{
  string delimiter = " ";

  size_t pos = 0;
  string token;
  vector <string> args;

  while ((pos = str.find(delimiter)) != string::npos)
  {
    token = str.substr(0, pos);
    args.push_back(token);
    str.erase(0, pos + delimiter.length());
  }
  args.push_back(str);
  return args;
}

/**
 * let BLE discoverable
 */
void 
cmdScan(int ctl, int hdev, char const* opt)
{
	struct hci_dev_req dr;

	dr.dev_id  = hdev;
	dr.dev_opt = SCAN_DISABLED;
	if (!strcmp(opt, "iscan"))
		dr.dev_opt = SCAN_INQUIRY;
	else if (!strcmp(opt, "pscan"))
		dr.dev_opt = SCAN_PAGE;
	else if (!strcmp(opt, "piscan"))
		dr.dev_opt = SCAN_PAGE | SCAN_INQUIRY;

	if (ioctl(ctl, HCISETSCAN, (unsigned long) &dr) < 0) {
		XLOG_ERROR("Can't set scan mode on hci%d: %s (%d)", hdev, strerror(errno), errno);
		exit(1);
	}
}

void 
reinitializeBLE()
{
  int ctl = 0;
  if ((ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) < 0)
  {
    XLOG_ERROR("Can't open HCI socket.");
    exit(1);
  }
  char const* dev_index = appSettings_get_ble_value("ble_device_id");
  di.dev_id = atoi(dev_index);

  if (ioctl(ctl, HCIGETDEVINFO, (void*) &di))
  {
    XLOG_ERROR("Can't get device info");
    exit(1);
  }

  bdaddr_t any = {0}; // BDADDR_ANY
  if (hci_test_bit(HCI_RAW, &di.flags) &&
      !bacmp(&di.bdaddr, &any))
  {
    int dd = hci_open_dev(di.dev_id);
    hci_read_bd_addr(dd, &di.bdaddr, 1000);
    hci_close_dev(dd);
  }

  cmdDown(ctl, di.dev_id);
  cmdUp(ctl, di.dev_id);
  cmdScan(ctl, di.dev_id, "piscan");
  cmdNoleadv(di.dev_id);

  std::string startUpCmd01(appSettings_get_ble_value("ble_init_cmd01"));
  hcitoolCmd(di.dev_id, parseArgs(startUpCmd01));

  std::string startUpCmd02(appSettings_get_ble_value("ble_init_cmd02"));
  hcitoolCmd(di.dev_id, parseArgs(startUpCmd02));
  cmdLeadv(di.dev_id);
}

/**
 * start up beacon
 * @param s the expected device name
 */
void
startBeacon(std::string const& s)
{
  updateDeviceName(s.c_str());
}