#include "../SDL_ble_c.h"
#include "../../core/windows/SDL_windows.h"
#include <errno.h>
#include <SDL_log.h>
#include "SDL_assert.h"

typedef enum {device_rdpd, device_lamp} device_type_t;

// static const device_type_t device_type = device_rdpd;
static const device_type_t device_type = device_lamp;

#define PROPERTY_INDICATE	32
#define PROPERTY_NOTIFY		16
#define PROPERTY_READ		2
#define PROPERTY_WRITE		8

#define FULL_UUID_SIZE	36

typedef struct {
	char uuid[40];
	uint32_t properties;
} tchar_desc;

#define MAX_SERVICES			6
#define MAX_CHARS_PER_SERVICE	6
typedef struct {
	char uuid[40];
	tchar_desc chars[MAX_CHARS_PER_SERVICE];
} tservice_desc;

typedef void (SDLCALL* fdid_characteristicwrite)();

typedef struct {
	uint8_t address[SDL_BLE_MAC_ADDR_BYTES * 3];
	char name[32];
	char service_uuid[FULL_UUID_SIZE + 1];
	uint8_t manufacturer_data[32];
	int manufacturer_data_len;

	tservice_desc services[MAX_SERVICES];
	fdid_characteristicwrite fn_did_characteristicwrite;
} tdevice_data;

typedef struct {
	int data1;
	int data2;
} peripheral_context;


#define MAX_BLEDID_DATA	128
typedef struct {
	uint32_t type;
	char service_uuid[FULL_UUID_SIZE + 1];
	char chara_uuid[FULL_UUID_SIZE + 1];
	uint8_t data[MAX_BLEDID_DATA];
	int len;
	SDL_bool notify;
} DidCharacteristicRead;

typedef struct {
	uint32_t type;
	char service_uuid[FULL_UUID_SIZE + 1];
	char chara_uuid[FULL_UUID_SIZE + 1];
	uint8_t data[MAX_BLEDID_DATA];
	int len;
	SDL_bool success;
} DidCharacteristicWrite;

typedef union {
	uint32_t type;
	// DidDiscoverPeripheral DiscoverPeripheral;
	// DidConnectionStateChange ConnectionStateChange;
	// DidServicesDiscovered ServicesDiscovered;
	DidCharacteristicRead CharacteristicRead;
	DidCharacteristicWrite CharacteristicWrite;
	// DidDescriptorWrite DescriptorWrite;

	// DidPConnectionStateChange PConnectionStateChange;
	// DidPCharacteristicRead PCharacteristicRead;
	// DidPNotificationSent PNotificationSent;
} BleDid;
static BleDid bledid = {0};
static DidCharacteristicRead bleresp;

static tdevice_data device_data;

//
// rdpd device data
//
tservice_desc rdpd_services[] = {
	{"00001801-0000-1000-8000-00805f9b34fb", // serivce uuid
		{
			{"00002a05-0000-1000-8000-00805f9b34fb", PROPERTY_INDICATE},
			{"00002b3a-0000-1000-8000-00805f9b34fb", PROPERTY_READ},
			{"00002b29-0000-1000-8000-00805f9b34fb", PROPERTY_READ | PROPERTY_WRITE},
			{"00002b2a-0000-1000-8000-00805f9b34fb", PROPERTY_READ}
		}
	},
	{"00001800-0000-1000-8000-00805f9b34fb",  // serivce uuid
		{
			{"00002a00-0000-1000-8000-00805f9b34fb", PROPERTY_READ},
			{"00002a01-0000-1000-8000-00805f9b34fb", PROPERTY_READ},
			{"00002aa6-0000-1000-8000-00805f9b34fb", PROPERTY_READ}
		}
	},
	{"00005356-0000-1000-8000-00805f9b34fb",  // serivce uuid
		{
			{"0000fd02-0000-1000-8000-00805f9b34fb", PROPERTY_READ},
			{"0000fd03-0000-1000-8000-00805f9b34fb", PROPERTY_NOTIFY},
			{"0000fd01-0000-1000-8000-00805f9b34fb", PROPERTY_READ | PROPERTY_WRITE | PROPERTY_NOTIFY}
		}
	}
};

enum {msg_queryip_req = 1, 
	msg_updateip_req,
	msg_connectwifi_req,
	msg_removewifi_req,
	msg_wifilist_req,

	msg_queryip_resp = 50,
	msg_wifilist_resp,
	msg_error_resp
};

void rdpd_did_characteristicwrite()
{
	DidCharacteristicWrite* write = &bledid.CharacteristicWrite;

	// new a did_characteristicread ble
	if (write->data[1] == msg_queryip_req) {
		SDL_Log("WIN_PumpBleDid, receive msg_queryip_req");
		bleresp.type = did_characteristicread;
		DidCharacteristicRead* read = &bleresp;

		SDL_strlcpy(read->service_uuid, write->service_uuid, sizeof(read->service_uuid));
		SDL_strlcpy(read->chara_uuid, "0000fd03-0000-1000-8000-00805f9b34fb", sizeof(read->chara_uuid));
		read->len = 17; // 192.168.1.115
		uint8_t data[17] = {0x5a, 0x32, 0x0d, 0x00,
			0x00, 0x01, 0x06, 0x02,
			0xc0, 0xa8, 0x01, 0x73,
			0x01, 0x01, 0x00, 0x00, 00};
		SDL_memcpy(read->data, data, read->len);
		read->notify = SDL_TRUE;

	} else if (write->data[1] == msg_updateip_req) {
		SDL_Log("WIN_PumpBleDid, receive msg_updateip_req, do nothing");

	} else if (write->data[1] == msg_wifilist_req) {
		SDL_Log("WIN_PumpBleDid, receive msg_wifilist_req, do nothing");

	} else if (write->data[1] == msg_connectwifi_req) {
		SDL_Log("WIN_PumpBleDid, receive msg_connectwifi_req, do nothing");

	} else if (write->data[1] == msg_removewifi_req) {
		SDL_Log("WIN_PumpBleDid, receive msg_removewifi_req, do nothing");
	}
}

//
// lamp device data
//
tservice_desc lamp_services[] = {
	{"00001800-0000-1000-8000-00805f9b34fb", // serivce uuid
	},
	{"fa879af4-d601-420c-b2b4-07ffb528dde3",  // serivce uuid
		{
			{"10e2fde2-d7fe-4845-b3f3-a32010ebb095", PROPERTY_NOTIFY},
			{"b02eaeaa-f6bc-4a7e-bc94-f7b7fc8ded0b", PROPERTY_WRITE}
		}
	},
	{"0000ae00-0000-1000-8000-00805f9b34fb",  // serivce uuid
	}
};

void lamp_did_characteristicwrite()
{
	DidCharacteristicWrite* write = &bledid.CharacteristicWrite;

	// new a did_characteristicread ble
	uint8_t initial_req[] = {0xfe, 0x01, 0x00, 0x02, 0x30, 0x04};
	uint8_t turnon_req[] = {0xfe, 0x01, 0x00, 0x03, 0x00, 0x01, 0x01};
	uint8_t turnoff_req[] = {0xfe, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00};

	uint16_t color = 0xffff;
	if (write->len == sizeof(initial_req) && SDL_memcmp(initial_req, write->data, write->len) == 0) {
		color = 0xd007;

	} else if (write->len == sizeof(turnon_req) && SDL_memcmp(turnon_req, write->data, write->len) == 0) {
		color = 0xb80b;

	} else if (write->len == sizeof(turnoff_req) && SDL_memcmp(turnoff_req, write->data, write->len) == 0) {
		color = 0;
	}

	if (color != 0xffff) {
		bleresp.type = did_characteristicread;
		DidCharacteristicRead* read = &bleresp;
		SDL_strlcpy(read->service_uuid, write->service_uuid, sizeof(read->service_uuid));
		SDL_strlcpy(read->chara_uuid, "10e2fde2-d7fe-4845-b3f3-a32010ebb095", sizeof(read->chara_uuid));
		uint8_t data[] = {0xfe, 0x01, 0x00, 0x22, 0x30, 0x05, 0x07, 0x04, 
			0x10, 0x02, 0x14, 0x05, 0x30, 0x60, 0x00, 0x00, 
			0x06, 0x30, 0x03, 0x00};
		data[14] = (uint8_t)(color & 0xff);
		data[15] = (uint8_t)((color >> 8) & 0xff);
		read->len = sizeof(data);
		SDL_memcpy(read->data, data, read->len);
		read->notify = SDL_TRUE;
	}
}

static void fill_device_data()
{
	SDL_memset(&device_data, 0, sizeof(device_data));
	tservice_desc* services = NULL;
	int service_count = 0;

	if (device_type == device_rdpd) {
		SDL_strlcpy(device_data.address, "DC:52:85:8A:E7:67", sizeof(device_data.address));
		SDL_strlcpy(device_data.name, "fake-win10", sizeof(device_data.name));
		SDL_strlcpy(device_data.service_uuid, "5356", sizeof(device_data.service_uuid));
		// SDL_strlcpy(device_data.service_uuid, "5357", sizeof(device_data.service_uuid));
		uint8_t manufacturer_data[] = {0xf0, 0xff, 0x6b, 0x6f, 0x73, 0x2d, 0x64, 0x65, 0x76, 0x69, 0x63, 0x65};
		device_data.manufacturer_data_len = sizeof(manufacturer_data) / sizeof(manufacturer_data[0]);
		SDL_memcpy(device_data.manufacturer_data, manufacturer_data, device_data.manufacturer_data_len);

		services = rdpd_services;
		service_count = sizeof(rdpd_services) / sizeof(rdpd_services[0]);

		device_data.fn_did_characteristicwrite = rdpd_did_characteristicwrite;

	} else if (device_type == device_lamp) {
		SDL_strlcpy(device_data.address, "36:E8:07:C6:8D:BA", sizeof(device_data.address));
		SDL_strlcpy(device_data.name, "JNS001", sizeof(device_data.name));
		SDL_strlcpy(device_data.service_uuid, "180d", sizeof(device_data.service_uuid));
		// ID: 1015 =>
		uint8_t manufacturer_data[] = {0x49, 0x65, 0x41, 0xd4, 0x8f, 0x2b, 0xb2, 0x79, 0x27, 0x4a, 0x4e, 0x53, 0x30, 0x30, 0x31};
		// ID: 6282 =>
		// uint8_t manufacturer_data[] = {0x49, 0x65, 0x41, 0x36, 0xe8, 0x07, 0xc6, 0x8d, 0xba, 0x4a, 0x4e, 0x53, 0x30, 0x30, 0x31};
		device_data.manufacturer_data_len = sizeof(manufacturer_data) / sizeof(manufacturer_data[0]);
		SDL_memcpy(device_data.manufacturer_data, manufacturer_data, device_data.manufacturer_data_len);

		services = lamp_services;
		service_count = sizeof(lamp_services) / sizeof(lamp_services[0]);

		device_data.fn_did_characteristicwrite = lamp_did_characteristicwrite;

	} else {
		SDL_assert(SDL_FALSE);

	}

	SDL_memcpy(device_data.services, services, service_count * sizeof(tservice_desc));
}


void WIN_PumpBleDid()
{
	uint32_t type = bledid.type;
	bledid.type = 0;

	if (type == did_characteristicread) {
		SDL_assert(SDL_FALSE);
		if (current_callbacks && current_callbacks->read_characteristic) {
			SDL_BlePeripheral* peripheral = connected_peripheral;
			DidCharacteristicRead* read = &bledid.CharacteristicRead;
			SDL_assert(peripheral != NULL && SDL_strlen(read->service_uuid) > 0 && SDL_strlen(read->chara_uuid) > 0);
			current_callbacks->read_characteristic(peripheral, find_characteristic_from_uuid(peripheral, read->service_uuid, read->chara_uuid), 
				read->data, read->len);
		}

	} else if (type == did_characteristicwrite) {
		if (current_callbacks && current_callbacks->write_characteristic) {
			SDL_BlePeripheral* peripheral = connected_peripheral;
			DidCharacteristicWrite* write = &bledid.CharacteristicWrite;
			SDL_assert(peripheral != NULL && SDL_strlen(write->service_uuid) > 0 && SDL_strlen(write->chara_uuid) > 0);
			current_callbacks->write_characteristic(peripheral, find_characteristic_from_uuid(peripheral, write->service_uuid, write->chara_uuid), bledid.CharacteristicWrite.success? 0: -1 * EFAULT);

			if (device_data.fn_did_characteristicwrite) {
				bleresp.type = 0;
				device_data.fn_did_characteristicwrite();

				if (bleresp.type == did_characteristicread && current_callbacks && current_callbacks->read_characteristic) {
					SDL_BlePeripheral* peripheral = connected_peripheral;
					DidCharacteristicRead* read = &bleresp;
					SDL_assert(peripheral != NULL && SDL_strlen(read->service_uuid) > 0 && SDL_strlen(read->chara_uuid) > 0);
					current_callbacks->read_characteristic(peripheral, find_characteristic_from_uuid(peripheral, read->service_uuid, read->chara_uuid), 
						read->data, read->len);
				}
			}
		}
	}

	if (bledid.type == 0) {
		SDL_memset(&bledid, 0, sizeof(bledid));
	}
}

void WIN_ScanPeripherals(const char* uuid)
{
	const char* address = device_data.address;
	const char* name = device_data.name;
	const char* service_uuid = device_data.service_uuid;
	if (address != NULL && name != NULL) {
		int rssi = -1;
		uint8_t uc6[SDL_BLE_MAC_ADDR_BYTES];
		mac_addr_str_2_uc6(address, uc6, ':');
		SDL_BlePeripheral* peripheral = discover_peripheral_uh_macaddr(uc6, name);
		if (!mac_addr_valid(peripheral->mac_addr)) {
			// uuid
			int len = (int)SDL_strlen(service_uuid);
			peripheral->uuid = (char*)SDL_malloc(len + 1);
			SDL_memcpy(peripheral->uuid, service_uuid, len);
			peripheral->uuid[len] = '\0';
			// mac address
			mac_addr_str_2_uc6(address, peripheral->mac_addr, ':');

			// manufacturer_data
			len = device_data.manufacturer_data_len;
			peripheral->manufacturer_data = (uint8_t*)SDL_malloc(len);
			SDL_memcpy(peripheral->manufacturer_data, device_data.manufacturer_data, len);
			peripheral->manufacturer_data_len = len;

			// context
			peripheral_context* context = SDL_malloc(sizeof(peripheral_context));
			context->data1 = 0;
			context->data2 = 0;
			peripheral->cookie = context;
		}
		discover_peripheral_bh(peripheral, SDL_BleDeviceTypeLE, rssi);
	}
}

static void WIN_ConnectPeripheral(SDL_BlePeripheral* peripheral)
{
	int error = 0; // -1 * EFAULT
	// int error = -1 * EFAULT;
	connect_peripheral_bh(peripheral, error);
}

static void WIN_DisconnectPeripheral(SDL_BlePeripheral* peripheral)
{
	SDL_memset(&bledid, 0, sizeof(bledid));

	disconnect_peripheral_bh(peripheral, -1 * EFAULT);
}

static void WIN_GetServices(SDL_BlePeripheral* peripheral)
{
	int count = 0;
	for (; count < MAX_SERVICES; count ++) {
		if (device_data.services[count].uuid[0] == '\0') {
			break;
		}
	}

	discover_services_uh(peripheral, count);
	for (int at = 0; at < count; at ++) {
		SDL_BleService* ble_service = peripheral->services + at;
		discover_services_bh(peripheral, ble_service, device_data.services[at].uuid, NULL);

		//
		// discover characteristics of this service
		//
		int count2 = 0;
		const tchar_desc* char_descs = device_data.services[at].chars;
		for (; count2 < MAX_CHARS_PER_SERVICE; count2 ++) {
			if (char_descs[count2].uuid[0] == '\0') {
				break;
			}
		}

		ble_service = discover_characteristics_uh(peripheral, peripheral->services + at, count2);
		for (int at2 = 0; at2 < count2; at2 ++) {
			const tchar_desc* char_desc = char_descs + at2;

			SDL_BleCharacteristic* ble_characteristic = ble_service->characteristics + at2;
			discover_characteristics_bh(peripheral, ble_service, ble_characteristic, char_desc->uuid, NULL);

			ble_characteristic->properties = char_desc->properties;
		}
	}

	if (current_callbacks && current_callbacks->discover_services) {
		current_callbacks->discover_services(peripheral, 0);
	}

	return;
}

static void WIN_GetCharacteristics(const SDL_BlePeripheral* peripheral, SDL_BleService* service)
{
	if (peripheral != connected_peripheral) {
		return;
	}
	if (current_callbacks && current_callbacks->discover_characteristics) {
		current_callbacks->discover_characteristics(connected_peripheral, service, 0);
	}
}

static void WIN_ReadCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic)
{
	SDL_assert(SDL_FALSE);
}


static void WIN_NotifyCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, SDL_bool enable)
{
	peripheral_context* context = peripheral->cookie;

	// this is notify
	if (current_callbacks && current_callbacks->notify_characteristic) {
		current_callbacks->notify_characteristic(peripheral, find_characteristic_from_uuid(peripheral, characteristic->service->uuid, characteristic->uuid), 0);
	}

	if (context == NULL) {
		return;
	}
	return;
}

static void WIN_WriteCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const unsigned char* data, int size)
{
	peripheral_context* context = peripheral->cookie;

	SDL_assert(bledid.type == 0);
	SDL_assert(SDL_strlen(characteristic->service->uuid) > 0 && SDL_strlen(characteristic->service->uuid) <= FULL_UUID_SIZE);
	SDL_assert(SDL_strlen(characteristic->uuid) > 0 && SDL_strlen(characteristic->uuid) <= FULL_UUID_SIZE);
	SDL_assert(size <= MAX_BLEDID_DATA);
	SDL_memset(&bledid, 0, sizeof(bledid));

	bledid.type = did_characteristicwrite;
	SDL_strlcpy(bledid.CharacteristicWrite.service_uuid, characteristic->service->uuid, sizeof(bledid.CharacteristicWrite.service_uuid));
	SDL_strlcpy(bledid.CharacteristicWrite.chara_uuid, characteristic->uuid, sizeof(bledid.CharacteristicWrite.chara_uuid));
	bledid.CharacteristicWrite.len = size;
	SDL_memcpy(bledid.CharacteristicWrite.data, data, size);
	bledid.CharacteristicWrite.success = SDL_TRUE;

	if (context == NULL) {
		return;
	}
}

static void WIN_ReleaseCharacteristicCookie(const SDL_BleCharacteristic* characteristic, int at)
{
	if (at == 0) {
		SDL_free(characteristic->cookie);
	}
}

static void WIN_ReleaseServiceCookie(const SDL_BleService* service, int at)
{
	if (at == 0) {
		SDL_free(service->cookie);
	}
}

static void WIN_ReleaseCookie(const SDL_BlePeripheral* peripheral)
{
	peripheral_context* context = peripheral->cookie;
	SDL_free(context);
}

static SDL_bool is_advertising = SDL_FALSE;
static void WIN_PStartAdvertising(const char* service_uuid16, const char* name, int manufacturer_id, const uint8_t* manufacturer_data, int manufacturer_data_size)
{
	if (is_advertising) {
		return;
	}
	is_advertising = SDL_TRUE;
	uint8_t mac_addr[SDL_BLE_MAC_ADDR_BYTES] = {0x5a, 0x10, 0x20, 0x30, 0x40, 0x56};
	pconnect_center_bh(mac_addr, 0);
	if (current_pcallbacks && current_pcallbacks->read_characteristic) {
		uint8_t data[4];
		data[0] = 0x5a;
		data[1] = 0x12;
		data[2] = 0x34;
		data[3] = 0x56;
		current_pcallbacks->read_characteristic("fd01", data, sizeof(data));
	}
}

static void WIN_PStopAdvertising(void)
{
	if (!is_advertising) {
		return;
	}
	is_advertising = SDL_FALSE;
	pdisconnect_center_bh();
}

static SDL_bool WIN_PIsAdvertising(void)
{
	return is_advertising;
}

static void WIN_PWriteCharacteristic(const char* uuid, const uint8_t* data, int size)
{
	SDL_Log("WIN_PWriteCharacteristic--- uuid: %s, size: %i", uuid, size);
}

// Windows driver bootstrap functions
static int WIN_Available(void)
{
    return (1);
}

static SDL_MiniBle* WIN_CreateBle(void)
{
    SDL_MiniBle *ble;

    // Initialize all variables that we clean on shutdown
    ble = (SDL_MiniBle *)SDL_calloc(1, sizeof(SDL_MiniBle));
    if (!ble) {
        SDL_OutOfMemory();
        return (0);
    }

	fill_device_data();

    // Set the function pointers
	ble->ScanPeripherals = WIN_ScanPeripherals;
	ble->ConnectPeripheral = WIN_ConnectPeripheral;
	ble->DisconnectPeripheral = WIN_DisconnectPeripheral;
	ble->GetServices = WIN_GetServices;
	ble->GetCharacteristics = WIN_GetCharacteristics;
	ble->ReadCharacteristic = WIN_ReadCharacteristic;
	ble->NotifyCharacteristic = WIN_NotifyCharacteristic;
	ble->WriteCharacteristic = WIN_WriteCharacteristic;
	ble->ReleaseServiceCookie = WIN_ReleaseServiceCookie;
	ble->ReleaseCharacteristicCookie = WIN_ReleaseCharacteristicCookie;
	ble->ReleaseCookie = WIN_ReleaseCookie;
	// ble->Quit = WIN_Quit;

	ble->PStartAdvertising = WIN_PStartAdvertising;
	ble->PStopAdvertising = WIN_PStopAdvertising;
	ble->PIsAdvertising = WIN_PIsAdvertising;
	ble->PWriteCharacteristic = WIN_PWriteCharacteristic;
    return ble;
}


BleBootStrap WINDOWS_ble = {
    WIN_Available, WIN_CreateBle
};

// #endif
